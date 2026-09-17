CFLAGS += -D__SAME70N19B__

# N19B shares Q21B's startup code but not its MEMORY sizes (512K/256K vs 2048K/384K),
# and the SDK ships no N19B linker script, so the board carries its own.
LD_FILE = $(BOARD_PATH)/same70n19b_flash.ld
STARTUP_FILE = $(SDK_DIR)/same70b/gcc/gcc/startup_same70q21b.c

JLINK_DEVICE = ATSAME70N19B
