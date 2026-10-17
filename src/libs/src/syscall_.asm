extern idt_handler_syscall
extern thread_current_fpu_state
global syscall_entry

; Must match GDT_SELECTOR_CS_KERNEL / GDT_SELECTOR_SS_KERNEL (gdt.h).
%define KERNEL_CS 0x08
; KERNEL_SS (0x10, the kernel data segment -- see gdt.h/main.asm) is
; still required for the IA32_STAR setup (syscall_fast_init() in
; syscall.c): on `syscall` *entry*, the CPU forces SS to STAR[47:32]+8
; without validating the GDT contents at all, so that offset just
; needs to exist. It is deliberately NOT used below when *returning*
; via iretq -- iretq performs full descriptor validation on a non-null
; SS, unlike entry, and nothing else in this kernel has ever loaded a
; real SS selector (main.asm never sets one -- every existing
; interrupt return has always resumed with a null SS). Loading SS with
; a null selector is specifically permitted when the target CPL is 0,
; which is always true here, so returning with SS=0 matches what
; every other return path in this kernel already does successfully.

section .text

; Entry point for the `syscall` instruction (IA32_LSTAR), configured
; by syscall_fast_init(). This is a second, faster path into the same
; dispatcher the int 0x80 path uses (idt_.asm) -- not a replacement
; for it.
;
; On entry here, the CPU has already (per the `syscall` definition):
;   - saved the return RIP in rcx
;   - saved the pre-call RFLAGS in r11, then cleared the FMASK bits
;     (currently just IF) from the live RFLAGS
;   - loaded CS/SS from IA32_STAR (kernel values -- see
;     syscall_fast_init())
;   - left RSP untouched: unlike an IDT interrupt gate, `syscall`
;     does not push anything or switch stacks. We're still running on
;     the caller's stack.
;
; We deliberately return via iretq rather than sysret: sysret
; unconditionally forces CPL=3 on return, which this kernel (single
; ring, no usermode GDT/TSS/user-accessible paging) can't survive. So
; instead we hand-build a normal ring-0 iretq frame using the rcx/r11
; the CPU handed us, exactly mirroring where the int 0x80 path ends up
; before its own iretq.
syscall_entry:
	push rcx ; return RIP -- needed for the iretq frame below
	push r11 ; saved RFLAGS -- needed for the iretq frame below

	; save the rest of the general-purpose registers
	push rax
	push rbx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r12
	push r13
	push r14
	push r15

	cld

	; Fast-path syscall convention (see x86_64/syscall.h): rdi=number,
	; rsi=arg1, rdx=arg2, r10=arg3 -- r10 instead of rcx, since rcx is
	; clobbered by `syscall` itself (same reason the real x86-64 Linux
	; ABI uses r10 here). idt_handler_syscall(number, arg1, arg2, arg3)
	; expects its 4th argument in rcx per SysV, so move it over right
	; before the call.
	mov rcx, r10

	call idt_handler_syscall

	; restore general-purpose registers
	pop r15
	pop r14
	pop r13
	pop r12
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rbx
	pop rax

	pop r11 ; saved RFLAGS
	pop rcx ; return RIP

	; Snapshot the stack pointer we should resume at -- i.e. exactly
	; where it is right now, since we've unwound every push above and
	; never switch stacks. Do this before pushing the iretq frame
	; itself, which would otherwise move rsp out from under us.
	mov rax, rsp

	; Build the 5-entry iretq frame. iretq pops RIP first, so it must
	; end up on top of the stack -- pushed last, in this order:
	; SS, RSP, RFLAGS, CS, RIP.
	push qword 0 ; SS = null -- see the comment above KERNEL_CS/KERNEL_SS
	push rax
	push r11
	push qword KERNEL_CS
	push rcx

	iretq
