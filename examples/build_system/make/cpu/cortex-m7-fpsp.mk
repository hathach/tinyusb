ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m7 \
    -mfloat-abi=hard \
    -mfpu=fpv5-sp-d16 \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m7 \
	  -mfpu=fpv5-sp-d16 \

else ifeq ($(TOOLCHAIN),iar)
  CFLAGS += \
  	--cpu cortex-m7 \
  	--fpu VFPv5_sp \

  ASFLAGS += \
		--cpu cortex-m7 \
		--fpu VFPv5_sp \

else
  $(error "TOOLCHAIN is not supported")
endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM7/r0p1
