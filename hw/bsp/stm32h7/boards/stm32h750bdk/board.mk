# STM32H745I-DISCO uses OTG_FS
# FIXME: Reset enumerates, un/replug USB plug does not enumerate

CFLAGS += -DSTM32H750xx -DCORE_CM7 -DHSE_VALUE=25000000

# Default is FulSpeed port
PORT ?= 0

# GCC
SRC_S_GCC += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h750xx.s
LD_FILE_GCC = $(BOARD_PATH)/stm32h750xx_flash_CM7.ld

# IAR
SRC_S_IAR += $(ST_CMSIS)/Source/Templates/iar/startup_stm32h750xx.s
LD_FILE_IAR = $(ST_CMSIS)/Source/Templates/iar/linker/stm32h750xx_flash.icf

# For flash-jlink target
JLINK_DEVICE = stm32h750xb_m7

# flash target using on-board stlink
flash: flash-stlink
