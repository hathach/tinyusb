MCU_VARIANT = stm32h573xx

CFLAGS += \
	-DSTM32H573xx

# For flash-jlink target
JLINK_DEVICE = stm32h573ii

SRC_C += \
	$(ST_TCPP0203)/tcpp0203.c \
	$(ST_TCPP0203)/tcpp0203_reg.c \

INC += \
	$(TOP)/$(ST_TCPP0203) \
