MCU_VARIANT = stm32c5a3xx

CFLAGS += \
  -DSTM32C5A3xx \
  -DHSE_VALUE=48000000 \
  -DHSE_STARTUP_TIMEOUT=100

# GCC
LD_FILE = hw/mcu/st/stm32c5xx-dfp/Source/Templates/gcc/linker/stm32c5a3xg_flash.ld


# For flash-jlink target
JLINK_DEVICE = stm32c5a3zg
