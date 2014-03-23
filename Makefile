# -*- Makefile -*-

# set to "true" or "false" depending on whether assembler output is required
GENASM := false

# The amount of RAM to present when booting the OS in KVM.
VIRTRAM := 480
#VIRT := sudo kvm
VIRT := qemu-system-x86_64

# The code in the kernel is compiled for three environments: boot, kernel and
# test. boot has to be for a freestanding x86, kernel may be for freestanding
# x86 or x86_64, and test is hosted code in the native environment.

AS := as
LD := ld
#CXX := mycolorgcc ccache g++-4.7
#CXX := ccache g++-4.7
#CXX := masala86g++
#CXX := ccache g++-4.7
CXX := g++-4.7
#CXX := ccache clang++
#CXX = clang++
TESTCXX := $(CXX)
#CXX := g++
CXXFILT := c++filt
NM := nm
AR := ar
OBJCOPY := objcopy
STRIP := strip

#ASFLAGS := -32
#ARCHFLAGS := -m32 -march=core2 -O3

# we want -march=i386 on the bootloader to disable various extensions such as
# SIMD that we're not yet prepared to handle

BOOTARCHFLAGS := -ffreestanding -m32 -march=i386 -mtune=corei7 -Os -flto -msoft-float
ARCHFLAGS := -ffreestanding -m64 -march=athlon64 -mtune=corei7 -Os -mcmodel=kernel -mno-red-zone 
#ARCHFLAGS := -ffreestanding -m32 -march=core2 -O0 #-flto

CXXFLAGS := --pipe -g -x c++ -std=gnu++11 \
	-fdiagnostics-show-option -Wall -Wextra -Wno-error -Weffc++ -Wshadow \
	-Wunreachable-code \
	 -funit-at-a-time -ffunction-sections -fdata-sections	\
	-fkeep-inline-functions \
                                    -Wundef -Wshadow -Wlarger-than-4096      \
        -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion \
        -Wsign-compare -Waggregate-return -Wno-aggregate-return                       \
        -Wmissing-noreturn -Wredundant-decls                        \
        -Wdisabled-optimization                                                       \
                                                                              \
        -Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder -Weffc++             \
        -Wno-non-template-friend -Wold-style-cast -Woverloaded-virtual                \
        -Wsign-promo -Wsynth                                                  \
	-fno-rtti -fno-exceptions -Werror

#-Wpacked -Wpadded

BOOTCXXEXTRA := -fno-rtti -fno-exceptions -fno-threadsafe-statics -m32
BOOTASEXTRA := -32

# -Wunreachable-code gives bogus warnings under -O3

#DEBUGFLAGS := -Os -DNDEBUG
DEBUGFLAGS := -O3 -DNDEBUG
#DEBUGFLAGS := -O0 -fno-lto -ggdb3
PROFFLAGS := #-pg
BOOTLIBS := /usr/lib/gcc/x86_64-linux-gnu/4.7/32/libgcc.a
LIBS := #/usr/lib/gcc/x86_64-linux-gnu/4.7/libgcc.a
	#/usr/lib/gcc/x86_64-linux-gnu/4.7/libgcc.a \
	/usr/lib/gcc/x86_64-linux-gnu/4.7/libsupc++.a \
#	/usr/lib/gcc/x86_64-linux-gnu/4.7/libgcc_eh.a
#BOOTINCLUDE := -nostdinc -Ikernel/x86/ -Ikernel/
#INCLUDE := -nostdinc -Ikernel/x86-64/ -Iinclude/ -Ikernel/ -I/usr/include/c++/4.7/ -I/usr/include/c++/4.7/x86_64-linux-gnu/32 -I/usr/include #-ISTLport/stlport/
#INCLUDE := -nostdinc -ISTLport/stlport -I/usr/include/c++/4.7 -I/usr/include/c++/4.7/x86_64-linux-gnu/32 -IuClibc/include -IuClibc/libc/sysdeps/linux/i386 -Iinclude -Ikernel
#INCLUDE := -nostdinc -I/usr/include/c++/4.7/ -I/usr/include/c++/4.7/x86_64-linux-gnu/32/ -IuClibc/include -ISTLport/stlport -Iinclude -Ikernel
BOOTINCLUDE := -nostdinc -IuClibc/include -Iinclude -Ikernel
INCLUDE := -nostdinc -IuClibc/include -Iinclude -Ikernel
TESTINCLUDE := -Ikernel/x86/ -Ikernel/
#BOOTINCLUDE := -nostdinc -Iboot_include # -I/usr/include/g++-3 -I/usr/include/g++-v3
BOOTLDS := masala86.lds
LDS := kernel.lds

