MCU_VARIANT = RW612
MCU_CORE = RW612

CPU_CORE = cortex-m33-nodsp-nofp
CFLAGS += \
	-DCPU_RW612ETA2I \
	-DCFG_TUSB_MCU=OPT_MCU_RW61X \
	-DBOOT_HEADER_ENABLE=1 \

JLINK_DEVICE = ${MCU_VARIANT}
PYOCD_TARGET = rw612eta2i

SRC_C += \
	$(SDK_DIR)/boards/frdmrw612/flash_config/flash_config.c \

INC += \
	$(TOP)/$(SDK_DIR)/boards/frdmrw612/flash_config/ \

# flash using pyocd
flash: flash-jlink
