MCU_VARIANT = stm32h7s3xx
CFLAGS += -DSTM32H7S3xx

# For flash-jlink target
JLINK_DEVICE = stm32h7s3xx

# flash target using on-board stlink
flash: flash-stlink

SRC_C += \
	$(BOARD_PATH)/tcpp0203/tcpp0203.c \
	$(BOARD_PATH)/tcpp0203/tcpp0203_reg.c \

INC += \
	$(BOARD_PATH)/tcpp0203 \