.PHONY: all clean floppy test

all: masala86

# All the modules that make up this project
#MODULES := $(shell find * -name module.mk | xargs -rn1 dirname)
MODULES := $(shell find kernel/ -name module.mk | xargs -n1 dirname)

# The makefile fragments add source files to the following defines to
# compile them appropriately:

# post-boot setup code
BOOTSRC :=
# kernel code
SRC :=
# boot/kernel code testable in a hosted environment
TESTSRC :=
# Hosted test wrappers (each has its own main())
TESTMAINSRC := 

# Include all the makefile fragments
include $(patsubst %,%/module.mk,$(MODULES))

# Determine the names of the object files
ASMOBJ := $(patsubst %.S,%.o,$(filter %.S,$(SRC)))
CPPOBJ := $(patsubst %.cpp,%.o,$(filter %.cpp,$(SRC)))

BOOTASMOBJ := $(patsubst %.S,%.bo,$(filter %.S,$(BOOTSRC)))
BOOTCPPOBJ := $(patsubst %.cpp,%.bo,$(filter %.cpp,$(BOOTSRC)))

TESTCPPOBJ := $(patsubst %.cpp,%.to,$(filter %.cpp,$(TESTSRC)))

TESTCPPMAINBIN := $(patsubst %.cpp,%.t,$(filter %.cpp,$(TESTMAINSRC)))

# Include the include dependencies
-include $(BOOTCPPOBJ:.bo=.d) $(CPPOBJ:.o=.d) $(TESTCPPOBJ:.to=.d)

# calculate the include dependencies
%.d : %.cpp
#	@ /bin/echo -e "\\033[1m Updating $<'s dependencies\\033[0m"
	@ /bin/echo -n $@ `dirname $<`/ >$@
	@ $(CXX) $(INCLUDE) $(CXXFLAGS) $(PROFFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS)  -MM $< >> $@ || ( rm $@ ; exit 1) ;\

# explicit rules for building

%.o : %.S
	@ /bin/echo -e "\\033[1m Assembling $<\\033[0m"
	@ $(CXX) $(ARCHFLAGS) -c -o $@ $<

%.bo : %.S
	@ /bin/echo -e "\\033[1m Assembling $< (boot)\\033[0m"
	@ $(CXX) $(BOOTARCHFLAGS) -c -o $@ $<

