MCU_VARIANT = stm32f103xe

CFLAGS += -DSTM32F103xE -DHSE_VALUE=8000000U

# Linker
LD_FILE = ${ST_CMSIS}/Source/Templates/gcc/linker/STM32F103XE_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32f103ze

# flash target ROM bootloader
flash: flash-jlink
