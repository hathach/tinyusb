CPU_CORE = cortex-m0
MCU_VARIANT = s124
MCU_VARIANT_UPPERCASE = S124

# 16 KB RAM: keep example disks and video frames in flash
CFLAGS += \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_EXAMPLE_VIDEO_READONLY

# For flash-jlink target
JLINK_DEVICE = R7FS124773A01CFM

flash: flash-jlink
