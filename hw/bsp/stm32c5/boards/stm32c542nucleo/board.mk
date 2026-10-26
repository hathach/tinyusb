MCU_VARIANT = stm32c542xx

CFLAGS += \
	-DSTM32C542xx \
	-DHSE_VALUE=24000000 \
	-DHSE_STARTUP_TIMEOUT=100

# GCC
LD_FILE = $(FAMILY_PATH)/linker/stm32c542xc_flash.ld


# For flash-jlink target
JLINK_DEVICE = stm32c542rc
