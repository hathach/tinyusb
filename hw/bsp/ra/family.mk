DEPS_SUBMODULES += hw/mcu/renesas/fsp lib/CMSIS_5

# Cross Compiler for RA
CROSS_COMPILE = arm-none-eabi-

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
	-Wno-error=undef \
	-Wno-error=strict-prototypes \
	-Wno-error=cast-align \
	-Wno-error=cast-qual \
	-Wno-error=unused-but-set-variable \
	-Wno-error=unused-variable \
	-mthumb \
	-nostdlib \
	-nostartfiles \
	-ffunction-sections \
	-fdata-sections \
	-ffreestanding

SRC_C += \
	src/portable/renesas/rusb2/dcd_rusb2.c \
	src/portable/renesas/rusb2/hcd_rusb2.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_clocks.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_common.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_delay.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_group_irq.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_guard.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_io.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_irq.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_register_protection.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_rom_registers.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_sbrk.c \
	hw/mcu/renesas/fsp/ra/fsp/src/bsp/mcu/all/bsp_security.c \
	hw/mcu/renesas/fsp/ra/fsp/src/r_ioport/r_ioport.c \
	$(FSP_BOARD_DIR)/board_init.c \
	$(FSP_BOARD_DIR)/board_leds.c

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/hw/mcu/renesas/fsp/ra/fsp/src/bsp/cmsis/Device/RENESAS/Include \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(BOARD_PATH)/fsp_cfg \
	$(TOP)/hw/mcu/renesas/fsp/ra/fsp/inc \
	$(TOP)/hw/mcu/renesas/fsp/ra/fsp/inc/api \
	$(TOP)/hw/mcu/renesas/fsp/ra/fsp/inc/instances \
	$(TOP)/$(FSP_MCU_DIR) \
	$(TOP)/$(FSP_BOARD_DIR)

# For freeRTOS port source
# hack to use the port provided by renesas
FREERTOS_PORTABLE_SRC = hw/mcu/renesas/fsp/ra/fsp/src/rm_freertos_port
