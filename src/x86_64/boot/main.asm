global start
global mb_info_ptr
extern long_mode_start

; Number of 1GiB regions to identity-map (2MiB huge pages). Must be kept in
; sync with PMM_MAX_MEMORY in src/libs/src/pmm.c -- physical memory beyond
; this is never mapped, so the PMM can't hand it out even if it's present.
MAPPED_GIB equ 16

section .text
bits 32
start:
	; The Multiboot2 spec explicitly leaves all flag bits (including DF)
	; undefined on entry -- only EBX (the multiboot info pointer) is
	; guaranteed. The SysV ABI requires DF=0 for every function call from
	; here on (compilers assume it's already clear and don't reset it
	; themselves before emitting rep movs/stos/etc for struct copies and
	; zero-init), so this must happen before anything else, including the
	; ebx save below.
	cld

	; GRUB passes the multiboot2 info pointer in ebx. Save it immediately,
	; before check_cpuid/check_long_mode (which use cpuid, clobbering ebx).
	mov [mb_info_ptr], ebx

	mov esp, stack_top

	call check_multiboot
	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

	lgdt [gdt64.pointer]
	jmp gdt64.code_segment:long_mode_start

	hlt

check_multiboot:
	cmp eax, 0x36D76289
	jne .no_multiboot
	ret
.no_multiboot:
	mov al, "M"
	jmp error

check_cpuid:
	pushfd
	pop eax
	mov ecx, eax
	xor eax, 1 << 21
	push eax
	popfd
	pushfd
	pop eax
	push ecx
	popfd
	cmp eax, ecx
	je .no_cpuid
	ret
.no_cpuid:
	mov al, "C"
	jmp error

check_long_mode:
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb .no_long_mode

	mov eax, 0x80000001
	cpuid
	test edx, 1 << 29
	jz .no_long_mode
	
	ret
.no_long_mode:
	mov al, "L"
	jmp error

setup_page_tables:
	mov eax, page_table_l3
	or eax, 0b11 ; present, writable
	mov [page_table_l4], eax

	; L3[0..MAPPED_GIB-1], each pointing to its own 1GiB-worth L2 table
	mov ecx, 0
.l3_loop:
	mov eax, ecx
	imul eax, eax, 4096 ; offset of the i-th L2 table (each is one page)
	add eax, page_table_l2
	or eax, 0b11 ; present, writable
	mov [page_table_l3 + ecx * 8], eax

	inc ecx
	cmp ecx, MAPPED_GIB
	jne .l3_loop

	; L2 entries: MAPPED_GIB * 512 huge (2MiB) pages, identity-mapped.
	; ecx is a global 2MiB-page index (0 .. MAPPED_GIB*512-1). The physical
	; address for entry ecx is (ecx << 21), which can exceed 32 bits, so
	; the low/high dwords of the PTE are built separately via shifts
	; rather than with `mul`, which would silently truncate.
	mov ecx, 0
.l2_loop:
	mov edx, ecx
	shr edx, 11          ; high 32 bits of the physical address

	mov eax, ecx
	shl eax, 21           ; low 32 bits of the physical address
	or eax, 0b10000011    ; present, writable, huge page (PS)

	; esi = &page_table_l2[ (ecx>>9) * 4096 + (ecx & 0x1FF) * 8 ]
	mov esi, ecx
	shr esi, 9
	shl esi, 12            ; * 4096 (bytes per L2 table)
	mov ebp, ecx
	and ebp, 0x1FF
	shl ebp, 3               ; * 8 (bytes per PTE)
	add esi, ebp
	add esi, page_table_l2

	mov [esi], eax
	mov [esi + 4], edx

	inc ecx
	cmp ecx, MAPPED_GIB * 512
	jne .l2_loop

	ret

enable_paging:
	; pass page table location to cpu
	mov eax, page_table_l4
	mov cr3, eax

	; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging
	mov eax, cr0
	or eax, 1 << 31
	mov cr0, eax

	ret

error:
	; print "ERR: X" where X is the error code
	mov dword [0xb8000], 0x4F524F45
	mov dword [0xb8004], 0x4F3A4F52
	mov dword [0xb8008], 0x4F204F20
	mov byte  [0xb800a], al
	hlt

section .bss
align 4096
page_table_l4:
	resb 4096
page_table_l3:
	resb 4096
page_table_l2:
	resb 4096 * MAPPED_GIB
stack_bottom:
	resb 4096 * 4
stack_top:

align 4
mb_info_ptr:
	resd 1

section .rodata
gdt64:
	dq 0 ; zero entry
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.pointer:
	dw $ - gdt64 - 1 ; length
	dq gdt64 ; address