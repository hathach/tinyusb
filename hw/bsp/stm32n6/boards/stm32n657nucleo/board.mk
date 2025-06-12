MCU_VARIANT = stm32n657xx
CFLAGS += -DSTM32N657xx
JLINK_DEVICE = stm32n6xx

LD_FILE_GCC = $(BOARD_PATH)/STM32N657XX_AXISRAM2_fsbl.ld

# flash target using on-board stlink
flash: flash-stlink

PORT = 1

SRC_C += \
	$(BOARD_PATH)/tcpp0203/tcpp0203.c \
	$(BOARD_PATH)/tcpp0203/tcpp0203_reg.c \

INC += \
	$(TOP)/$(BOARD_PATH)/tcpp0203 \
