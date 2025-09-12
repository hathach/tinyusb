ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mthumb \
    -mcpu=cortex-m4 \
    -mfloat-abi=soft

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
	  --target=arm-none-eabi \
	  -mcpu=cortex-m4

else ifeq ($(TOOLCHAIN),iar)
  CFLAGS += --cpu cortex-m4 --fpu none
  ASFLAGS += --cpu cortex-m4 --fpu none

else
  $(error "TOOLCHAIN is not supported")
endif

FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/ARM_CM3
