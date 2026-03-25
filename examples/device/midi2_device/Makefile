# This example requires RP2040/RP2350 (USB descriptors and config are board-specific)
ifeq (,$(findstring rp2040,$(FAMILY)))
$(info Skipping midi2_device: requires FAMILY=rp2040)
all:
	@:
.DEFAULT:
	@:
else

include ../../../hw/bsp/family_support.mk

INC += \
	src \

# Example source
EXAMPLE_SOURCE += \
  src/main.c \
  src/usb_descriptors.c \

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

# Suppress pre-existing warning in usbd.c
CFLAGS_GCC += -Wno-type-limits

include ../../../hw/bsp/family_rules.mk

endif
