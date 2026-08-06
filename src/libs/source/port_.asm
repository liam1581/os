global port_inb
global port_outb
global port_inw
global port_outw

port_inb:
	mov dx, di ; load `port` argument
	in al, dx ; read from port given by `port`
	movzx rax, al ; zero-extend
	ret

port_outb:
	mov dx, di ; load `port` argument
	mov al, sil ; load `data` argument
	out dx, al ; write to port given by `port` with data given by `data`
	ret

port_inw:
    mov dx, di
    in ax, dx
    movzx rax, ax
    ret

port_outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret