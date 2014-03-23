OUTPUT_FORMAT(elf64-x86-64)
ENTRY(__kernel_start)

SECTIONS {
    .text 0xffffffff81000000 : {
        *(.text.front); /* __kernel_start defined in here */
        *(.text);
        *(.text.*);
        *(.rodata);
        *(.rodata.*);
        *(.gcc_except_table);
        *(.gcc_except_table.*);

        /* exception table needs a NULL pointer at the end */
        . = ALIGN(64);
        __eh_start = .;
        *(.eh_frame);
        *(.eh_frame.*);
        . = . + 8;
        __eh_end = .;

/* It appears that some versions of GCC stick static initialisers in .ctor /
.dtor sections, and others put them in .init_array sections (with the code
calling __cxa_atexit). We add support for both flavours. */

        . = ALIGN(8);
        __ctors_start = .;
        *(.ctor*);
        __init_array_start = .;
        KEEP(*(.init_array.*));
        KEEP(*(.init_array));
        __ctors_end = .;

        __dtors_start = .;
        *(.dtor*);
        __dtors_end = .;
        . = ALIGN(4096);
        __kernel_code_end = .;

    }

 .data : {
        *(.data);
        *(.data.*);
        . = ALIGN(64);
        __kernel_data_end = .;
    }

 .bss : {
        *(.bss);
        *(.bss.*);
        *(.common);
        . = ALIGN(64);
        __kernel_bss_end = .;
    }

/*
/DISCARD/ : {
          *(.rel.*);
          *(.igot.*);
          *(.iplt);
          *(.comment);
}
*/
}
