ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m33 \
    -mfloat-abi=hard \
    -mfpu=fpv5-d16 \

  #set(FREERTOS_PORT GCC_ARM_CM33_NONSECURE CACHE INTERNAL "")
else ifeq ($(TOOLCHAIN),iar)
  # TODO support IAR
endif
