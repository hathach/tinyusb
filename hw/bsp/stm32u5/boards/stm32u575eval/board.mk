MCU_VARIANT = stm32u575xx
CFLAGS += \
  -DSTM32U575xx \

# All source paths should be relative to the top level.
LD_FILE = ${FAMILY_PATH}/linker/STM32U575xx_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32u575ai
