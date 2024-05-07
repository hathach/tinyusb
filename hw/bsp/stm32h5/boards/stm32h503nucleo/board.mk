MCU_VARIANT = stm32h503xx

CFLAGS += \
	-DSTM32H503xx

# For flash-jlink target
JLINK_DEVICE = stm32h503rb
