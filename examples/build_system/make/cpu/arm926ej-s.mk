ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -mcpu=arm926ej-s \

else ifeq ($(TOOLCHAIN),iar)
	#CFLAGS += --cpu cortex-a53
	#ASFLAGS += --cpu cortex-a53

endif
