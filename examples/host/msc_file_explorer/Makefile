include ../../../hw/bsp/family_support.mk

FATFS_PATH = lib/fatfs/source

INC += \
	src \
	$(TOP)/$(FATFS_PATH) \
	$(TOP)/lib/embedded-cli \

# Example source
EXAMPLE_SOURCE = \
  src/main.c \
  src/msc_app.c \

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

# FatFS source
SRC_C += \
  $(FATFS_PATH)/ff.c \
  $(FATFS_PATH)/ffsystem.c \
  $(FATFS_PATH)/ffunicode.c \

# suppress warning caused by fatfs
CFLAGS_GCC += -Wno-error=cast-qual

include ../../../hw/bsp/family_rules.mk
