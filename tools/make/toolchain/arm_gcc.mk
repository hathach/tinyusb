# makefile for arm gcc toolchain

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AS = $(CC) -x assembler-with-cpp
LD = $(CC)

GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

# ---------------------------------------
# Compiler Flags
# ---------------------------------------
CFLAGS += \
  -MD \
  -ggdb \
  -fdata-sections \
  -ffunction-sections \
  -fsingle-precision-constant \
  -fno-strict-aliasing \
  -Wall \
  -Wextra \
  -Werror \
  -Wfatal-errors \
  -Wdouble-promotion \
  -Wstrict-prototypes \
  -Wstrict-overflow \
  -Werror-implicit-function-declaration \
  -Wfloat-equal \
  -Wundef \
  -Wshadow \
  -Wwrite-strings \
  -Wsign-compare \
  -Wmissing-format-attribute \
  -Wunreachable-code \
  -Wcast-align \
  -Wcast-function-type \
  -Wcast-qual \
  -Wnull-dereference \
  -Wuninitialized \
  -Wunused \
  -Wreturn-type \
  -Wredundant-decls \

# conversion is too strict for most mcu driver, may be disable sign/int/arith-conversion
#  -Wconversion

# Size Optimization as default
CFLAGS_OPTIMIZED ?= -Os

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -O0
  NO_LTO = 1
else
  CFLAGS += $(CFLAGS_OPTIMIZED)
endif

# ---------------------------------------
# Linker Flags
# ---------------------------------------
LDFLAGS += \
  -Wl,-Map=$@.map \
  -Wl,-cref \
  -Wl,-gc-sections \

# Some toolchain such as renesas rx does not support --print-memory-usage flags
ifneq ($(FAMILY),rx)
LDFLAGS += -Wl,--print-memory-usage
endif
