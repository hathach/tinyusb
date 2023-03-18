SDK_DIR = hw/mcu/nxp/mcux-sdk
MCU = K32L2A41A
MCU_DIR = $(SDK_DIR)/devices/$(MCU)

CFLAGS += \
  -mcpu=cortex-m0plus \
  -DCPU_K32L2A41VLH1A \
  -DCFG_TUSB_MCU=OPT_MCU_K32L2AXX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=redundant-decls -Wno-error=cast-qual

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/K32L2A41xxxxA_flash.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

# For flash-jlink target
JLINK_DEVICE = K32L2A41xxxxA

# For flash-pyocd target
PYOCD_TARGET = K32L2A

# flash using pyocd
flash: flash-pyocd
