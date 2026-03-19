UF2_FAMILY_ID = 0x4fb2d5bd
MCUX_CORE = hw/mcu/nxp/mcuxsdk-core
MCUX_DEVICES = hw/mcu/nxp/mcux-devices-rt

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m7
MCU_VARIANT_WITH_CORE = ${MCU_VARIANT}${MCU_CORE}
MCU_DIR = $(MCUX_DEVICES)/$(MCU_FAMILY)/$(MCU_VARIANT)

# XIP boot files: some devices reference RT1052's xip (see each device's xip/CMakeLists.txt)
ifeq ($(MCU_FAMILY),RT1064)
  XIP_DIR = $(MCUX_DEVICES)/RT1064/MIMXRT1064/xip
else ifeq ($(MCU_FAMILY),RT1170)
  XIP_DIR = $(MCUX_DEVICES)/RT1170/MIMXRT1176/xip
else
  # RT1010, RT1015, RT1020, RT1050, RT1060 all use RT1052's xip
  XIP_DIR = $(MCUX_DEVICES)/RT1050/MIMXRT1052/xip
endif

CFLAGS += \
	-D__START=main \
  -D__STARTUP_CLEAR_BSS \
  -DCFG_TUSB_MCU=OPT_MCU_MIMXRT1XXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section("NonCacheable")))' \

ifneq ($(M4), 1)
CFLAGS += \
  -DXIP_EXTERNAL_FLASH=1 \
  -DXIP_BOOT_HEADER_ENABLE=1
endif

ifdef BOARD_TUD_RHPORT
CFLAGS += -DBOARD_TUD_RHPORT=$(BOARD_TUD_RHPORT)
endif

ifdef BOARD_TUH_RHPORT
CFLAGS += -DBOARD_TUH_RHPORT=$(BOARD_TUH_RHPORT)
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=implicit-fallthrough -Wno-error=redundant-decls

LDFLAGS_GCC += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE ?= $(MCU_DIR)/gcc/$(MCU_VARIANT)xxxxx${MCU_CORE}_flexspi_nor.ld

# TODO for net_lwip_webserver example, but may not needed !!
LDFLAGS += \
	-Wl,--defsym,__stack_size__=0x800 \

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	src/portable/chipidea/ci_hs/hcd_ci_hs.c \
	src/portable/ehci/ehci.c \
	${BOARD_PATH}/board/clock_config.c \
	${BOARD_PATH}/board/pin_mux.c \
	$(MCU_DIR)/system_$(MCU_VARIANT_WITH_CORE).c \
	$(XIP_DIR)/fsl_flexspi_nor_boot.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCUX_CORE)/drivers/common/fsl_common.c \
	$(MCUX_CORE)/drivers/common/fsl_common_arm.c \
	$(MCUX_CORE)/drivers/igpio/fsl_gpio.c \
	$(MCUX_CORE)/drivers/lpuart/fsl_lpuart.c \
	$(MCUX_CORE)/drivers/ocotp/fsl_ocotp.c \

# Optional drivers: RT1170 power/anatop_ai subdirectories
ifneq (,$(wildcard ${TOP}/${MCU_DIR}/drivers/power/fsl_dcdc.c))
SRC_C += \
  ${MCU_DIR}/drivers/power/fsl_dcdc.c \
  ${MCU_DIR}/drivers/power/fsl_pmu.c \
  ${MCU_DIR}/drivers/anatop_ai/fsl_anatop_ai.c
INC += \
  $(TOP)/$(MCU_DIR)/drivers/power \
  $(TOP)/$(MCU_DIR)/drivers/anatop_ai
endif

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(MCUX_DEVICES)/$(MCU_FAMILY)/periph \
	$(TOP)/$(MCUX_CORE)/drivers/common \
	$(TOP)/$(MCUX_CORE)/drivers/igpio \
	$(TOP)/$(MCUX_CORE)/drivers/lpuart \
	$(TOP)/$(MCUX_CORE)/drivers/ocotp \
	$(TOP)/$(XIP_DIR) \

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU_VARIANT_WITH_CORE).S

# UF2 generation, iMXRT need to strip to text only before conversion
APPLICATION_ADDR = 0x6000C000
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary -R .flash_config -R .ivt $^ $(BUILD)/$(PROJECT)-textonly.bin
	$(PYTHON) $(TOP)/tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -b $(APPLICATION_ADDR) -c -o $@ $(BUILD)/$(PROJECT)-textonly.bin
