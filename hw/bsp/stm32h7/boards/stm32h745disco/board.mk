# STM32H745I-DISCO uses OTG_FS
# FIXME: Reset enumerates, un/replug USB plug does not enumerate

CFLAGS += -DSTM32H745xx -DCORE_CM7 -DHSE_VALUE=25000000

# Default is FulSpeed port
PORT ?= 0

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h745xx.s
LD_FILE = $(ST_CMSIS)/Source/Templates/gcc/linker/stm32h745xx_flash_CM7.ld

# For flash-jlink target
JLINK_DEVICE = stm32h745xi_m7

# flash target using on-board stlink
flash: flash-stlink

