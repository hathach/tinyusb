ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m7 \
    -mfloat-abi=hard \
    -mfpu=fpv5-d16 \

  #set(FREERTOS_PORT GCC_ARM_CM7 CACHE INTERNAL "")
else ifeq ($(TOOLCHAIN),iar)
  # TODO support IAR
endif
