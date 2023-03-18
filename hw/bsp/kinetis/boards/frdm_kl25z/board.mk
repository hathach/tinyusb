SDK_DIR = hw/mcu/nxp/nxp_sdk
MCU = MKL25Z4
MCU_DIR = $(SDK_DIR)/devices/$(MCU)

CFLAGS += \
  -mcpu=cortex-m0plus \
  -DCPU_MKL25Z128VLK4 \
  -DCFG_TUSB_MCU=OPT_MCU_MKL25ZXX \
  -DCFG_EXAMPLE_VIDEO_READONLY

LDFLAGS += \
  -Wl,--defsym,__stack_size__=0x400 \
  -Wl,--defsym,__heap_size__=0

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=format -Wno-error=redundant-decls

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/MKL25Z128xxx4_flash.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

# For flash-jlink target
JLINK_DEVICE = MKL25Z128xxx4

# For flash-pyocd target
PYOCD_TARGET = mkl25zl128

# flash using pyocd
flash: flash-jlink
