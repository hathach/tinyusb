ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -march=rv32imac_zicsr \
    -mabi=ilp32 \

else ifeq ($(TOOLCHAIN),clang)
  CFLAGS += \
    -march=rv32imac_zicsr \
    -mabi=ilp32 \

else ifeq ($(TOOLCHAIN),iar)
  $(error not support)

endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
