DEPS_SUBMODULES += hw/mcu/renesas/rx

# Cross Compiler for RX
CROSS_COMPILE = rx-elf-

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -nostartfiles \
  -ffunction-sections \
  -fdata-sections \
  -fshort-enums \
  -mlittle-endian-data \

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

SRC_C += \
	src/portable/renesas/usba/dcd_usba.c \
	$(MCU_DIR)/vects.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)

SRC_S += $(MCU_DIR)/start.S

$(BUILD)/$(PROJECT).mot: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	$(OBJCOPY) -O srec -I elf32-rx-be-ns $^ $@

# flash using rfp-cli
flash-rfp: $(BUILD)/$(PROJECT).mot
	rfp-cli -device rx65x -tool e2l -if fine -fo id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -auto $^
