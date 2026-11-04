CFLAGS += -DLWIP_HIGH_THROUGHPUT=0

LDFLAGS += \
  -Wl,--defsym=__FLASH_SIZE=128K \
  -Wl,--defsym=__RAM_SIZE=32K \
