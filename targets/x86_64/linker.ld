ENTRY(start)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 1M;
    _kernel_start = .;

    .boot : {
        KEEP(*(.multiboot_header))
    } :text

    .text : {
        *(.text)
    } :text

    . = ALIGN(4096);

    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data

    .data : {
        *(.data)
    } :data

    .bss : {
        *(.bss)
    } :data

    . = ALIGN(4096);
    _kernel_end = .;

    /* Assert that the kernel never grows into the LHE load region */
    ASSERT(_kernel_end < 0x400000, "Kernel too large, overlaps LHE load address!")
}