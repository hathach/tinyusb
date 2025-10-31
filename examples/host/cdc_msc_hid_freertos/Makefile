RTOS = freertos
include ../../../hw/bsp/family_support.mk

INC += \
	src \


# Example source
EXAMPLE_SOURCE = \
  src/cdc_app.c \
  src/hid_app.c \
  src/main.c \
  src/msc_app.c \

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

include ../../../hw/bsp/family_rules.mk
