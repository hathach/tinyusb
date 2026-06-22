ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=cortex-a53 \

endif
