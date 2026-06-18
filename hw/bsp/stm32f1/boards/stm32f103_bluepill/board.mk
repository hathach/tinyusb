MCU_VARIANT = stm32f103xb

CFLAGS += -DSTM32F103xB -DHSE_VALUE=8000000U -DCFG_EXAMPLE_VIDEO_READONLY

# Linker
LD_FILE = $(BOARD_PATH)/STM32F103X8_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32f103c8

# flash target ROM bootloader
flash: flash-dfu-util
