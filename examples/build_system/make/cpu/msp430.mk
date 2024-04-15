ifeq ($(TOOLCHAIN),gcc)
  # nothing to add
else ifeq ($(TOOLCHAIN),iar)
  # nothing to add
endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/GCC_MSP430F449
