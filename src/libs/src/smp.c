#include "smp.h"

#include "x86_64/msr.h"
#include "x86_64/idt.h"
#include "x86_64/port.h"
#include "x86_64/syscall.h"
#include "mem/kheap.h"

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define MSR_APIC_BASE 0x0000001B
#define MSR_EFER      0xC0000080
#define MSR_GS_BASE   0xC0000101

#define APIC_BASE_ENABLE (1ULL << 11)
#define APIC_BASE_X2APIC (1ULL << 10)

#define LAPIC_TPR       0x080
#define LAPIC_EOI       0x0B0
#define LAPIC_SVR       0x0F0
#define LAPIC_ICR_LOW   0x300
#define LAPIC_ICR_HIGH  0x310
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360

#define LAPIC_SVR_ENABLE  (1u << 8)
#define LAPIC_LVT_MASKED  (1u << 16)
#define LAPIC_ICR_PENDING (1u << 12)

// ICR delivery modes / flags
#define ICR_ASSERT     (1u << 14)
#define ICR_DM_INIT    (5u << 8)
#define ICR_DM_STARTUP (6u << 8)

// Where the AP trampoline gets copied to. Must be below 1MiB, 4KiB
// aligned (the SIPI vector is TRAMP_BASE >> 12), and never handed out
// by the PMM -- pmm_init() reserves all of [0, 1MiB), so it isn't.
// These offsets must match smp_trampoline_.asm exactly.
#define TRAMP_BASE       0x8000
#define TRAMP_OFF_CR3    0x08
#define TRAMP_OFF_STACK  0x10
#define TRAMP_OFF_ENTRY  0x18
#define TRAMP_OFF_INDEX  0x20
#define TRAMP_OFF_EFER   0x28
#define TRAMP_OFF_CR4    0x2C

#define AP_STACK_SIZE (32 * 1024)

enum {
    TASK_IDLE = 0,
    TASK_CLAIMED,  // a dispatcher reserved the slot and is filling it in
    TASK_PENDING,  // fn/arg are valid, the AP hasn't picked it up yet
    TASK_RUNNING,
};

typedef struct cpu_info {
    struct cpu_info* self; // must stay first: gs:[0] reads it back
    uint32_t index;
    uint32_t apic_id;
    volatile uint32_t online;
    volatile uint32_t task_state;
    void (*task_fn)(void*);
    void* task_arg;
    uint8_t* stack;
} cpu_info_t;

static cpu_info_t cpus[SMP_MAX_CPUS];
static int detected_count = 1;
static int online_count = 1;
static bool smp_ready = false;

static volatile uint32_t* lapic = NULL;

extern uint8_t smp_trampoline_start[];
extern uint8_t smp_trampoline_end[];

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static inline void cpuid(uint32_t leaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    asm volatile ("cpuid" : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d) : "a" (leaf), "c" (0));
}

static uint32_t bsp_apic_id(void) {
    uint32_t a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    return b >> 24;
}

// Crude but dependency-free: each port 0x80 write takes roughly a
// microsecond on real hardware. Only used for the handful of delays the
// INIT/SIPI protocol asks for, where approximately right is fine.
static void delay_us(uint32_t us) {
    for (uint32_t i = 0; i < us; i++)
        port_wait();
}

/* ------------------------------------------------------------------ */
/* Local APIC                                                          */
/* ------------------------------------------------------------------ */

static inline uint32_t lapic_read(uint32_t reg) {
    return lapic[reg / 4];
}

static inline void lapic_write(uint32_t reg, uint32_t value) {
    lapic[reg / 4] = value;
}

