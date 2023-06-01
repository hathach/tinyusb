ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m4 \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16 \

  #set(FREERTOS_PORT GCC_ARM_CM4F CACHE INTERNAL "")
  FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F
else ifeq ($(TOOLCHAIN),iar)
  # TODO support IAR
endif
