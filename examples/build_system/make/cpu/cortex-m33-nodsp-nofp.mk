ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
	-mthumb \
	-mcpu=cortex-m33+nodsp \
	-mfloat-abi=soft \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m33 \
	  -mfpu=softvp \

else ifeq ($(TOOLCHAIN),iar)
  CFLAGS += \
		--cpu cortex-m33+nodsp \

  ASFLAGS += \
		--cpu cortex-m33+nodsp \

else
  $(error "TOOLCHAIN is not supported")
endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM33_NTZ/non_secure