// The kernel identity-maps the first 16GiB with plain, cacheable 2MiB
// pages (main.asm). Right for RAM, wrong for MMIO: LAPIC registers
// must be uncached. Set PWT|PCD on the one 2MiB page containing it.
static void map_mmio_uncached(uint64_t phys) {
    uint64_t cr3;
    asm volatile ("mov %%cr3, %0" : "=r" (cr3));

    uint64_t* pml4 = (uint64_t*) (cr3 & 0x000FFFFFFFFFF000ULL);
    uint64_t entry = pml4[(phys >> 39) & 0x1FF];
    if (!(entry & 1)) return;

    uint64_t* pdpt = (uint64_t*) (entry & 0x000FFFFFFFFFF000ULL);
    entry = pdpt[(phys >> 30) & 0x1FF];
    if (!(entry & 1) || (entry & 0x80)) return; // absent, or a 1GiB page

    uint64_t* pd = (uint64_t*) (entry & 0x000FFFFFFFFFF000ULL);
    uint64_t* pde = &pd[(phys >> 21) & 0x1FF];
    if (!(*pde & 1) || !(*pde & 0x80)) return;  // absent, or not a 2MiB page

    *pde |= (1ULL << 3) | (1ULL << 4); // PWT | PCD
    asm volatile ("invlpg (%0)" :: "r" (phys) : "memory");
}

static void lapic_enable_this_cpu(bool is_bsp) {
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | SMP_SPURIOUS_VECTOR);

    if (is_bsp) {
        // The 8259 PIC reaches the CPU through LINT0 in "ExtINT" mode.
        // Keep it unmasked now that the LAPIC is software-enabled, or
        // the timer and keyboard IRQs stop arriving. LINT1 is NMI.
        lapic_write(LAPIC_LVT_LINT0, 0x00000700);
        lapic_write(LAPIC_LVT_LINT1, 0x00000400);
    } else {
        // APs must never see PIC interrupts -- those belong to the BSP.
        lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
        lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    }
}

static void lapic_wait_icr_idle(void) {
    for (int i = 0; i < 1000000; i++) {
        if (!(lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING))
            return;
        asm volatile ("pause");
    }
}

static void lapic_send_ipi(uint32_t apic_id, uint32_t icr_low) {
    // The two ICR writes must not be split by an interrupt that sends
    // an IPI of its own.
    uint64_t flags;
    asm volatile ("pushfq; pop %0; cli" : "=r" (flags) :: "memory");

    lapic_wait_icr_idle();
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, icr_low);

    if (flags & (1u << 9))
        asm volatile ("sti" ::: "memory");
}

void smp_handle_wake_ipi(void) {
    lapic_write(LAPIC_EOI, 0);
}

/* ------------------------------------------------------------------ */
/* ACPI                                                                */
/* ------------------------------------------------------------------ */

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t lapic_address;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

struct madt_lapic_entry {
    uint8_t type;   // 0
    uint8_t length;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags; // bit 0: enabled, bit 1: online-capable
} __attribute__((packed));

static bool sig_equals(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return false;
    return true;
}

static bool rsdp_checksum_ok(const struct acpi_rsdp* rsdp) {
    const uint8_t* p = (const uint8_t*) rsdp;
    uint8_t sum = 0;
    for (int i = 0; i < 20; i++) sum += p[i];
    return sum == 0;
}

// GRUB hands over a copy of the RSDP as a multiboot2 tag (type 14 for
// the ACPI 1.0 one, 15 for 2.0+). Preferred over scanning memory since
// it also works when booted via UEFI.
static const struct acpi_rsdp* find_rsdp_multiboot(uint64_t multiboot_info_addr) {
    if (multiboot_info_addr == 0) return NULL;

    const uint8_t* p = (const uint8_t*) multiboot_info_addr + 8; // skip total_size/reserved

    for (;;) {
        uint32_t type = *(const uint32_t*) p;
        uint32_t size = *(const uint32_t*) (p + 4);

        if (type == 0) break; // end tag

        if (type == 14 || type == 15) {
            const struct acpi_rsdp* rsdp = (const struct acpi_rsdp*) (p + 8);
            if (sig_equals(rsdp->signature, "RSD PTR ", 8)) return rsdp;
        }

        p += (size + 7) & ~7u;
    }

    return NULL;
}

