SDK_DIR = hw/mcu/microchip/same70

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m7

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_SAMX7X

# suppress following warnings from mcu driver
CFLAGS += -Wno-error=unused-parameter -Wno-error=cast-align -Wno-error=redundant-decls

# SAM driver is flooded with -Wcast-qual which slows down compilation significantly
CFLAGS_SKIP += -Wcast-qual

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE = $(SDK_DIR)/same70b/gcc/gcc/same70q21b_flash.ld

SRC_C += \
	src/portable/microchip/samx7x/dcd_samx7x.c \
	$(SDK_DIR)/same70b/gcc/gcc/startup_same70q21b.c \
	$(SDK_DIR)/same70b/gcc/system_same70q21b.c \
	$(SDK_DIR)/hpl/core/hpl_init.c \
	$(SDK_DIR)/hpl/usart/hpl_usart.c \
	$(SDK_DIR)/hpl/pmc/hpl_pmc.c \
	$(SDK_DIR)/hal/src/hal_usart_async.c \
	$(SDK_DIR)/hal/src/hal_io.c \
	$(SDK_DIR)/hal/src/hal_atomic.c \
	$(SDK_DIR)/hal/utils/src/utils_ringbuffer.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_DIR) \
	$(TOP)/$(SDK_DIR)/config \
	$(TOP)/$(SDK_DIR)/same70b/include \
	$(TOP)/$(SDK_DIR)/hal/include \
	$(TOP)/$(SDK_DIR)/hal/utils/include \
	$(TOP)/$(SDK_DIR)/hpl/core \
	$(TOP)/$(SDK_DIR)/hpl/pio \
	$(TOP)/$(SDK_DIR)/hpl/pmc \
	$(TOP)/$(SDK_DIR)/hri \
	$(TOP)/$(SDK_DIR)/CMSIS/Core/Include

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM7

# For flash-jlink target
flash: $(BUILD)/$(PROJECT).bin
	edbg --verbose -t same70 -pv -f $<
