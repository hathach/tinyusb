UF2_FAMILY_ID = 0x4fb2d5bd
SDK_DIR = hw/mcu/nxp/mcux-sdk
DEPS_SUBMODULES += $(SDK_DIR) lib/CMSIS_5

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m7

CFLAGS += \
  -D__ARMVFP__=0 \
  -D__ARMFPV5__=0 \
  -DXIP_EXTERNAL_FLASH=1 \
  -DXIP_BOOT_HEADER_ENABLE=1 \
  -DCFG_TUSB_MCU=OPT_MCU_MIMXRT1XXX

ifdef BOARD_TUD_RHPORT
CFLAGS += -DBOARD_TUD_RHPORT=$(BOARD_TUD_RHPORT)
endif

ifdef BOARD_TUH_RHPORT
CFLAGS += -DBOARD_TUH_RHPORT=$(BOARD_TUH_RHPORT)
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=implicit-fallthrough -Wno-error=redundant-decls

MCU_DIR = $(SDK_DIR)/devices/$(MCU_VARIANT)

# All source paths should be relative to the top level.
LD_FILE ?= $(MCU_DIR)/gcc/$(MCU_VARIANT)xxxxx_flexspi_nor.ld

# TODO for net_lwip_webserver example, but may not needed !!
LDFLAGS += \
	-Wl,--defsym,__stack_size__=0x800 \

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	src/portable/chipidea/ci_hs/hcd_ci_hs.c \
	src/portable/ehci/ehci.c \
	$(MCU_DIR)/system_$(MCU_VARIANT).c \
	$(MCU_DIR)/xip/fsl_flexspi_nor_boot.c \
	$(MCU_DIR)/project_template/clock_config.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(SDK_DIR)/drivers/common/fsl_common.c \
	$(SDK_DIR)/drivers/igpio/fsl_gpio.c \
	$(SDK_DIR)/drivers/lpuart/fsl_lpuart.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/project_template \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(SDK_DIR)/drivers/common \
	$(TOP)/$(SDK_DIR)/drivers/igpio \
	$(TOP)/$(SDK_DIR)/drivers/lpuart

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU_VARIANT).S

# UF2 generation, iMXRT need to strip to text only before conversion
APPLICATION_ADDR = 0x6000C000
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary -R .flash_config -R .ivt $^ $(BUILD)/$(PROJECT)-textonly.bin
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -b $(APPLICATION_ADDR) -c -o $@ $(BUILD)/$(PROJECT)-textonly.bin
