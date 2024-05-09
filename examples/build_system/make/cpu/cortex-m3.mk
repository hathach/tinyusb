ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m3 \
    -mfloat-abi=soft \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m3 \

else ifeq ($(TOOLCHAIN),iar)
  # IAR Flags
  CFLAGS += --cpu cortex-m3
  ASFLAGS += --cpu cortex-m3

else
  $(error "TOOLCHAIN is not supported")
endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM3
