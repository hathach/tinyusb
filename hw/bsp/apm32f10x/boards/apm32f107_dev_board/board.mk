MCU_VARIANT = apm32f107xc
MCU_LINKER_NAME = apm32f107xc

JLINK_DEVICE = APM32F107VC

CFLAGS += \
  -DAPM32F107xC \
  -DCFG_TUSB_MCU=OPT_MCU_APM32F107 \
  -DHSE_VALUE=25000000U \
  -DCFG_EXAMPLE_VIDEO_READONLY

SRC_C += \
  src/portable/synopsys/dwc2/dcd_dwc2.c \
  src/portable/synopsys/dwc2/hcd_dwc2.c \
  src/portable/synopsys/dwc2/dwc2_common.c