%.o : %.cpp
	@ /bin/echo -e "\\033[1m Compiling $<\\033[0m"
	@ if $(GENASM) ; then $(CXX) $(INCLUDE) $(CXXFLAGS) $(PROFFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -g0 -fverbose-asm -S -o /dev/stdout $< | $(CXXFILT) >$*.s ; fi
	@ $(CXX) $(INCLUDE) $(CXXFLAGS) $(PROFFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -c -o $*.o $<

%.bo : %.cpp
	@ /bin/echo -e "\\033[1m Compiling $< (boot)\\033[0m"
	@ if $(GENASM) ; then $(CXX) $(BOOTINCLUDE) $(CXXFLAGS) $(PROFFLAGS) $(BOOTARCHFLAGS) $(DEBUGFLAGS) $(BOOTCXXEXTRA) -g0 -fverbose-asm -S -o /dev/stdout $< | $(CXXFILT) >$*.s ; fi
	@ $(CXX) $(BOOTINCLUDE) $(CXXFLAGS) $(PROFFLAGS) $(BOOTARCHFLAGS) $(DEBUGFLAGS) $(BOOTCXXEXTRA) -c -o $*.bo $<

%.to : %.cpp
	@ /bin/echo -e "\\033[1m Compiling $< (testable)\\033[0m"
	@ $(TESTCXX) $(TESTINCLUDE) $(CXXFLAGS) -DTESTING=1 -Dinline= -O0 -ggdb --coverage -c -o $*.to $<

# Note carefully the GCC docs for -fcoverage-arcs. Specifically, if you
# generate an executable directly, the .gcda file strips the path, so we
# need to do a two-stage build-then-link to get the .gcda to be put in the
# same directory as the source file.

%.t : %.cpp test.a
	@ /bin/echo -e "\\033[1m Compiling $< (test)\\033[0m"
	@ $(TESTCXX) $(TESTINCLUDE) $(CXXFLAGS) -DTESTING=1 -Dinline= -O0 -ggdb --coverage -c -o $*.to $<
	@ $(TESTCXX) $(TESTINCLUDE) -ggdb --coverage -o $*.t $*.to test.a

test.a: $(sort $(TESTCPPOBJ))
	@ /bin/echo -e "\\033[1m Archiving testables \\033[0m"
	@ ar rcs $@ $(sort $(TESTCPPOBJ))

test: $(TESTCPPMAINBIN) test.a
	@ /bin/echo -e "\\033[1m Running test suite \\033[0m"
	@ lcov --directory t/ --zerocounters
	@ prove -e '' t/
	@ find * -iname '*.gcno' | while read ; do gcov -p -o$$REPLY $$REPLY >/dev/null ; done

masala86.small: masala86
	@ /bin/echo -e "\\033[1m Stripping \\033[0m"
	@ cp $< $@
	@ $(STRIP) $@
	@ /bin/echo -e "\\033[1m Compressing \\033[0m"
	@ bzip2 -9vvq $@
	@ mv $@.bz2 $@

masala86: $(BOOTLDS) $(BOOTASMOBJ) $(BOOTCPPOBJ) kernel.bin
	@ /bin/echo -e "\\033[1m Linking (boot)\\033[0m"
	@ $(LD) -o $@ -n -sort-section=name --gc-sections --script=$(BOOTLDS) $(BOOTASMOBJ) $(BOOTCPPOBJ) kernel.bin $(BOOTLIBS) -flto
# filtered twice because sometimes it doesn't work the first time
	@ $(NM) $@ | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | c++filt | c++filt | sort -u >$@.map

kernel.bin: kernel.o
	@ /bin/echo -e "\\033[1m Prepare kernel image for boot\\033[0m"

	@ $(OBJCOPY) -O binary $< $@
	@ $(OBJCOPY) -I binary -O elf32-i386 -B i386 $@ --rename-section .data=.data.kernel

kernel.o: $(LDS) $(ASMOBJ) $(CPPOBJ)
	@ /bin/echo -e "\\033[1m Linking (kernel)\\033[0m"
#	@ $(LD) --sort-section=name --print-gc-sections --gc-sections $^ -o $@
	@ $(LD) -o $@ -n -sort-section=name --gc-sections --script=$(LDS) $(sort $(ASMOBJ) $(CPPOBJ)) $(LIBS) -flto
# filtered twice because sometimes it doesn't work the first time
	@ $(NM) $@ | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | c++filt | c++filt | sort -u >$@.map

clean:
	find . -name '*~' -print0 | xargs -0 rm -f
	find . -name '*.[osd]' -print0 | xargs -0 rm -f
	find . -name '*.[tb]o' -print0 | xargs -0 rm -f
	find . -name '*.gc??' -print0 | xargs -0 rm -f
	rm -f {kernel,masala86}{,.map,.small,.fdd}
	rm -rf html/ genhtml/ t.info
	rm -f test.a

wc:
	find kernel/ -type f \( -name '*.[ch]pp' -or -name '*.S' \) -print0 | sort -z | xargs -0 wc

# VMWare supports raw floppy images if they're called *.fdd, whereas VirtualBox
# prefers *.img. This thus creates files with both names, hardlinked together.

masala86.fdd: masala86
	@ echo -e "\\033[1m Creating floppy image\\033[0m"
	@ ./script/mkimage.sh $@ $<
	@ ln -f $@ masala86.img

masala86.hdd:
	dd if=/dev/null bs=8225280 seek=1024 count=1 of=$@

masala86.iso: masala86.fdd
	genisoimage -b $< -r -hfs --osx-double . >$@

floppy: masala86.fdd
iso: masala86.iso

tftp: masala86
	sudo cp $< /tftpboot/$<
	sudo chmod 644 /tftpboot/$<

#kvm: masala86
#	$(VIRT) -kernel $< -nographic -echr 29 -no-reboot -no-shutdown -m $(VIRTRAM)

kvm_curses: masala86
	$(VIRT) -kernel $< -curses -echr 29 -no-reboot -no-shutdown -m $(VIRTRAM)

kvm: masala86
	$(VIRT) -kernel $<  -echr 29 -nographic -no-reboot -no-shutdown -m $(VIRTRAM)

kvm_fdd: masala86.fdd
#	$(VIRT) -fda $< -nographic -echr 29 -drive file=masala86.hdd,if=ide,format=raw -m $(VIRTRAM)
	$(VIRT) -fda $< -nographic -echr 29 -m $(VIRTRAM)

kvm_vnc: masala86.fdd
	$(VIRT) -fda $< -echr 29 -display vnc=:0 -no-reboot -no-shutdown -m $(VIRTRAM)

