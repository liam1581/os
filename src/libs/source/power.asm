bits 64

section .text
global arch_restart
global arch_shutdown

arch_restart:
    ; Reboot the system using the keyboard controller
    mov al, 0xFE
    out 0x64, al
    hlt


arch_shutdown:
    ; For QEMU (Modern)
    mov ax, 0x2000
    mov dx, 0x604
    out dx, ax
    
    ; For VirtualBox / Older QEMU
    mov ax, 0x3100
    mov dx, 0x4004
    out dx, ax

    ; If we're still here, try the "Shutdown" via Bochs/QEMU debug port
    mov dx, 0xB004
    mov ax, 0x2000
    out dx, ax
    
    hlt