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

$(BUILD)/$(PROJECT).mot: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	$(OBJCOPY) -O srec -I elf32-rx-be-ns $^ $@
