global long_mode_start
extern kernel_main
extern mb_info_ptr

section .text
bits 64
long_mode_start:
    ; load null into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov edi, [mb_info_ptr]
    xor rsi, rsi
	call kernel_main
    hlt