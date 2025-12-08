SDK_DIR = hw/mcu/hpmicro/hpm_sdk

CROSS_COMPILE ?= riscv32-unknown-elf-

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= rv32imac-ilp32

# All source paths should be relative to the top level.
LD_FILE = $(HPM_SOC)/toolchains/gcc/flash_xip.ld

CFLAGS += \
  -DFLASH_XIP \
  -DCFG_TUSB_MCU=OPT_MCU_HPM \
  -DCFG_TUD_MEM_SECTION='__attribute__((section(".noncacheable.non_init")))' \
  -DCFG_TUH_MEM_SECTION='__attribute__((section(".noncacheable.non_init")))'

ifdef BOARD_TUD_RHPORT
CFLAGS += -DBOARD_TUD_RHPORT=$(BOARD_TUD_RHPORT)
CFLAGS += -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED
endif

ifdef BOARD_TUH_RHPORT
CFLAGS += -DBOARD_TUH_RHPORT=$(BOARD_TUH_RHPORT)
CFLAGS += -DBOARD_TUH_MAX_SPEED=OPT_MODE_HIGH_SPEED
endif

# mcu driver cause following warnings
CFLAGS += -Wno-error=cast-align -Wno-error=double-promotion -Wno-error=discarded-qualifiers \
          -Wno-error=undef -Wno-error=unused-parameter -Wno-error=redundant-decls

LDFLAGS_GCC += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs

LDFLAGS += -Wl,--defsym,_flash_size=$(BOARD_FLASH_SIZE)
LDFLAGS += -Wl,--defsym,_stack_size=$(BOARD_STACK_SIZE)
LDFLAGS += -Wl,--defsym,_heap_size=$(BOARD_HEAP_SIZE)

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	src/portable/chipidea/ci_hs/hcd_ci_hs.c \
	src/portable/ehci/ehci.c \
	${BOARD_PATH}/board.c \
	${BOARD_PATH}/pinmux.c \
	$(HPM_SOC)/boot/hpm_bootheader.c \
	$(HPM_SOC)/toolchains/gcc/initfini.c \
	$(HPM_SOC)/toolchains/reset.c \
	$(HPM_SOC)/toolchains/trap.c \
	$(HPM_SOC)/system.c \
	$(HPM_SOC)/hpm_sysctl_drv.c \
	$(HPM_SOC)/hpm_clock_drv.c \
	$(HPM_SOC)/hpm_otp_drv.c \
	$(SDK_DIR)/arch/riscv/l1c/hpm_l1c_drv.c \
	$(SDK_DIR)/utils/hpm_sbrk.c \
	$(SDK_DIR)/drivers/src/hpm_gpio_drv.c \
	$(SDK_DIR)/drivers/src/hpm_uart_drv.c \
	$(SDK_DIR)/drivers/src/hpm_usb_drv.c \
	$(SDK_DIR)/drivers/src/hpm_pcfg_drv.c \
	$(SDK_DIR)/drivers/src/hpm_pmp_drv.c \
	$(SDK_DIR)/drivers/src/$(HPM_PLLCTL_DRV_FILE) \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/src/portable/chipidea/ci_hs \
	$(TOP)/$(HPM_SOC) \
	$(TOP)/$(HPM_IP_REGS) \
	$(TOP)/$(HPM_SOC)/boot \
	$(TOP)/$(HPM_SOC)/toolchains \
	$(TOP)/$(HPM_SOC)/toolchains/gcc \
	$(TOP)/$(SDK_DIR)/arch \
	$(TOP)/$(SDK_DIR)/arch/riscv/intc \
	$(TOP)/$(SDK_DIR)/arch/riscv/l1c \
	$(TOP)/$(SDK_DIR)/drivers/inc \

SRC_S += $(HPM_SOC)/toolchains/gcc/start.S
