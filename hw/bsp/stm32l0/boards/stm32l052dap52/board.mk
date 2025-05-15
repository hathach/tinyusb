MCU_VARIANT = stm32l052xx
CFLAGS += \
	-DSTM32L052xx

LD_FILE = $(BOARD_PATH)/STM32L052K8Ux_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = stm32l052k8

# flash target using on-board stlink
flash: flash-stlink
