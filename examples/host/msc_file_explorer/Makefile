include ../../../tools/top.mk
include ../../make.mk

FATFS_PATH = lib/fatfs/source

INC += \
	src \
	$(TOP)/hw \
	$(TOP)/$(FATFS_PATH) \
	$(TOP)/lib/embedded-cli \

# Example source
EXAMPLE_SOURCE = \
  src/main.c \
  src/msc_app.c \

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

# FatFS source
SRC_C += \
  $(FATFS_PATH)/ff.c \
  $(FATFS_PATH)/ffsystem.c \
  $(FATFS_PATH)/ffunicode.c \

# TODO: suppress warning caused by host stack
CFLAGS += -Wno-error=cast-align -Wno-error=null-dereference

# TinyUSB Host Stack source
SRC_C += \
	src/class/cdc/cdc_host.c \
	src/class/hid/hid_host.c \
	src/class/msc/msc_host.c \
	src/host/hub.c \
	src/host/usbh.c \
	src/portable/ohci/ohci.c \
	src/portable/nxp/lpc17_40/hcd_lpc17_40.c

include ../../rules.mk
