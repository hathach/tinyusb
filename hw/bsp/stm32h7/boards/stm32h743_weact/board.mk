# STM32H743I-WEACT uses OTG_FS
# FIXME: Reset enumerates, un/replug USB plug does not enumerate
MCU_VARIANT = stm32h743xx
CFLAGS += -DSTM32H743xx -DHSE_VALUE=25000000

LD_FILE_GCC = $(FAMILY_PATH)/linker/${MCU_VARIANT}_flash.ld

# For flash-jlink target
JLINK_DEVICE = stm32h743xi

# flash target using on-board stlink
flash: flash-jlink
