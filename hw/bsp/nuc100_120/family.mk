include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_EXAMPLE_VIDEO_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_NUC120

CPU_CORE ?= cortex-m0

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# LD_FILE is defined in board.mk

SRC_C += \
  src/portable/nuvoton/nuc120/dcd_nuc120.c \
  hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Source/system_NUC100Series.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/timer.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/uart.c

SRC_S += \
  hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Source/GCC/startup_NUC100Series.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Include \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/CMSIS/Include \
  $(TOP)/$(BOARD_PATH)

FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
OPENOCD_NUVOTON_PATH ?= $(HOME)/app/OpenOCD-Nuvoton
flash: $(BUILD)/$(PROJECT).elf
	$(OPENOCD_NUVOTON_PATH)/src/openocd -s $(OPENOCD_NUVOTON_PATH)/tcl -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
