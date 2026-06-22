include ../../../hw/bsp/family_support.mk

INC += \
	src \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

# Include device and host stack
SRC_C += \
	src/class/cdc/cdc_device.c \
	src/host/hub.c \
	src/host/usbh.c

include ../../../hw/bsp/family_rules.mk
