DEPS_SUBMODULES += hw/mcu/ti

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m4

CFLAGS += \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_TM4C123 \
  -uvectors \
  -DTM4C123GH6PM

# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=cast-qual

MCU_DIR=hw/mcu/ti/tm4c123xx/

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/tm4c123.ld

INC += \
	$(TOP)/$(MCU_DIR)/CMSIS/5.7.0/CMSIS/Include \
	$(TOP)/$(MCU_DIR)/Include/TM4C123 \
	$(TOP)/$(BOARD_PATH)

SRC_C += \
	src/portable/mentor/musb/dcd_musb.c \
	src/portable/mentor/musb/hcd_musb.c \
	$(MCU_DIR)/Source/system_TM4C123.c \
	$(MCU_DIR)/Source/GCC/tm4c123_startup.c
