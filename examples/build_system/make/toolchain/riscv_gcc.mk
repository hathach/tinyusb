# makefile for arm gcc toolchain

# Can be set by family, default to ARM GCC
CROSS_COMPILE ?= riscv-none-embed-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AS = $(CC) -x assembler-with-cpp
LD = $(CC)

GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

CFLAGS += \
  -fsingle-precision-constant \

LIBS += -lgcc -lm -lnosys

include ${TOP}/examples/build_system/make/toolchain/gcc_common.mk
