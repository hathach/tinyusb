
CROSS_COMPILE ?= riscv-none-elf-

CH32_FAMILY = ch58x
SDK_DIR = hw/mcu/wch/CH585
SDK_SRC_DIR = $(SDK_DIR)/EVT/EXAM/SRC

include $(TOP)/$(BOARD_PATH)/board.mk

#CPU_CORE ?= rv32imac-ilp32
CFLAGS += \
    -march=rv32imac_zicsr_zifencei \s
    -mabi=ilp32 \


CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDFLAGS_GCC += \
    -march=rv32imac_zicsr_zifencei \
    -mabi=ilp32 \

CFLAGS += -Wno-error=strict-prototypes -Wno-error=undef -Wno-error=cast-qual -Wno-error=unused-variable -Wno-error=pointer-sign
#these flangs are used to supress WCH's low quality SDK, which is full of warnings that will otherwise stop the building process


SPEED ?= full

CFLAGS += \
	-g\
	-gdwarf-2\
	-DCFG_TUSB_MCU=OPT_MCU_CH585 \
	-DINT_SOFT


CFLAGS += -DCFG_TUD_WCH_USBIP_USBFS_585=1

LDFLAGS_GCC += \
	-nostdlib -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \
  -T $(TOP)/$(SDK_SRC_DIR)/Ld/Link.ld\
  -Wl,--start-group -L$(TOP)/$(SDK_SRC_DIR)/StdPeriphDriver \
           -lISP585 \ # This part of the SDK is not open sourced.
		   
	-Wl,--trace-symbol=FLASH_EEPROM_CMD, --end-group
		   


SRC_C += \
	src/portable/wch/dcd_ch32_usbfs_585.c \
	$(wildcard $(TOP)/$(SDK_SRC_DIR)/StdPeriphDriver/*.c)


SRC_S += \
	$(SDK_SRC_DIR)/Startup/startup_CH585.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_SRC_DIR)/RVMSIS/\
	$(TOP)/$(SDK_SRC_DIR)/StdPeriphDriver/inc


OPENOCD_WCH_OPTION=-f $(TOP)/$(FAMILY_PATH)/wch-riscv.cfg
flash: flash-openocd-wch
