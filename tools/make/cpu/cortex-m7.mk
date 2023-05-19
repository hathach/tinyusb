ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m7 \
    -mfloat-abi=hard \
    -mfpu=fpv5-d16 \

  #set(FREERTOS_PORT GCC_ARM_CM7 CACHE INTERNAL "")
  FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM7/r0p1
else ifeq ($(TOOLCHAIN),iar)
  # TODO support IAR
endif
