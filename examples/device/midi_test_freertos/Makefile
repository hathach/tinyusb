include ../../build_system/make/make.mk

INC += \
  src \
  $(TOP)/hw \

# Example source
EXAMPLE_SOURCE += \
  src/main.c \
  src/usb_descriptors.c \

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
