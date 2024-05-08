ifeq ($(TOOLCHAIN),gcc)
  CFLAGS += \
    -march=rv32imac \
    -mabi=ilp32 \

else ifeq ($(TOOLCHAIN),iar)
	#CFLAGS += --cpu cortex-a53
	#ASFLAGS += --cpu cortex-a53

endif

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
