CFLAGS += \
	-DHSE_VALUE=8000000

JLINK_DEVICE = MM32F3273G9P

LD_FILE = $(BOARD_PATH)/flash.ld
SRC_S += $(SDK_DIR)/mm32f327x/MM32F327x/Source/GCC_StartAsm/startup_mm32m3ux_u_gcc.S

flash: flash-jlink
