CFLAGS += -DSTM32F401xC

LD_FILE = $(BOARD_PATH)/STM32F401VCTx_FLASH.ld

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f401xc.s

# For flash-jlink target
JLINK_DEVICE = stm32f401cc

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
