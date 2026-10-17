global long_mode_start
extern entrypoint
extern mb_info_ptr

section .text
bits 64
long_mode_start:
    ; Enable FPU / MMX / SSE / SSE2
    mov rax, cr0
    and rax, ~(1 << 2) ; Clear EM bit
    or rax, (1 << 1)   ; Set MP bit
    or rax, (1 << 5)   ; Set NE bit
    mov cr0, rax

    mov rax, cr4
    or rax, (1 << 9)   ; Set OSFXSR bit
    or rax, (1 << 10)  ; Set OSXMMEXCPT bit
    mov cr4, rax

    fninit ; Initialize FPU

    
    ; load null into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov edi, [mb_info_ptr]
    xor rsi, rsi
	call entrypoint
    hlt