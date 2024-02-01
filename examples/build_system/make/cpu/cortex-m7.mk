ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m7 \
    -mfloat-abi=hard \
    -mfpu=fpv5-d16 \

else ifeq ($(TOOLCHAIN),iar)
  CFLAGS += \
  	--cpu cortex-m7 \
  	--fpu VFPv5_D16 \

  ASFLAGS += \
		--cpu cortex-m7 \
		--fpu VFPv5_D16 \

endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM7/r0p1
