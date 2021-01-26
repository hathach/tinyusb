CFLAGS += -DSTM32F411xE

LD_FILE = $(BOARD_PATH)/STM32F411CEUx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f411xe.s

# For flash-jlink target
JLINK_DEVICE = stm32f411ce

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
