include ../../../tools/top.mk
include ../../make.mk

INC += \
	src \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

CFLAGS += -Wno-error=cast-align

# TinyUSB Host Stack source
SRC_C += \
	src/class/cdc/cdc_host.c \
	src/class/hid/hid_host.c \
	src/class/msc/msc_host.c \
	src/host/hub.c \
	src/host/usbh.c \
	src/host/usbh_control.c \
	src/host/ehci/ehci.c \
	src/host/ohci/ohci.c \
	src/portable/nxp/lpc18_43/hcd_lpc18_43.c \
	src/portable/nxp/lpc17_40/hcd_lpc17_40.c

include ../../rules.mk
