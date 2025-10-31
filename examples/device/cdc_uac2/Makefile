include ../../../hw/bsp/family_support.mk

INC += \
	src \


# Example source
EXAMPLE_SOURCE += \
  src/cdc_app.c \
  src/main.c \
  src/uac2_app.c \
  src/usb_descriptors.c \

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

include ../../../hw/bsp/family_rules.mk
