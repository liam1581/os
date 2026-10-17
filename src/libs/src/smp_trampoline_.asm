global smp_trampoline_start
global smp_trampoline_end

; Must match the TRAMP_* constants in smp.c.
%define TRAMP_BASE      0x8000
%define OFF_CR3         0x08
%define OFF_STACK       0x10
%define OFF_ENTRY       0x18
%define OFF_INDEX       0x20
%define OFF_EFER        0x28
%define OFF_CR4         0x2C

; Resolves a label inside this blob to the address it will have once
; smp.c has copied the blob to TRAMP_BASE. The blob is linked into the
; kernel at its usual high address but *executes* from TRAMP_BASE, so
; every address the trampoline itself uses has to be computed this way
; rather than letting the assembler/linker pick one.
%define AT(label) (TRAMP_BASE + ((label) - smp_trampoline_start))

section .text

; ---------------------------------------------------------------------
; A freshly-started AP begins here, in 16-bit real mode, with CS:IP =
; (TRAMP_BASE >> 4):0 (the SIPI vector is TRAMP_BASE >> 12). Layout:
;
;   0x00  jmp over the data block
;   0x08  data slots the BSP fills in before sending the SIPI (cr3,
;         stack, entry point, cpu index, efer, cr4 -- see OFF_* above)
;   0x40  real-mode code
;   ...   protected-mode code, long-mode code, GDT
;
; Path: real mode -> 32-bit protected mode (own flat GDT) -> enable
; PAE + load the BSP's page tables + set EFER.LME + enable paging ->
; 64-bit long mode -> call smp_ap_entry(cpu_index) on a private stack.
; ---------------------------------------------------------------------
bits 16
smp_trampoline_start:
	jmp short real_entry
	times 0x40 - ($ - smp_trampoline_start) db 0   ; data slots live in here

real_entry:
	cli
	cld
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax

	lgdt [AT(tramp_gdt_ptr)]

	mov eax, cr0
	or eax, 1               ; PE
	mov cr0, eax

	jmp dword 0x18:AT(prot_entry)

; ---------------------------------------------------------------------
bits 32
prot_entry:
	mov ax, 0x20            ; flat 32-bit data segment
	mov ds, ax
	mov es, ax
	mov ss, ax

	mov eax, [TRAMP_BASE + OFF_CR4]
	mov cr4, eax            ; includes PAE, which long mode requires

	mov eax, [TRAMP_BASE + OFF_CR3]
	mov cr3, eax            ; the BSP's page tables (identity-mapped)

	mov ecx, 0xC0000080     ; IA32_EFER
	mov eax, [TRAMP_BASE + OFF_EFER]
	or eax, (1 << 8)        ; LME
	xor edx, edx
	wrmsr

	mov eax, cr0
	or eax, 0x80000001      ; PG | PE
	mov cr0, eax

	jmp 0x08:AT(long_entry) ; far jump into the 64-bit code segment

; ---------------------------------------------------------------------
bits 64
long_entry:
	; Null data selectors, same as the BSP runs with (it never loads
	; real ones; null SS is permitted at CPL 0).
	xor eax, eax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax

	mov rsp, [abs TRAMP_BASE + OFF_STACK]
	mov rdi, [abs TRAMP_BASE + OFF_INDEX]
	mov rax, [abs TRAMP_BASE + OFF_ENTRY]
	xor ebp, ebp

	call rax                ; smp_ap_entry(cpu_index) -- never returns

.hang:
	cli
	hlt
	jmp .hang

; ---------------------------------------------------------------------
; GDT. Selectors 0x08 (64-bit code) and 0x10 (data) deliberately match
; the kernel's own GDT (main.asm), so the shared IDT (whose gates name
; selector 0x08) and syscall_fast_init()'s STAR value work unchanged on
; every CPU. 0x18/0x20 are 32-bit code/data, only needed for the
; real-mode -> long-mode hop. This blob's page stays reserved for the
; life of the kernel, so APs can keep running on this GDT.
; ---------------------------------------------------------------------
tramp_gdt:
	dq 0
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)   ; 0x08: 64-bit code
	dq (1 << 41) | (1 << 44) | (1 << 47)               ; 0x10: data
	dq 0x00CF9A000000FFFF                              ; 0x18: 32-bit code, flat
	dq 0x00CF92000000FFFF                              ; 0x20: 32-bit data, flat
tramp_gdt_end:

tramp_gdt_ptr:
	dw tramp_gdt_end - tramp_gdt - 1
	dd AT(tramp_gdt)

smp_trampoline_end:
