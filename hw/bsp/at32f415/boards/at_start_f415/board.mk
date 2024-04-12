CFLAGS += \
 -DAT32F415RCT7 \
 -DHEXT_VALUE=8000000U

# Linker
LD_FILE_GCC = $(BOARD_PATH)/ld/AT32F415xC_FLASH.ld

# For flash-jlink target
JLINK_DEVICE = at32f415rct7-7

# flash target ROM bootloader
flash: flash-jlink
