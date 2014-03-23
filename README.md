masala86
========

This is an early hack of a toy operating system kernel I built to explore bare
metal programming in C++. I've not touched it for a while, but other people
might find some of it interesting.

The kernel image itself is a
[Multiboot](http://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
image and thus can be started with a Multiboot-compatible loader such as GRUB
or qemu. Multiboot can only boot 32 bit code and does not itself provide
paging, and 64 bit mode *requires* enabling paging, so there is a stub 32 bit
image that sets up suitable page tables and shuffles memory around so the
kernel and its workspace don't get clobbered while booting up.

So far, the code does the following:

* Provides a kprintf()-alike for logging boot progress, with simple bit-banging
  output drivers for both the standard serial port and VGA text-mode console.

* Provides a Knuth-style buddy allocator for pages, a Solaris-style slab
  allocator for arbitrary-size allocations, and new/free implemented in terms
  of these.

* Parses the Multiboot information structure to determine where usable RAM is,
  and configures the allocator to use them.

* Sets up appropriate long-mode page tables.

There is also a stub task-switcher but this has not yet been implemented. There
is also a massive gap where there should be a test suite to ensure that the
allocators actually work.

