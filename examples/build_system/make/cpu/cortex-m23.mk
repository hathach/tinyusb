ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m23 \
    -mfloat-abi=soft \

else ifeq ($(TOOLCHAIN),iar)
  # IAR Flags
  CFLAGS += --cpu cortex-m23
  ASFLAGS += --cpu cortex-m23
endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM23
