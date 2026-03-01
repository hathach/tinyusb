MCU_SUB_VARIANT = 129

CFLAGS += -DTM4C1294NCPDT

LD_FILE_GCC = $(BOARD_PATH)/tm4c1294nc.ld
LD_FILE_IAR = $(BOARD_PATH)/TM4C1294NC.icf

# For flash-jlink target
JLINK_DEVICE = TM4C1294NCPDT

# flash using openocd
OPENOCD_OPTION = -f board/ti_ek-tm4c1294xl.cfg

UNIFLASH_OPTION = -c ${TOP}/${BOARD_PATH}/${BOARD}.ccxml -r 1

flash: flash-openocd
