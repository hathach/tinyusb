MCU_VARIANT = stm32h7s3xx
CFLAGS += -DSTM32H7S3xx

# For flash-jlink target
JLINK_DEVICE = stm32h7s3l8

# Run from RAM, image on the MX25UW25645G octal flash (XSPIM_P2)
XSPI_BOOT = 1
STM32_PROGRAMMER_EXTLOADER = $(shell dirname "$$(which STM32_Programmer_CLI)")/ExternalLoader/MX25UW25645G_NUCLEO-H7S3L8.stldr

# flash target using on-board stlink
flash: flash-stlink

SRC_C += \
	$(ST_TCPP0203)/tcpp0203.c \
	$(ST_TCPP0203)/tcpp0203_reg.c \

INC += \
	$(TOP)/$(ST_TCPP0203) \
