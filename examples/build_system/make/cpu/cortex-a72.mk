ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=cortex-a72 \

else ifeq ($(TOOLCHAIN),iar)
	CFLAGS += \
		--cpu cortex-a72 \

	ASFLAGS += \
		--cpu cortex-a72 \

endif
