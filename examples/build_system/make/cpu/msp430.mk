ifeq ($(TOOLCHAIN),gcc)
  # nothing to add
else ifeq ($(TOOLCHAIN),clang)
  # nothing to add
else ifeq ($(TOOLCHAIN),iar)
  # nothing to add
else
  $(error "TOOLCHAIN is not supported")
endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC ?= $(FREERTOS_PORTABLE_PATH)/GCC_MSP430F449
