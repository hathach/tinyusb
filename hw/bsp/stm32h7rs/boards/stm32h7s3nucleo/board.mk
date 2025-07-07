MCU_VARIANT = stm32h7s3xx
CFLAGS += -DSTM32H7S3xx

# For flash-jlink target
JLINK_DEVICE = stm32h7s3xx

# flash target using on-board stlink
flash: flash-stlink

SRC_C += \
	$(ST_TCPP0203)/tcpp0203.c \
	$(ST_TCPP0203)/tcpp0203_reg.c \

INC += \
	$(TOP)/$(ST_TCPP0203) \
