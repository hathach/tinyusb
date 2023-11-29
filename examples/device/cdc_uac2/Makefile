include ../../build_system/make/make.mk

INC += \
	src \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += \
  src/cdc_app.c \
  src/main.c \
  src/uac2_app.c \
  src/usb_descriptors.c \

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../build_system/make/rules.mk
