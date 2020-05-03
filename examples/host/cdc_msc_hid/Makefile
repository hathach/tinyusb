include ../../../tools/top.mk
include ../../make.mk

INC += \
	src \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))


# TinyUSB Host Stack source
SRC_C += \
	src/host/usbh.c \
	src/host/hub.c \
	src/host/ehci/ehci.c \
	src/class/cdc/cdc_host.c \
	src/host/ehci/ehci.c \
	src/portable/nxp/lpc18_43/hcd_lpc18_43.c

include ../../rules.mk
