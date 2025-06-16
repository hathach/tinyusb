MCU_VARIANT = stm32u585xx
CFLAGS += \
  -DSTM32U585xx \

# All source paths should be relative to the top level.
LD_FILE = ${FAMILY_PATH}/linker/STM32U575xx_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32u585zi
