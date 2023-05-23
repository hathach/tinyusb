ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m33 \
    -mfloat-abi=hard \
    -mfpu=fpv5-sp-d16 \

    #-mfpu=fpv5-d16 \

  #set(FREERTOS_PORT GCC_ARM_CM33_NONSECURE CACHE INTERNAL "")
  FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM33_NTZ/non_secure
else ifeq ($(TOOLCHAIN),iar)
  # TODO support IAR
endif
