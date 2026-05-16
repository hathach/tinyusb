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
