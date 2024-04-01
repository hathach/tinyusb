ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m33 \
    -mfloat-abi=hard \
    -mfpu=fpv5-sp-d16 \

else ifeq ($(TOOLCHAIN),iar)
	CFLAGS += \
		--cpu cortex-m33 \
		--fpu VFPv5-SP \

	ASFLAGS += \
		--cpu cortex-m33 \
		--fpu VFPv5-SP \

endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM33_NTZ/non_secure
