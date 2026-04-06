MCU_VARIANT = stm32f103xb

CFLAGS += -DSTM32F103xB -DHSE_VALUE=8000000U

# Linker
LD_FILE = $(BOARD_PATH)/STM32F103XC_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32f103rc

# flash target ROM bootloader
flash: flash-jlink
