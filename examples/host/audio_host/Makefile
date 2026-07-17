include ../../../hw/bsp/family_support.mk
RHPORT_HOST=1
INC += \
	src \

# CFLAGS += -Wno-unused-function -Wno-unused-variable
# LDFLAGS += -Wno-unused-function -Wno-unused-variable
# Example source
EXAMPLE_SOURCE += \
	src/audio_app.c \
	src/main.c

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

include ../../../hw/bsp/family_rules.mk
