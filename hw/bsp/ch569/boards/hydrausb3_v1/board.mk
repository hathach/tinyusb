# 16KB RAM cannot hold two 8KB RAM disks: use read-only disks in flash
CFLAGS += -DCFG_EXAMPLE_MSC_DUAL_READONLY

LDFLAGS += \
	-Wl,--defsym=__FLASH_SIZE=448K \
	-Wl,--defsym=__RAM_SIZE=16K
