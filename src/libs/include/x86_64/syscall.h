#pragma once

#include <stdint.h>

/*
 * Two ways into the same syscalls, both dispatched through
 * idt_handler_syscall() below:
 *
 *   1. `int 0x80`, an IDT interrupt gate (see idt.c/idt_.asm).
 *
 *   2. `syscall`, the dedicated fast-syscall instruction (see
 *      syscall_.asm / syscall_fast_init()). Faster (no IDT lookup),
 *      but needs its own MSR setup, called once at boot:
 *      syscall_fast_init().
 *
 * The two paths use slightly different registers for the 4th
 * argument -- see the per-path notes below -- because `syscall`
 * itself clobbers rcx. Neither of the two syscalls implemented today
 * uses a 4th argument, so in practice this doesn't matter yet, but
 * keep it in mind if you add one that does.
 *
 * int 0x80 convention:
 *     mov rdi, <syscall number>
 *     mov rsi, <arg1>
 *     mov rdx, <arg2>
 *     mov rcx, <arg3>
 *     int 0x80
 *
 * syscall convention (r10 instead of rcx for arg3 -- same reason the
 * real x86-64 Linux ABI does this: `syscall` overwrites rcx with the
 * return address):
 *     mov rdi, <syscall number>
 *     mov rsi, <arg1>
 *     mov rdx, <arg2>
 *     mov r10, <arg3>
 *     syscall
 *
 * Either way, rdi/rsi/rdx(/rcx or r10) are exactly the SysV C
 * argument registers idt_handler_syscall() expects, so both entry
 * stubs can call straight into it. Every other register is preserved
 * across the call.
 *
 * The kernel currently runs everything at ring 0 (no usermode/ring 3
 * separation exists yet), so both are same-privilege calls for now --
 * but the vector/MSR setup, calling convention and dispatcher are the
 * same shape a real ring 3 -> ring 0 syscall path would use later.
 * Notably, the syscall path returns via iretq rather than sysret:
 * sysret unconditionally forces CPL=3 on return, which this kernel
 * can't yet survive (see syscall_.asm).
 */
#define SYSCALL_VECTOR 0x80

// Args: rsi = character to print (low byte used)
#define SYSCALL_PRINT_CHAR 0x00
// Args: rsi = address of null-terminated string, rdx = length in bytes.
#define SYSCALL_PRINT_STRING 0x01
// Args: rsi = address of null-terminated string, rdx = length in bytes.
#define SYSCALL_PRINTLN 0x02
// Args: rsi = signed integer to print
#define SYSCALL_PRINT_INT 0x03
// Args: rsi = unsigned integer to print as hex (without 0x prefix)
#define SYSCALL_PRINT_UINT_HEX 0x04
// Args: rsi = unsigned integer to print as binary (without 0b prefix)
#define SYSCALL_PRINT_UINT_BIN 0x05
// Args: rsi = unsigned integer to print as decimal
#define SYSCALL_PRINT_UINT_DEC 0x06
// Args: rsi = foreground color, rdx = background color(using 8 bits)
#define SYSCALL_PRINT_SET_COLOR 0x07
// Args: none
#define SYSCALL_CLEAR_SCREEN 0x08

// Args: none
#define SYSCALL_ARCH_RESTART 0x10
// Args: none
#define SYSCALL_ARCH_SHUTDOWN 0x11

// Args: rsi = baudrate
#define SYSCALL_SERIAL_INIT 0x20
// Args: rsi = byte to write
#define SYSCALL_SERIAL_WRITE_BYTE 0x21
// Args: rsi = address of null-terminated string, rdx = length in bytes.
#define SYSCALL_SERIAL_WRITE_STRING 0x22
// Args: rsi = address of null-terminated string, rdx = length in bytes.
#define SYSCALL_SERIAL_WRITELN 0x23
// Args: rsi = signed integer to print
#define SYSCALL_SERIAL_WRITE_INT 0x24
// Args: rsi = unsigned integer to print as hex (without 0x prefix)
#define SYSCALL_SERIAL_WRITE_UINT_HEX 0x25
// Args: rsi = unsigned integer to print as binary (without 0b prefix)
#define SYSCALL_SERIAL_WRITE_UINT_BIN 0x26
// Args: rsi = unsigned integer to print as decimal
#define SYSCALL_SERIAL_WRITE_UINT_DEC 0x27
// Args: none
#define SYSCALL_SERIAL_CLOSE 0x28

// Args: rsi = seconds to delay
#define SYSCALL_DELAY_SECONDS 0x30
// Args: rsi = minutes to delay
#define SYSCALL_DELAY_MINUTES 0x31

// Dispatcher shared by both entry paths (idt_handler_syscall_wrapped
// in idt_.asm, and syscall_entry in syscall_.asm). Not meant to be
// called directly from C.
void idt_handler_syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3);

// Configures IA32_EFER/STAR/LSTAR/FMASK so the `syscall` instruction
// reaches syscall_entry. Call once at boot, after the GDT is set up
// (GDT_SELECTOR_CS_KERNEL/SS_KERNEL in gdt.h must already be valid).
// The int 0x80 path (idt_set_entry in idt_init()) is independent of
// this and needs no separate enable step.
void syscall_fast_init(void);