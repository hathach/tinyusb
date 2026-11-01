# Toolchain from https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
CROSS_COMPILE ?= riscv-none-elf-

SDK_DIR = hw/mcu/wch/ch569/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= rv32imac-ilp32

# Controller selection: super = USB3.0 SuperSpeed (USBSS), high = USB2.0 HighSpeed (USBHS)
SPEED ?= super
FREQ_SYS ?= 120000000

CFLAGS += \
	-flto \
	-msmall-data-limit=16 \
	-mno-save-restore \
	-fmessage-length=0 \
	-fsigned-char \
	-DCFG_TUSB_MCU=OPT_MCU_CH569 \
	-DFREQ_SYS=$(FREQ_SYS) \
	-DCFG_TUSB_MEM_SECTION='__attribute__((section(".dmadata")))' \
	-DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(16)))' \
	-Wno-error=strict-prototypes \
	-Wno-comment

ifeq ($(SPEED),super)
  # USB3 SuperSpeed with runtime USB2 high-speed fallback (uses TMR0 as training timeout).
  # Set CFG_TUD_WCH_USB30_FALLBACK=0 for SuperSpeed-only (trains indefinitely)
  CFG_TUD_WCH_USB30_FALLBACK ?= 1
  CFLAGS += -DCFG_TUD_WCH_USBIP_USB30=1 -DCFG_TUD_WCH_USB30_FALLBACK=$(CFG_TUD_WCH_USB30_FALLBACK)
else
  CFLAGS += -DCFG_TUD_WCH_USBIP_USBHS=1
endif

LDFLAGS += \
	-nostartfiles \
	--specs=nosys.specs --specs=nano.specs

SRC_C += \
	src/portable/wch/dcd_ch56x_usb30.c \
	src/portable/wch/dcd_ch56x_usbhs.c \
	$(SDK_DIR)/Peripheral/src/CH56x_clk.c \
	$(SDK_DIR)/Peripheral/src/CH56x_gpio.c \
	$(SDK_DIR)/Peripheral/src/CH56x_uart.c \
	$(SDK_DIR)/Peripheral/src/CH56x_sys.c \
	$(SDK_DIR)/RVMSIS/core_riscv.c \
	$(FAMILY_PATH)/debug_uart.c

SRC_S += \
	$(SDK_DIR)/Startup/startup_CH56x.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_DIR)/Peripheral/inc \
	$(TOP)/$(SDK_DIR)/RVMSIS

LD_FILE ?= $(FAMILY_PATH)/linker/ch569.ld

OPENOCD_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch-ch569

# Like flash-openocd-wch, plus the SDI debug-module quiesce: clear dcsr.ebreak* (a TU_ASSERT would
# otherwise halt the core silently) and deactivate the DM, which sporadically corrupts core
# registers while it stays active during USB operation (hw-measured 2026-08-11: 10/12 HIL suite
# failures, board dead until the next flash). Must stay in the SAME openocd invocation as the
# program+reset - a second attach re-activates the DM on the now-running firmware.
flash-openocd-wch-ch569: $(BUILD)/$(PROJECT).elf
	$(OPENOCD) $(OPENOCD_OPTION) -c init -c halt -c "flash write_image $<" -c reset \
	  -c "halt" -c "reg dcsr 0x40000003" -c "resume" -c "riscv dmi_write 0x10 0" -c exit

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/RISC-V
