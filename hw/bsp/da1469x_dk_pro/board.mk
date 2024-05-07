MCU_FAMILY_DIR = hw/mcu/dialog/da1469x

CFLAGS += \
  -flto \
  -mthumb \
  -mthumb-interwork \
  -mabi=aapcs \
  -mcpu=cortex-m33+nodsp \
  -mfloat-abi=hard \
  -mfpu=fpv5-sp-d16 \
  -nostdlib \
  -DCORE_M33 \
  -DCFG_TUSB_MCU=OPT_MCU_DA1469X \
  -DCFG_TUD_ENDPOINT0_SIZE=8\

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/da1469x.ld

# While this is for da1469x chip, there is chance that da1468x chip family will also work
SRC_C += \
	src/portable/dialog/da146xx/dcd_da146xx.c \
	$(MCU_FAMILY_DIR)/src/system_da1469x.c \
	$(MCU_FAMILY_DIR)/src/da1469x_clock.c \
	$(MCU_FAMILY_DIR)/src/hal_gpio.c \

SRC_S += hw/bsp/$(BOARD)/gcc_startup_da1469x.S

INC += \
	$(TOP)/hw/bsp/$(BOARD) \
	$(TOP)/$(MCU_FAMILY_DIR)/include \
	$(TOP)/$(MCU_FAMILY_DIR)/SDK_10.0.8.105/sdk/bsp/include

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM33_NTZ/non_secure

# For flash-jlink target
JLINK_DEVICE = DA14699

# flash using jlink but with some twists
flash: flash-dialog

flash-dialog: $(BUILD)/$(PROJECT).bin
	@echo '#define SW_VERSION "v_1.0.0.1"' >$(BUILD)/version.h
	@echo '#define SW_VERSION_DATE "'`date +"%Y-%m-%d %H:%M"`'"' >>$(BUILD)/version.h
	mkimage da1469x $(BUILD)/$(PROJECT).bin $(BUILD)/version.h $^.img
	cp $(TOP)/hw/bsp/$(BOARD)/product_header.dump $(BUILD)/$(BOARD)-image.bin
	cat $^.img >> $(BUILD)/$(BOARD)-image.bin
	@echo r > $(BUILD)/$(BOARD).jlink
	@echo halt >> $(BUILD)/$(BOARD).jlink
	@echo loadfile $(BUILD)/$(BOARD)-image.bin 0x16000000 >> $(BUILD)/$(BOARD).jlink
	@echo r >> $(BUILD)/$(BOARD).jlink
	@echo go >> $(BUILD)/$(BOARD).jlink
	@echo exit >> $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD)/$(BOARD).jlink
