ENTRY(main)

SECTIONS {
    . = 0x400000;

    .text : {
        *(.text.entry)
        *(.text)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}