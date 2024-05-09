ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m0 \
    -mfloat-abi=soft \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m0 \

else ifeq ($(TOOLCHAIN),iar)
  # IAR Flags
  CFLAGS += --cpu cortex-m0
  ASFLAGS += --cpu cortex-m0

else
  $(error "TOOLCHAIN is not supported")
endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM0
