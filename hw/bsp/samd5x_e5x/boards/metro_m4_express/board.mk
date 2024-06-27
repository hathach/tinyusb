SAM_FAMILY = samd51

CFLAGS += -D__SAMD51N19A__

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/$(BOARD).ld

# For flash-jlink target
# JLINK_DEVICE = ATSAMD51J19
# Updated by Nikhil to allow spoofing of this file by DeVito BUCB
JLINK_DEVICE = ATSAMD51N19

flash: flash-bossac
