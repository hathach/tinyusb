MCU_VARIANT = D6

CFLAGS += -DCFG_EXAMPLE_MSC_DUAL_READONLY

LDFLAGS += \
  -Wl,--defsym=__flash_size=64K \
  -Wl,--defsym=__ram_size=20K \
