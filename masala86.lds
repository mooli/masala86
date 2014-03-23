OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(__boot_start)

SECTIONS {
    .text 0x00100000 : {
        *(.text.front); /* __boot_start defined in here */

        . = ALIGN(64);
        *(.text);
        *(.text.*);
        . = ALIGN(64);
        *(.rodata);
        *(.rodata.*);

        . = ALIGN(64);
        __kernel_start = .;
        KEEP(*(.data.kernel));
        __kernel_end = .;

}

 .data : {
        . = ALIGN(64);
        *(.data);
        *(.data.*);

    }

 .bss : {
        . = ALIGN(64);
        *(.multiboot.bss);
        . = ALIGN(64);
        *(.bss);
        *(.bss.*);
        . = ALIGN(64);
        *(.common);

        . = ALIGN(4096);
        __boot_scratch = .;
        /* The scratch area extends up to the 14MB point so that it doesn't
        foul any ISA hole that might be present there. */
        . = 0x00e00000 - 0x00100000 - __boot_start;
        __boot_scratch_end = . - 4096;
        __boot_stack = .;
    }
}
