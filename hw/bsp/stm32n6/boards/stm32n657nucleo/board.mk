MCU_VARIANT = stm32n6xx
CFLAGS += -DSTM32N6xx

# For flash-jlink target
JLINK_DEVICE = stm32n6xx

# flash target using on-board stlink
flash: flash-stlink

PORT = 1


SRC_C += \
	$(BOARD_PATH)/tcpp0203/tcpp0203.c \
	$(BOARD_PATH)/tcpp0203/tcpp0203_reg.c \

INC += \
	$(TOP)/$(BOARD_PATH)/tcpp0203 \

CFLAGS += \
	-DSEGGER_RTT_SECTION=\"noncacheable_buffer\" \
	-DSTM32N657xx
	-DBUFFER_SIZE_UP=0x3000 \
