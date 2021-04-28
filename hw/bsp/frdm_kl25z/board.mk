SDK_DIR = hw/mcu/nxp/nxp_sdk
DEPS_SUBMODULES += $(SDK_DIR)

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCPU_MKL25Z128VLK4 \
  -DCFG_TUSB_MCU=OPT_MCU_MKL25ZXX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

MCU_DIR = $(SDK_DIR)/devices/MKL25Z4

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/MKL25Z128xxx4_flash.ld

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	$(MCU_DIR)/system_MKL25Z4.c \
	$(MCU_DIR)/project_template/clock_config.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_lpsci.c

INC += \
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(SDK_DIR)/CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(MCU_DIR)/project_template \

SRC_S += $(MCU_DIR)/gcc/startup_MKL25Z4.S

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = MKL25Z128xxx4

# For flash-pyocd target
PYOCD_TARGET = mkl25zl128

# flash using pyocd
flash: flash-pyocd
