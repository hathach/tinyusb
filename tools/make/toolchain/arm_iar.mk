# makefile for arm iar toolchain
AS = iasmarm
LD = ilinkarm
OBJCOPY = ielftool --silent
SIZE = size

include $(TOP)/tools/make/cpu/$(CPU_CORE).mk

# Enable extension mode (gcc compatible)
CFLAGS += -e --debug --silent

# silent mode
ASFLAGS += -S
