ENTRY(main)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 0x1000000;

    .text : {
        *(.text.entry)
        *(.text)
    } :text

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text

    . = ALIGN(4096);

    .data : {
        *(.data)
    } :data

    .bss : {
        *(.bss)
    } :data
}