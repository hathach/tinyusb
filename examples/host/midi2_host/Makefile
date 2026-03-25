# This example requires RP2040/RP2350 (PIO-USB, Pico SDK I2C, SSD1306 display)
ifeq (,$(findstring rp2040,$(FAMILY)))
$(info Skipping midi2_host: requires FAMILY=rp2040)
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
  src/display.c \

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

# Suppress pre-existing warning
CFLAGS_GCC += -Wno-type-limits

include ../../../hw/bsp/family_rules.mk

endif
