ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m85 \
    -mfloat-abi=hard \
    -mfpu=fpv5-d16 \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m85 \
	  -mfpu=fpv5-d16 \

else ifeq ($(TOOLCHAIN),iar)
  CFLAGS += \
  	--cpu cortex-m85 \
  	--fpu VFPv5_D16 \

  ASFLAGS += \
		--cpu cortex-m85 \
		--fpu VFPv5_D16 \

else
  $(error "TOOLCHAIN is not supported")
endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM85_NTZ/non_secure
