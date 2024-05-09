ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=cortex-a53 \

else ifeq ($(TOOLCHAIN),iar)
	CFLAGS += \
		--cpu cortex-a53 \

	ASFLAGS += \
		--cpu cortex-a53 \

endif
