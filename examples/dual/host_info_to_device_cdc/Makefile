include ../../../hw/bsp/family_support.mk

INC += \
	src \


# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

CFLAGS_GCC += -Wno-error=cast-align -Wno-error=null-dereference

SRC_C += \
	src/host/hub.c \
	src/host/usbh.c

include ../../../hw/bsp/family_rules.mk
