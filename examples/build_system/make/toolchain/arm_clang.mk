CC = clang
CXX = clang++
AS = $(CC) -x assembler-with-cpp
LD = $(CC)

GDB = $(CROSS_COMPILE)gdb
OBJCOPY = llvm-objcopy
SIZE = llvm-size

include ${TOP}/examples/build_system/make/toolchain/gcc_common.mk
