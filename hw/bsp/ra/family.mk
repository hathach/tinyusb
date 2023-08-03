DEPS_SUBMODULES += hw/mcu/renesas/fsp lib/CMSIS_5

FSP_RA = hw/mcu/renesas/fsp/ra/fsp
include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -DCFG_TUSB_MCU=OPT_MCU_RAXXX \
	-Wno-error=undef \
	-Wno-error=strict-prototypes \
	-Wno-error=cast-align \
	-Wno-error=cast-qual \
	-Wno-error=unused-but-set-variable \
	-Wno-error=unused-variable \
	-nostdlib \
	-nostartfiles \
	-ffreestanding

SRC_C += \
	src/portable/renesas/rusb2/dcd_rusb2.c \
	src/portable/renesas/rusb2/hcd_rusb2.c \
	$(FSP_RA)/src/bsp/cmsis/Device/RENESAS/Source/startup.c \
	$(FSP_RA)/src/bsp/cmsis/Device/RENESAS/Source/system.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_clocks.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_common.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_delay.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_group_irq.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_guard.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_io.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_irq.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_register_protection.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_rom_registers.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_sbrk.c \
	$(FSP_RA)/src/bsp/mcu/all/bsp_security.c \
	$(FSP_RA)/src/r_ioport/r_ioport.c \

INC += \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(BOARD_PATH)/fsp_cfg \
	$(TOP)/$(BOARD_PATH)/fsp_cfg/bsp \
	$(TOP)/$(FSP_RA)/src/bsp/cmsis/Device/RENESAS/Include \
	$(TOP)/$(FSP_RA)/inc \
	$(TOP)/$(FSP_RA)/inc/api \
	$(TOP)/$(FSP_RA)/inc/instances \
	$(TOP)/$(FSP_RA)/src/bsp/mcu/$(MCU_VARIANT) \

ifndef LD_FILE
LD_FILE = $(FAMILY_PATH)/linker/gcc/$(MCU_VARIANT).ld
LDFLAGS += -L$(TOP)/$(FAMILY_PATH)/linker/gcc
endif

# For freeRTOS port source
# hack to use the port provided by renesas
FREERTOS_PORTABLE_SRC = $(FSP_RA)/src/rm_freertos_port
