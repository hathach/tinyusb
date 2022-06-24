DEPS_SUBMODULES += hw/mcu/microchip

CFLAGS += \
  -mthumb \
  -mcpu=cortex-m3 \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -D__ATSAM3U2C__ \
  -D__SAM3U2C__ \
  -DCFG_TUSB_MCU=OPT_MCU_SAM3U

# suppress following warnings from mcu driver
# CFLAGS += -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=cast-qual

DEPS_SUBMODULES += hw/mcu/microchip
DEPS_SUBMODULES += lib/CMSIS_5

MCU_DIR = hw/mcu/microchip/sam3u

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/gcc/sam3u2c_flash.ld
LDFLAGS += -L"$(TOP)/$(MCU_DIR)/gcc/gcc/"

SRC_C += \
	src/portable/microchip/sam3u/dcd_samhs.c \
	$(MCU_DIR)/gcc/gcc/startup_sam3u.c \
	$(MCU_DIR)/gcc/system_sam3u.c

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR)/include \
	$(TOP)/hw/bsp/$(BOARD)

# For freeRTOS port source
FREERTOS_PORT = ARM_CM3

# For flash-jlink target
JLINK_DEVICE = ATSAM3U2C

flash: flash-jlink
