#include <stddef.h>
#include <stdint.h>
#include "smp.h"
#include "log.h"
#include "x86_64/gdt.h"
#include "x86_64/idt.h"
#include "x86_64/pic.h"
#include "x86_64/port.h"
#include "x86_64/syscall.h"

#define PORT_PS2_DATA 	  0x60

#define IDT_IRQ0_TIMER    0x20
#define IDT_IRQ1_KEYBOARD 0x21
#define IDT_IRQ12_MOUSE	  0x2C

#define IDT_GATE_PRESENT (1 << 7)
#define IDT_GATE_DPL0 (0b00 << 5)
#define IDT_GATE_DPL1 (0b01 << 5)
#define IDT_GATE_DPL2 (0b10 << 5)
#define IDT_GATE_DPL3 (0b11 << 5)
#define IDT_GATE_TYPE_INTERRUPT 0xE

#define IDT_ENTRY_TYPE_INTERRUPT (IDT_GATE_PRESENT | IDT_GATE_DPL0 | IDT_GATE_TYPE_INTERRUPT)
#define IDT_ENTRY_TYPE_SYSCALL (IDT_GATE_PRESENT | IDT_GATE_DPL3 | IDT_GATE_TYPE_INTERRUPT)

struct IdtEntry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t  ist;
	uint8_t  type;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed));

struct IdtPtr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct IdtEntry idt[256] __attribute__((aligned(16)));
struct IdtPtr idt_ptr;

void (*idt_handler_keyboard_user)();
void (*idt_handler_timer_user)();
void (*idt_handler_mouse_user)();

extern void idt_load(struct IdtPtr* idt_ptr);

void idt_set_entry(uint8_t vector, uint64_t isr_addr, uint16_t selector, uint8_t type) {
	idt[vector] = (struct IdtEntry) {
		.offset_low = (uint16_t) (isr_addr >> 0),
		.selector = selector,
		.ist = 0,
		.type = type,
		.offset_mid = (uint16_t) (isr_addr >> 16),
		.offset_high = (uint32_t) (isr_addr >> 32),
		.reserved = 0,
	};
}

extern void idt_handler_keyboard_wrapped();
extern void idt_handler_timer_wrapped();
extern void idt_handler_syscall_wrapped();
extern void idt_handler_mouse_wrapped();
extern void idt_handler_ipi_wake_wrapped();
extern void idt_handler_spurious_wrapped();

extern void idt_handler_exc0_wrapped();
extern void idt_handler_exc6_wrapped();
extern void idt_handler_exc8_wrapped();
extern void idt_handler_exc13_wrapped();
extern void idt_handler_exc14_wrapped();

void idt_handler_keyboard() {
	if (idt_handler_keyboard_user != NULL) {
		idt_handler_keyboard_user();
	} else {
		port_inb(PORT_PS2_DATA);
	}
	
	pic_eoi_master();
}

void idt_handler_timer() {
	if (idt_handler_timer_user != NULL) {
		idt_handler_timer_user();
	}

	pic_eoi_master();
}

void idt_handler_ipi_wake() {
	smp_handle_wake_ipi();
}
void idt_handler_spurious() {
	INF_PRINT("spurious interrupt");
}

void idt_handler_mouse() {
	if (idt_handler_mouse_user != NULL) {
		idt_handler_mouse_user();
	} else {
		port_inb(PORT_PS2_DATA);
	}

	pic_eoi_slave();
	pic_eoi_master();
}

void idt_init() {
	pic_remap();
	
	idt_ptr.limit = (sizeof(struct IdtEntry) * 256) - 1;
	idt_ptr.base = (uint64_t) &idt;
	
	idt_set_entry(IDT_IRQ0_TIMER, (uint64_t) idt_handler_timer_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(IDT_IRQ1_KEYBOARD, (uint64_t) idt_handler_keyboard_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(SYSCALL_VECTOR, (uint64_t) idt_handler_syscall_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_SYSCALL);
	idt_set_entry(SMP_IPI_WAKE_VECTOR, (uint64_t) idt_handler_ipi_wake_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(SMP_SPURIOUS_VECTOR, (uint64_t) idt_handler_spurious_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(IDT_IRQ12_MOUSE, (uint64_t) idt_handler_mouse_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);

	idt_set_entry(0,  (uint64_t) idt_handler_exc0_wrapped,  GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(6,  (uint64_t) idt_handler_exc6_wrapped,  GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(8,  (uint64_t) idt_handler_exc8_wrapped,  GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(13, (uint64_t) idt_handler_exc13_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	idt_set_entry(14, (uint64_t) idt_handler_exc14_wrapped, GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	
	idt_load(&idt_ptr);
	
	asm volatile("sti");
}

void idt_set_handler_keyboard(void (*handler)()) {
	idt_handler_keyboard_user = handler;
}

void idt_set_handler_timer(void (*handler)()) {
	idt_handler_timer_user = handler;
}

void idt_set_handler_mouse(void (*handler)()) {
	idt_handler_mouse_user = handler;
}

void idt_load_ap(void) {
	idt_load(&idt_ptr);
}