// Legacy BIOS fallback: the RSDP lives on a 16-byte boundary in the
// first KiB of the EBDA, or in the BIOS ROM area 0xE0000-0xFFFFF.
static const struct acpi_rsdp* find_rsdp_scan(void) {
    uint16_t ebda_segment = *(volatile uint16_t*) 0x40E;
    uint64_t ebda = (uint64_t) ebda_segment << 4;

    struct { uint64_t start; uint64_t end; } regions[2] = {
        { ebda, ebda + 1024 },
        { 0xE0000, 0x100000 },
    };

    for (int r = 0; r < 2; r++) {
        if (regions[r].start == 0) continue;

        for (uint64_t addr = regions[r].start; addr + 20 <= regions[r].end; addr += 16) {
            const struct acpi_rsdp* rsdp = (const struct acpi_rsdp*) addr;
            if (sig_equals(rsdp->signature, "RSD PTR ", 8) && rsdp_checksum_ok(rsdp))
                return rsdp;
        }
    }

    return NULL;
}

static const struct acpi_madt* find_madt(const struct acpi_rsdp* rsdp) {
    bool use_xsdt = rsdp->revision >= 2 && rsdp->xsdt_address != 0;

    const struct acpi_sdt_header* root = use_xsdt
        ? (const struct acpi_sdt_header*) rsdp->xsdt_address
        : (const struct acpi_sdt_header*) (uint64_t) rsdp->rsdt_address;

    if (root == NULL) return NULL;

    uint32_t entry_size = use_xsdt ? 8 : 4;
    if (root->length < sizeof(struct acpi_sdt_header)) return NULL;
    uint32_t count = (root->length - sizeof(struct acpi_sdt_header)) / entry_size;

    const uint8_t* entries = (const uint8_t*) root + sizeof(struct acpi_sdt_header);

    for (uint32_t i = 0; i < count; i++) {
        uint64_t address = use_xsdt
            ? *(const uint64_t*) (entries + i * 8)
            : *(const uint32_t*) (entries + i * 4);

        const struct acpi_sdt_header* table = (const struct acpi_sdt_header*) address;
        if (table != NULL && sig_equals(table->signature, "APIC", 4))
            return (const struct acpi_madt*) table;
    }

    return NULL;
}

// Fills cpus[] from the MADT, always placing the CPU we're running on
// at index 0.
static void enumerate_cpus(const struct acpi_madt* madt) {
    uint32_t bsp_id = bsp_apic_id();

    cpus[0].index = 0;
    cpus[0].apic_id = bsp_id;
    detected_count = 1;

    if (madt == NULL) return;

    const uint8_t* p = madt->entries;
    const uint8_t* end = (const uint8_t*) madt + madt->header.length;

    while (p + 2 <= end) {
        uint8_t type = p[0];
        uint8_t length = p[1];
        if (length < 2) break;

        if (type == 0 && length >= sizeof(struct madt_lapic_entry)) {
            const struct madt_lapic_entry* e = (const struct madt_lapic_entry*) p;
            bool usable = (e->flags & 1) || (e->flags & 2);

            if (usable && e->apic_id != bsp_id && detected_count < SMP_MAX_CPUS) {
                cpus[detected_count].index = (uint32_t) detected_count;
                cpus[detected_count].apic_id = e->apic_id;
                detected_count++;
            }
        }

        p += length;
    }
}

/* ------------------------------------------------------------------ */
/* Application processor startup                                       */
/* ------------------------------------------------------------------ */

