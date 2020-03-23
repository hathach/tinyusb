CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -D__ARMVFP__=0 -D__ARMFPV5__=0\
  -DCPU_MIMXRT1064DVL6A \
  -DXIP_EXTERNAL_FLASH=1 \
  -DXIP_BOOT_HEADER_ENABLE=1 \
  -DCFG_TUSB_MCU=OPT_MCU_MIMXRT10XX

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

MCU_DIR = hw/mcu/nxp/sdk/devices/MIMXRT1064

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/MIMXRT1064xxxxx_flexspi_nor.ld

SRC_C += \
	$(MCU_DIR)/system_MIMXRT1064.c \
	$(MCU_DIR)/xip/fsl_flexspi_nor_boot.c \
	$(MCU_DIR)/project_template/clock_config.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_common.c \
	$(MCU_DIR)/drivers/fsl_lpuart.c

INC += \
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(MCU_DIR)/project_template \

SRC_S += $(MCU_DIR)/gcc/startup_MIMXRT1064.S

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = transdimension

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7/r0p1

# For flash-jlink target
JLINK_DEVICE = MIMXRT1064xxx6A
JLINK_IF = swd

# flash by copying bin file to DAP Mass Storage
flash: $(BUILD)/$(BOARD)-firmware.bin
	cp $< /media/$(USER)/RT1064-EVK/
