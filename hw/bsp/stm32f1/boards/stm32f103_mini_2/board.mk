CFLAGS += -DSTM32F103xB -DHSE_VALUE=8000000U

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f103xb.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F103XC_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f103xb.s
IAR_LD_FILE = $(BOARD_PATH)/stm32f103xc_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f103rc

# flash target ROM bootloader
flash: flash-jlink
