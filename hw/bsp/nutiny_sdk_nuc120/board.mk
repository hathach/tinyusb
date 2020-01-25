CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m0 \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_NUC120

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/nutiny_sdk_nuc120/nuc120_flash.ld

SRC_C += \
  hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Source/system_NUC100Series.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/acmp.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/adc.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/crc.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/fmc.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/i2c.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/i2s.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/pdma.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/ps2.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/pwm.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/rtc.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/sc.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/spi.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/timer.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/uart.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/usbd.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/wdt.c \
  hw/mcu/nuvoton/nuc100_120/StdDriver/src/wwdt.c

SRC_S += \
  hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Source/GCC/startup_NUC100Series.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/Device/Nuvoton/NUC100Series/Include \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc100_120/CMSIS/Include

# For TinyUSB port source
VENDOR = nuvoton
CHIP_FAMILY = nuc120

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = NUC120LE3
JLINK_IF = swd

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
flash: $(BUILD)/$(BOARD)-firmware.elf
	openocd -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
