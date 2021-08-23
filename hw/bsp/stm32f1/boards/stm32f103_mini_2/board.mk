CFLAGS += -DSTM32F103xB -DHSE_VALUE=8000000U

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/STM32F103XC_FLASH.ld
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f103xb.s

# For flash-jlink target
JLINK_DEVICE = stm32f103rc

# flash target ROM bootloader
flash: flash-jlink
