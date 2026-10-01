extern idt_handler_exception

global idt_handler_exc0_wrapped
global idt_handler_exc6_wrapped
global idt_handler_exc8_wrapped
global idt_handler_exc13_wrapped
global idt_handler_exc14_wrapped

section .text

; These exist purely to turn a previously-silent triple fault (no
; handler at all existed for CPU exception vectors 0-31) into an
; actual diagnostic panic. They never return -- idt_handler_exception
; always panics -- so unlike idt_.asm's IRQ wrappers, there's no
; register save/restore or iretq here, just enough to hand the C side
; (vector, error_code, RIP) and call it.
;
; Some vectors push a hardware error code onto the stack before RIP,
; some don't (see the Intel SDM's exception table) -- the two stub
; flavors below account for that so `mov rdx, [rsp+16]` below always
; lands on RIP regardless of which vector got hit.

%macro EXC_STUB_NO_ERR 1
idt_handler_exc%1_wrapped:
	push qword 0  ; synthetic error code, so the stack layout matches
	              ; the "has an error code" stubs below
	push qword %1 ; vector number
	jmp exception_common
%endmacro

%macro EXC_STUB_HAS_ERR 1
idt_handler_exc%1_wrapped:
	; CPU already pushed a real error code before RIP.
	push qword %1 ; vector number
	jmp exception_common
%endmacro

EXC_STUB_NO_ERR 0  ; #DE
EXC_STUB_NO_ERR 6  ; #UD
EXC_STUB_HAS_ERR 8  ; #DF
EXC_STUB_HAS_ERR 13 ; #GP
EXC_STUB_HAS_ERR 14 ; #PF

exception_common:
	; Stack right now (low to high address): [vector][error_code][RIP][CS][RFLAGS][RSP][SS]
	mov rdi, [rsp]      ; vector
	mov rsi, [rsp + 8]  ; error_code
	mov rdx, [rsp + 16] ; RIP, pushed by the CPU as part of its own exception frame

	call idt_handler_exception

	; idt_handler_exception never returns, but just in case:
.hang:
	cli
	hlt
	jmp .hang
