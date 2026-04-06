ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=cortex-a72 \

endif
