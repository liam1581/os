ENTRY(start)

SECTIONS {
    . = 1M;
    _kernel_start = .;

    .boot : {
        KEEP(*(.multiboot_header))
    }

    .text : {
        *(.text)
    }

    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    . = ALIGN(4096);
    _kernel_end = .;

    /* Assert that the kernel never grows into the LHE load region */
    ASSERT(_kernel_end < 0x400000, "Kernel too large, overlaps LHE load address!")
}