// Where every AP lands once the trampoline (smp_trampoline_.asm) has
// taken it from real mode to 64-bit long mode on its own stack.
void smp_ap_entry(uint64_t index) {
    cpu_info_t* cpu = &cpus[index];

    // GS base -> this CPU's cpu_info_t, so smp_current_cpu() is a
    // single load instead of a (VM-trapping) CPUID.
    wrmsr(MSR_GS_BASE, (uint64_t) cpu);

    idt_load_ap();
    lapic_enable_this_cpu(false);

    // MSRs are per-core: `syscall` needs its own setup on every CPU.
    syscall_fast_init();

    __atomic_store_n(&cpu->online, 1, __ATOMIC_RELEASE);

    // Park until handed work. Check for work with interrupts off, then
    // `sti; hlt` -- the sti shadow means an IPI that lands in between
    // still wakes the hlt rather than being lost.
    for (;;) {
        asm volatile ("cli");

        if (__atomic_load_n(&cpu->task_state, __ATOMIC_ACQUIRE) == TASK_PENDING) {
            void (*fn)(void*) = cpu->task_fn;
            void* arg = cpu->task_arg;

            __atomic_store_n(&cpu->task_state, TASK_RUNNING, __ATOMIC_RELEASE);

            asm volatile ("sti");
            fn(arg);
            asm volatile ("cli");

            __atomic_store_n(&cpu->task_state, TASK_IDLE, __ATOMIC_RELEASE);
            continue;
        }

        asm volatile ("sti; hlt");
    }
}

static bool start_ap(int index, uint64_t cr3, uint32_t efer, uint32_t cr4) {
    cpu_info_t* cpu = &cpus[index];

    cpu->stack = (uint8_t*) kmalloc(AP_STACK_SIZE);
    if (cpu->stack == NULL) return false;

    uint64_t stack_top = ((uint64_t) cpu->stack + AP_STACK_SIZE) & ~0xFULL;

    volatile uint8_t* t = (volatile uint8_t*) TRAMP_BASE;
    *(volatile uint64_t*) (t + TRAMP_OFF_CR3)   = cr3;
    *(volatile uint64_t*) (t + TRAMP_OFF_STACK) = stack_top;
    *(volatile uint64_t*) (t + TRAMP_OFF_ENTRY) = (uint64_t) smp_ap_entry;
    *(volatile uint64_t*) (t + TRAMP_OFF_INDEX) = (uint64_t) index;
    *(volatile uint32_t*) (t + TRAMP_OFF_EFER)  = efer;
    *(volatile uint32_t*) (t + TRAMP_OFF_CR4)   = cr4;

    // Standard INIT-SIPI-SIPI sequence.
    lapic_send_ipi(cpu->apic_id, ICR_ASSERT | ICR_DM_INIT);
    delay_us(10000);

    for (int sipi = 0; sipi < 2; sipi++) {
        lapic_send_ipi(cpu->apic_id, ICR_ASSERT | ICR_DM_STARTUP | (TRAMP_BASE >> 12));
        delay_us(200);

        if (__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE))
            return true;
    }

    // Generous: ~500ms nominal, since TCG can be slow to bring up a vCPU.
    for (int i = 0; i < 500; i++) {
        if (__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE))
            return true;
        delay_us(1000);
    }

    return false;
}

