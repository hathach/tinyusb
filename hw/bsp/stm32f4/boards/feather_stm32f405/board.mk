CFLAGS += -DSTM32F405xx

# GCC
SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32f405xx.s
LD_FILE = $(BOARD_PATH)/STM32F405RGTx_FLASH.ld


# For flash-jlink target
JLINK_DEVICE = stm32f405rg

# flash target ROM bootloader
flash: $(BUILD)/$(PROJECT).bin
	dfu-util -R -a 0 --dfuse-address 0x08000000 -D $<
