ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=arm1176jzf-s \

else ifeq ($(TOOLCHAIN),iar)
	#CFLAGS += --cpu cortex-a53
	#ASFLAGS += --cpu cortex-a53

endif
