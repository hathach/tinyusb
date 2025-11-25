include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -D__ARM_FEATURE_DSP=0 \
  -DUSE_ASSERT=0 \
  -DCFG_EXAMPLE_MSC_READONLY \
  -DCFG_TUSB_MCU=OPT_MCU_NUC121

CPU_CORE ?= cortex-m0

# mcu driver cause following warnings
CFLAGS += -Wno-error=redundant-decls

LDFLAGS_GCC += \
  --specs=nosys.specs --specs=nano.specs

# All source paths should be relative to the top level.
# LD_FILE is defined in board.mk

# Common sources for all NUC12x variants
SRC_C += \
  src/portable/nuvoton/nuc121/dcd_nuc121.c \
  hw/mcu/nuvoton/nuc121_125/Device/Nuvoton/NUC121/Source/system_NUC121.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/clk.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/gpio.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/fmc.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/sys.c \
  hw/mcu/nuvoton/nuc121_125/StdDriver/src/timer.c

# Additional sources are added in board.mk if needed (e.g., fmc, sys, timer, uart for NUC121)

SRC_S += \
  hw/mcu/nuvoton/nuc121_125/Device/Nuvoton/NUC121/Source/GCC/startup_NUC121.S

INC += \
  $(TOP)/hw/mcu/nuvoton/nuc121_125/Device/Nuvoton/NUC121/Include \
  $(TOP)/hw/mcu/nuvoton/nuc121_125/StdDriver/inc \
  $(TOP)/hw/mcu/nuvoton/nuc121_125/CMSIS/Include \
  $(TOP)/$(BOARD_PATH)

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

# Flash using Nuvoton's openocd fork at https://github.com/OpenNuvoton/OpenOCD-Nuvoton
# Please compile and install it from github source
OPENOCD_NUVOTON_PATH ?= $(HOME)/app/OpenOCD-Nuvoton
flash: $(BUILD)/$(PROJECT).elf
	$(OPENOCD_NUVOTON_PATH)/src/openocd -s $(OPENOCD_NUVOTON_PATH)/tcl -f interface/nulink.cfg -f target/numicroM0.cfg -c "program $< reset exit"
