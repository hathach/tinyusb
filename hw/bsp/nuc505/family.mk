include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -DCFG_TUSB_MCU=OPT_MCU_NUC505

CPU_CORE ?= cortex-m4

# mcu driver cause following warnings
CFLAGS += -Wno-error=redundant-decls

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# LD_FILE is defined in board.mk

SRC_C += \
  src/portable/nuvoton/nuc505/dcd_nuc505.c \
  hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Source/system_NUC505Series.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/adc.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/i2c.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/i2s.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/pwm.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/rtc.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/spi.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/spim.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/timer.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/uart.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/wdt.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/wwdt.c

SRC_S += \
  hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Source/GCC/startup_NUC505Series.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Include \
  $(TOP)/hw/mcu/nuvoton/nuc505/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc505/CMSIS/Include \
  $(TOP)/$(BOARD_PATH)

FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F

# Note
# To be able to program the SPI flash, it need to boot with ICP mode "1011".
# However, in ICP mode, opencod cannot establish connection to the mcu.
# Therefore, there is no easy command line flash for NUC505
# It is probably better to just use Nuvoton NuMicro ICP programming on windows to program the board

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
OPENOCD_NUVOTON_PATH ?= $(HOME)/app/OpenOCD-Nuvoton
flash: $(BUILD)/$(PROJECT).elf
	$(OPENOCD_NUVOTON_PATH)/src/openocd -s $(OPENOCD_NUVOTON_PATH)/tcl -f interface/nulink.cfg -f target/numicroM4.cfg -c "program $< reset exit"
