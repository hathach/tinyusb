DEPS_SUBMODULES += hw/mcu/renesas/rx

CFLAGS += \
  -nostartfiles \
  -ffunction-sections \
  -fdata-sections \
  -fshort-enums \
  -mcpu=rx610 \
  -misa=v1 \
  -mlittle-endian-data \
  -DCFG_TUSB_MCU=OPT_MCU_RX63X

# Cross Compiler for RX
CROSS_COMPILE = rx-elf-

RX_NEWLIB ?= 1

ifeq ($(CMDEXE),1)
OPTLIBINC="$(shell for /F "usebackq delims=" %%i in (`where rx-elf-gcc`) do echo %%~dpi..\rx-elf\optlibinc)"
else
OPTLIBINC=$(shell dirname `which rx-elf-gcc`)../rx-elf/optlibinc
endif

ifeq ($(RX_NEWLIB),1)
CFLAGS += -DSSIZE_MAX=__INT_MAX__
else
# setup for optlib
CFLAGS += -nostdinc \
  -isystem $(OPTLIBINC) \
  -DLWIP_NO_INTTYPES_H

LIBS += -loptc -loptm
endif

MCU_DIR = hw/mcu/renesas/rx/rx63n

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/r5f5631fd.ld

SRC_C += \
	src/portable/renesas/usba/dcd_usba.c \
	$(MCU_DIR)/vects.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)

SRC_S += $(MCU_DIR)/start.S

# For freeRTOS port source
FREERTOS_PORT = RX600

# For flash-jlink target
JLINK_DEVICE = R5F5631F
JLINK_IF     = JTAG

# For flash-pyocd target
PYOCD_TARGET =

# flash using jlink
flash: flash-jlink
