include ../../../tools/top.mk
include ../../make.mk

ifeq ($(DISABLE_MJPEG),1)
CFLAGS += -DCFG_EXAMPLE_VIDEO_DISABLE_MJPEG
endif
ifeq ($(FORCE_READONLY),1)
CFLAGS += -DCFG_EXAMPLE_VIDEO_READONLY
endif

INC += \
	src \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../rules.mk
