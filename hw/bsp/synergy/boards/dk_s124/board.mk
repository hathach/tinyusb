CPU_CORE = cortex-m0
MCU_VARIANT = s124
MCU_VARIANT_UPPERCASE = S124

# 16 KB RAM: keep example disks and video frames in flash
CFLAGS += \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_EXAMPLE_VIDEO_READONLY

LD_FILE = $(BOARD_PATH)/script/r7fs124773a01cfm.ld

LIBS += \
  $(TOP)/$(RENESAS_SSP_PATH)/src/driver/r_fmi/libs/libfmi_cm0_$(MCU_VARIANT)_gcc.a \
  $(TOP)/$(RENESAS_SSP_PATH)/src/bsp/mcu/$(MCU_VARIANT)/libfmi_R7FS124773A01CFM_gcc.a

# For flash-jlink target
JLINK_DEVICE = R7FS124773A01CFM

flash: flash-jlink
