include ../../build_system/make/make.mk

FREERTOS_SRC = lib/FreeRTOS-Kernel
FREERTOS_PORTABLE_PATH= $(FREERTOS_SRC)/portable/$(if $(findstring iar,$(TOOLCHAIN)),IAR,GCC)

INC += \
	src \
	src/FreeRTOSConfig \
	$(TOP)/hw \
	$(TOP)/$(FREERTOS_SRC)/include \
	$(TOP)/$(FREERTOS_PORTABLE_SRC) \

# Example source
EXAMPLE_SOURCE = \
  src/cdc_app.c \
  src/hid_app.c \
  src/main.c \
  src/msc_app.c \

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

include ../../build_system/make/rules.mk
