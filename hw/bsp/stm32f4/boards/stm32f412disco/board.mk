CFLAGS += -DSTM32F412Zx

# GCC
GCC_SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f412zx.s
GCC_LD_FILE = $(BOARD_PATH)/STM32F412ZGTx_FLASH.ld

# IAR
IAR_SRC_S += $(ST_CMSIS)/Source/Templates/iar/startup_stm32f412zx.s
IAR_LD_FILE = $(ST_CMSIS)/Source/Templates/iar/linker/stm32f412zx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32f412zg

# flash target using on-board stlink
flash: flash-stlink
