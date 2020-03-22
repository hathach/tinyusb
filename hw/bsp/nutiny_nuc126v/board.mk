CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m0 \
  -D__ARM_FEATURE_DSP=0 \
  -DUSE_ASSERT=0 \
  -D__CORTEX_SC=0 \
  -DCFG_TUSB_MCU=OPT_MCU_NUC126

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/nuc126_flash.ld

SRC_C += \
  hw/mcu/nuvoton/nuc126/Device/Nuvoton/NUC126/Source/system_NUC126.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/acmp.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/adc.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/crc.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/ebi.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/fmc.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/pdma.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/pwm.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/rtc.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/sc.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/scuart.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/spi.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/timer.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/timer_pwm.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/uart.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/usbd.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/usci_spi.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/usci_uart.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/wdt.c \
  hw/mcu/nuvoton/nuc126/StdDriver/src/wwdt.c

SRC_S += \
  hw/mcu/nuvoton/nuc126/Device/Nuvoton/NUC126/Source/GCC/startup_NUC126.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc126/Device/Nuvoton/NUC126/Include \
  $(TOP)/hw/mcu/nuvoton/nuc126/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc126/CMSIS/Include

# For TinyUSB port source
VENDOR = nuvoton
CHIP_FAMILY = nuc121

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = NUC126VG4AE
JLINK_IF = swd

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
flash: $(BUILD)/$(BOARD)-firmware.elf
	openocd -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