void smp_init(uint64_t multiboot_info_addr) {
    for (int i = 0; i < SMP_MAX_CPUS; i++)
        cpus[i].self = &cpus[i];

    cpus[0].online = 1;
    cpus[0].task_state = TASK_RUNNING; // the BSP is always "busy": it's running the kernel

    const struct acpi_rsdp* rsdp = find_rsdp_multiboot(multiboot_info_addr);
    if (rsdp == NULL) rsdp = find_rsdp_scan();

    const struct acpi_madt* madt = rsdp ? find_madt(rsdp) : NULL;
    enumerate_cpus(madt);

    // From here on smp_current_cpu() works on the BSP.
    wrmsr(MSR_GS_BASE, (uint64_t) &cpus[0]);
    smp_ready = true;

    if (detected_count <= 1)
        return; // nothing to start

    uint64_t apic_base_msr = rdmsr(MSR_APIC_BASE);

    if (apic_base_msr & APIC_BASE_X2APIC) {
        // The MMIO register window below isn't valid in x2APIC mode.
        // Stay single-CPU rather than guess.
        detected_count = 1;
        return;
    }

    wrmsr(MSR_APIC_BASE, apic_base_msr | APIC_BASE_ENABLE);

    uint64_t lapic_phys = apic_base_msr & 0x000FFFFFFFFFF000ULL;
    map_mmio_uncached(lapic_phys);
    lapic = (volatile uint32_t*) lapic_phys;

    lapic_enable_this_cpu(true);

    // Copy the real-mode trampoline into low memory.
    uint64_t tramp_size = (uint64_t) (smp_trampoline_end - smp_trampoline_start);
    volatile uint8_t* dst = (volatile uint8_t*) TRAMP_BASE;
    for (uint64_t i = 0; i < tramp_size; i++)
        dst[i] = smp_trampoline_start[i];

    uint64_t cr3;
    uint64_t cr4;
    asm volatile ("mov %%cr3, %0" : "=r" (cr3));
    asm volatile ("mov %%cr4, %0" : "=r" (cr4));
    uint64_t efer = rdmsr(MSR_EFER);

    // Hand the APs the same EFER (LMA is read-only status, not
    // something to write back) and CR4 the BSP is running with.
    uint32_t efer_low = (uint32_t) (efer & ~(1ULL << 10));

    // One at a time -- they share a single set of trampoline data
    // slots, so each must be done with them before the next goes.
    for (int i = 1; i < detected_count; i++) {
        if (start_ap(i, cr3, efer_low, (uint32_t) cr4))
            online_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Queries and task dispatch                                           */
/* ------------------------------------------------------------------ */

int smp_current_cpu(void) {
    if (!smp_ready) return 0;

    cpu_info_t* cpu;
    asm volatile ("mov %%gs:0, %0" : "=r" (cpu));
    return (int) cpu->index;
}

int smp_cpu_count(void) {
    return online_count;
}

int smp_cpu_detected_count(void) {
    return detected_count;
}

uint32_t smp_cpu_apic_id(int cpu) {
    if (cpu < 0 || cpu >= detected_count) return 0;
    return cpus[cpu].apic_id;
}

bool smp_cpu_is_online(int cpu) {
    if (cpu < 0 || cpu >= detected_count) return false;
    return __atomic_load_n(&cpus[cpu].online, __ATOMIC_ACQUIRE) != 0;
}

bool smp_cpu_is_idle(int cpu) {
    if (cpu <= 0 || cpu >= detected_count) return false;
    if (!smp_cpu_is_online(cpu)) return false;
    return __atomic_load_n(&cpus[cpu].task_state, __ATOMIC_ACQUIRE) == TASK_IDLE;
}

bool smp_run_on(int cpu, void (*fn)(void*), void* arg) {
    if (!smp_ready || fn == NULL) return false;
    if (cpu <= 0 || cpu >= detected_count) return false; // 0 is the BSP
    if (!smp_cpu_is_online(cpu)) return false;

    cpu_info_t* c = &cpus[cpu];

    // Claim the slot atomically so two dispatchers can't both believe
    // they got an idle CPU.
    uint32_t expected = TASK_IDLE;
    if (!__atomic_compare_exchange_n(&c->task_state, &expected, TASK_CLAIMED,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        return false;

    c->task_fn = fn;
    c->task_arg = arg;
    __atomic_store_n(&c->task_state, TASK_PENDING, __ATOMIC_RELEASE);

    lapic_send_ipi(c->apic_id, ICR_ASSERT | SMP_IPI_WAKE_VECTOR);
    return true;
}

void smp_wait(int cpu) {
    if (cpu <= 0 || cpu >= detected_count) return;
    if (!smp_cpu_is_online(cpu)) return;

    while (__atomic_load_n(&cpus[cpu].task_state, __ATOMIC_ACQUIRE) != TASK_IDLE)
        asm volatile ("pause");
}
