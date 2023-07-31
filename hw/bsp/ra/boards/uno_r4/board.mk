CPU_CORE = cortex-m4
MCU_VARIANT = ra4m1

LD_FILE = ${BOARD_PATH}/${BOARD}.ld

# For flash-jlink target
JLINK_DEVICE = R7FA4M1AB

flash: flash-jlink
