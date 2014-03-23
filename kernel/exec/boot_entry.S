/*  -*- mode: asm; coding: utf-8 -*- */
/**
        Multiboot header and code entry point
        \file
*/

        .code32
        
        /* This is a Multiboot compatible kernel. See
        http://www.gnu.org/software/grub/manual/multiboot/ for the Multiboot
        specification, which describes the required header, and what
        environment is passed to the kernel. */

        /* multiboot magic constants */
#define MBOOT_MAGIC 0x1badb002
#define MBOOT_PAGE_ALIGN 1<<0
#define MBOOT_MEM_INFO 1<<1
#define MBOOT_AOUT_KLUDGE 1<<16

        /* our multiboot settings */
#define MBOOT_FLAGS MBOOT_MEM_INFO
#define MBOOT_CHECKSUM -(MBOOT_MAGIC + MBOOT_FLAGS)

        /* This header needs to appear within the first 8,192 bytes of the
        kernel image, or it may not be recognised as a Multiboot kernel. To
        ensure this, it is placed in a special section which the linker
        script will then arrange to place right at the start */

        .section .text.front
__boot_start:   .globl __boot_start
        /* skip over multiboot header */
        jmp 2f

        .align 4
        /* mandatory multiboot header: magic, desired options, checksum */
1:      .int MBOOT_MAGIC, MBOOT_FLAGS, MBOOT_CHECKSUM
        /* multiboot address fields: location of header, start of code, end
        of data, end of BSS, entry point */
        .int 1b, 0, 0, 0, 2f
2:
        /* set up a very temporary 32 bit GDT for our 32 bit code. This doesn't
	seem to be strictly necessary, but we have it anyway as belt-and-braces
	in case we somehow break the multiboot loader's GDT, then initialise the
	segments from the GDT. */
        lgdt 7f
        /* data segment 0x8 into GPRs */
        mov $0x8, %dx
        mov %dx, %ds
        mov %dx, %es
        mov %dx, %fs
        mov %dx, %gs
        /* stack segment 0x18 into %esp (and also move %esp to our safe scratch stack) */
        mov $0x18, %dx
        mov %dx, %ss
        lea __boot_stack, %esp
        /* code segment 0x10 into %eip */
        jmp $0x10, $8f
8:

        /* We now call the kernel initialisation code with the address of
        the multiboot structure */
        push %ebx
        call __boot_init
        add $4, %esp

        /* at this point, the multiboot information structure has been
	copied and we're just a GDT load and far jump from getting full 64
	bit mode. Which is what we do here to call before calling the kernel
	proper. We pass the current physical address of the start of the
	kernel in %ebx, and the return value of __boot_init remains in %eax
	*/

        lea __kernel_start, %ebx
        lgdt 4f
        jmp $8, $5f

5:      .code64
        jmp *%rbx

        .align 8
        /* This is the 64 bit GDT. FIXME: what do these magic numbers mean? */
3:      .quad 0x0000000000000000 /* selector 0: unused null descriptor */
        .quad 0x0020980000000000 /* selector 8: code descriptor */
        .quad 0x0000900000000000 /* selector 16: data descriptor */

        /* segment limit (i.e. length) of GDT and 64 bit pointer to it. */
4:      
        .word 4b-3b-1
        .long 3b, 0 /* should really be .quad 3b, but split to appease linker */
        
        .align 8
        /* this is the 32 bit GDT */
6:      
        /* null: base 0, limit 0 */
        .int 0, 0
        /* 0x08: kernel data segment: base 0, limit 4GB, DPL 0, 32 bit, present, system, expand-up, writable */
        .int 0xffff, 0x00cf9200
        /* 0x10: kernel code segment: base 0, limit 4GB, DPL 0, 32 bit, present, system, non-conforming, readable */
        .int 0xffff, 0x00cf9a00
        /* 0x18: kernel stack segment: base 0, limit 4GB, DPL 0, 32 bit, present, system, expand-up, writable */
        .int 0xffff, 0x00cf9200
7:      
        .word 7b-6b-1
        .long 6b
