CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -DCFG_TUSB_MCU=OPT_MCU_NUC505

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/nuc505_flashtoram.ld

SRC_C += \
  hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Source/system_NUC505Series.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/adc.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/i2c.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/i2s.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/pwm.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/rtc.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/sd.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/spi.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/spim.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/timer.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/uart.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/usbd.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/wdt.c \
  hw/mcu/nuvoton/nuc505/StdDriver/src/wwdt.c

SRC_S += \
  hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Source/GCC/startup_NUC505Series.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc505/Device/Nuvoton/NUC505Series/Include \
  $(TOP)/hw/mcu/nuvoton/nuc505/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc505/CMSIS/Include

# For TinyUSB port source
VENDOR = nuvoton
CHIP_FAMILY = nuc505

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = NUC505YO13Y
JLINK_IF = swd

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
flash: $(BUILD)/$(BOARD)-firmware.elf
	openocd -f interface/nulink.cfg -f target/numicroM4.cfg -c "program $< reset exit"
