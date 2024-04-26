CFLAGS += \
	-DHSE_VALUE=8000000

LD_FILE = $(BOARD_PATH)/flash.ld
SRC_S += $(SDK_DIR)/mm32f327x/MM32F327x/Source/GCC_StartAsm/startup_mm32m3ux_u_gcc.S


# For flash-jlink target
#JLINK_DEVICE = stm32f411ve

# flash target using on-board stlink
#flash: flash-jlink
