CFLAGS += \
  -DSTM32U575xx \

# All source paths should be relative to the top level.
LD_FILE = ${ST_CMSIS}/Source/Templates/gcc/linker/STM32U575xx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32u575xx.s

# For flash-jlink target
JLINK_DEVICE = stm32u575ai
