ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m33 \
    -mfloat-abi=hard \
    -mfpu=fpv5-sp-d16 \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m33 \
	  -mfpu=fpv5-sp-d16 \

else
  $(error "TOOLCHAIN is not supported")
endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM33_NTZ/non_secure
