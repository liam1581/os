#include "pmm.h"
#include "multiboot2.h"
#include "debug.h"

// Upper bound on how much physical RAM the PMM can track. This must not
// exceed the amount of memory identity-mapped by the boot page tables --
// see MAPPED_GIB in src/x86_64/boot/main.asm. Bump both together if more
// RAM needs to be usable.
#define PMM_MAX_MEMORY  (16ULL * 1024 * 1024 * 1024)
#define PMM_MAX_FRAMES  (PMM_MAX_MEMORY / PMM_FRAME_SIZE)
#define PMM_BITMAP_SIZE (PMM_MAX_FRAMES / 8)

// Defined by the linker script -- bounds of the loaded kernel image,
// including .bss (so this bitmap, the boot page tables, and the boot
// stack are all automatically covered by the kernel reservation below).
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

static uint8_t bitmap[PMM_BITMAP_SIZE];
static uint64_t total_frames = 0; // frames within detected RAM (bitmap universe)
static uint64_t used_frames = 0;  // used frames within [0, total_frames)
static uint64_t search_hint = 0;  // first frame that might still be free

static inline void set_used(uint64_t frame) {
    if (frame >= PMM_MAX_FRAMES) return;
    uint8_t mask = (uint8_t)(1u << (frame % 8));
    if (!(bitmap[frame / 8] & mask)) {
        bitmap[frame / 8] |= mask;
        used_frames++;
    }
}

static inline void set_free(uint64_t frame) {
    if (frame >= PMM_MAX_FRAMES) return;
    uint8_t mask = (uint8_t)(1u << (frame % 8));
    if (bitmap[frame / 8] & mask) {
        bitmap[frame / 8] &= (uint8_t)~mask;
        used_frames--;
    }
}

static inline int is_used(uint64_t frame) {
    if (frame >= PMM_MAX_FRAMES) return 1;
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

static void reserve_range(uint64_t addr, uint64_t len) {
    uint64_t start = addr / PMM_FRAME_SIZE;
    uint64_t end = (addr + len + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    for (uint64_t f = start; f < end; f++) {
        set_used(f);
    }
}

void pmm_init(uint64_t multiboot_info_addr) {
    // Nothing usable until proven otherwise.
    for (uint64_t i = 0; i < PMM_BITMAP_SIZE; i++) bitmap[i] = 0xFF;
    total_frames = 0;
    used_frames = 0;
    search_hint = 0;

    struct multiboot_tag_mmap* mmap = multiboot2_find_mmap(multiboot_info_addr);
    if (!mmap) {
        DBG_PRINTLNS("PMM: no multiboot memory map tag found, 0 bytes usable");
        return;
    }

    uint8_t* mmap_end = (uint8_t*)mmap + mmap->size;

    // Pass 1: find the highest available address, to size total_frames.
    uint64_t highest_addr = 0;
    for (uint8_t* p = (uint8_t*)mmap->entries; p < mmap_end; p += mmap->entry_size) {
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)p;
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t top = entry->addr + entry->len;
            if (top > highest_addr) highest_addr = top;
        }
    }

    total_frames = highest_addr / PMM_FRAME_SIZE;
    if (total_frames > PMM_MAX_FRAMES) total_frames = PMM_MAX_FRAMES;
    used_frames = total_frames; // everything reserved until freed below

    // Pass 2: free whatever the memory map actually reports as available.
    for (uint8_t* p = (uint8_t*)mmap->entries; p < mmap_end; p += mmap->entry_size) {
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)p;
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

        uint64_t start = entry->addr / PMM_FRAME_SIZE;
        uint64_t end = (entry->addr + entry->len) / PMM_FRAME_SIZE;
        if (end > total_frames) end = total_frames;

        for (uint64_t f = start; f < end; f++) set_free(f);
    }

    // Re-reserve memory that's technically "available" per the map but is
    // actually in use: the low 1MiB (real-mode IVT/BDA/VGA/BIOS areas), the
    // kernel image itself (which also covers this bitmap, the boot page
    // tables, and the boot stack, all living in .bss), and the multiboot
    // info structure GRUB gave us.
    reserve_range(0, 0x100000);
    reserve_range((uint64_t)(uintptr_t)_kernel_start,
                  (uint64_t)(uintptr_t)_kernel_end - (uint64_t)(uintptr_t)_kernel_start);

    uint32_t mb_size = *(uint32_t*)(uintptr_t)multiboot_info_addr;
    reserve_range(multiboot_info_addr, mb_size);
}

uint64_t pmm_alloc_frame(void) {
    for (uint64_t f = search_hint; f < total_frames; f++) {
        if (!is_used(f)) {
            set_used(f);
            search_hint = f + 1;
            return f * PMM_FRAME_SIZE;
        }
    }
    // Wrap around once, in case frames were freed behind the hint.
    for (uint64_t f = 0; f < search_hint && f < total_frames; f++) {
        if (!is_used(f)) {
            set_used(f);
            search_hint = f + 1;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0; // out of memory
}

void pmm_free_frame(uint64_t phys_addr) {
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame < search_hint) search_hint = frame;
    set_free(frame);
}

uint64_t pmm_total_memory_bytes(void) {
    return total_frames * PMM_FRAME_SIZE;
}

uint64_t pmm_free_memory_bytes(void) {
    return (total_frames - used_frames) * PMM_FRAME_SIZE;
}
