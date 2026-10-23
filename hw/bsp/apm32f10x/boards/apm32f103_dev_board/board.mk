MCU_VARIANT = apm32f103xb
MCU_LINKER_NAME = apm32f103xb

JLINK_DEVICE = APM32F103VB

CFLAGS += \
  -DAPM32F103xB \
  -DCFG_TUSB_MCU=OPT_MCU_APM32F103 \
  -DHSE_VALUE=8000000U \
  -DCFG_EXAMPLE_VIDEO_READONLY

SRC_C += \
  src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
  src/portable/st/stm32_fsdev/fsdev_common.c
