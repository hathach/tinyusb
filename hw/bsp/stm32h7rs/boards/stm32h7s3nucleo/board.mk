MCU_VARIANT = stm32h7s3xx
CFLAGS += -DSTM32H7S3xx

# For flash-jlink target
JLINK_DEVICE = stm32h7s3xx

# flash target using on-board stlink
flash: flash-stlink

# Linker
LD_FILE_GCC = $(BOARD_PATH)/stm32h7s3xx_flash.ld
LD_FILE_IAR = $(BOARD_PATH)/stm32h7s3xx_flash.icf

SRC_C += \
	$(BOARD_PATH)/tcpp0203/tcpp0203.c \
	$(BOARD_PATH)/tcpp0203/tcpp0203_reg.c \

INC += \
	$(TOP)/$(BOARD_PATH)/tcpp0203 \

CFLAGS += \
	-DSEGGER_RTT_SECTION=\"noncacheable_buffer\" \
	-DBUFFER_SIZE_UP=0x3000 \
