/*  -*- mode: asm; coding: utf-8 -*- */
/**
        Kernel entry point
        \file
*/

        .code64
        
        /* We are called from the other entry.S after paging and 64 bit mode
	has been set up, but we are still running at its address (i.e. probably
	0x01xxxxxx.) We now need to copy the kernel image (text+data) to the
	linked virtual address, then call it. We received the address of the
	handover structure in %rax and the current start address of the kernel
	in %ebx */

        .section .text.front
__kernel_start:  .globl __kernel_start
        push %rax
        lea __kernel_start, %rcx
        lea __kernel_data_end, %rdx
        jmp 2f
1:      mov (%ebx), %rax
        mov %rax, (%rcx)
        add $8, %ebx
        add $8, %rcx
2:      cmp %rdx, %rcx
        jb 1b

        /* initialise kernel BSS section */

        lea __kernel_bss_end, %rdx
        jmp 2f
1:      movl $0, (%rcx)
        add $8, %rcx
2:      cmp %rdx, %rcx
        jb 1b

        /* now we've copied the kernel to its final address, flush the CPU
	caches and jump to the location we've just copied the next instruction
	to. */
        wbinvd
        lea 1f, %rbx
        jmp *%rbx
1:      

        /* call the first stage initialiser, which will initialise high memory
	and return a pointer to a new stack in said memory */
        mov (%rsp), %rdi
        call __kernel_init
        pop %rdi
        mov %rax, %rsp
        
        /* we call a second-stage initialisation which will configure low
	memory. Since the handover structure is in low memory, it will be
	clobbered and its pointer is useless to us */
        call __kernel_init2
        
        /* We now need to call all of the static constructors */
        mov $__ctors_start, %rbx
        jmp 4f
3:      call *(%rbx)
        add $8, %rbx
4:      cmp $__ctors_end, %rbx
        jb 3b

        /* now we call the kernel run loop */
        call __kernel_run
        
        /* Getting here is a "can't happen" situation - the kernel is
        supposed to start a scheduler and never return. However, if we do,
        we run the destructors */
        mov $__dtors_start, %rbx
        jmp 2f
1:      call *(%ebx)
        add $8, %rbx
2:      cmp $__dtors_end, %rbx
        jb 1b

        /* Finally, disable interrupts and halt the CPU */

        cli
        hlt
