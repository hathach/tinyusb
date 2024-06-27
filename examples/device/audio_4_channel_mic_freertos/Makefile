include ../../build_system/make/make.mk

FREERTOS_SRC = lib/FreeRTOS-Kernel
FREERTOS_PORTABLE_PATH = $(FREERTOS_SRC)/portable/$(if $(findstring iar,$(TOOLCHAIN)),IAR,GCC)

INC += \
	src \
	src/FreeRTOSConfig \
	$(TOP)/hw \
	$(TOP)/$(FREERTOS_SRC)/include \
	$(TOP)/$(FREERTOS_PORTABLE_SRC) \

# Example source
EXAMPLE_SOURCE = \
	src/freertos_hook.c \
	src/main.c \
	src/usb_descriptors.c

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

# FreeRTOS source, all files in port folder
SRC_C += \
	$(FREERTOS_SRC)/list.c \
	$(FREERTOS_SRC)/queue.c \
	$(FREERTOS_SRC)/tasks.c \
	$(FREERTOS_SRC)/timers.c \
	$(subst $(TOP)/,,$(wildcard $(TOP)/$(FREERTOS_PORTABLE_SRC)/*.c))

SRC_S += \
	$(subst $(TOP)/,,$(wildcard $(TOP)/$(FREERTOS_PORTABLE_SRC)/*.s))

# Suppress FreeRTOS warnings
CFLAGS += -Wno-error=cast-qual -Wno-error=redundant-decls

# FreeRTOS (lto + Os) linker issue
LDFLAGS += -Wl,--undefined=vTaskSwitchContext

include ../../build_system/make/rules.mk
