# -*- mode: makefile; coding: us-ascii -*-

BOOTSRC += \
	kernel/exec/boot.cpp \
	kernel/exec/boot_entry.S \
	kernel/exec/format.cpp \
	kernel/exec/memory.cpp \

SRC += \
	kernel/exec/format.cpp \
	kernel/exec/kernel.cpp \
	kernel/exec/kernel_entry.S \
	kernel/exec/memory.cpp \
	kernel/exec/task.cpp \
