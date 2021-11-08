CFLAGS += -DTM4C123GH6PM
 
LD_FILE = $(BOARD_PATH)/tm4c123.ld

# For flash-jlink target
JLINK_DEVICE = TM4C123GH6PM

# flash using openocd
OPENOCD_OPTION = -f board/ti_ek-tm4c123gxl.cfg

flash: flash-openocd
