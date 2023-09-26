include ../../make.mk

FREERTOS_SRC = lib/FreeRTOS-Kernel
FREERTOS_PORTABLE_PATH= $(FREERTOS_SRC)/portable/$(if $(USE_IAR),IAR,GCC)

INC += \
	src \
	$(TOP)/hw \
	$(TOP)/$(FREERTOS_SRC)/include \
	$(TOP)/$(FREERTOS_PORTABLE_SRC) \

# Example source
EXAMPLE_SOURCE = \
  src/cdc_app.c \
  src/freertos_hook.c \
  src/hid_app.c \
  src/main.c \
  src/msc_app.c \

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

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
