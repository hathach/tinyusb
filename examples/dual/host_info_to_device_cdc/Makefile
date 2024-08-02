include ../../build_system/make/make.mk

INC += \
	src \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

CFLAGS += -Wno-error=cast-align -Wno-error=null-dereference

SRC_C += \
	src/host/hub.c \
	src/host/usbh.c

include ../../build_system/make/rules.mk
