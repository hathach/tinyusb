UF2_FAMILY_ID = 0xADA52840

NRFX_PATH = hw/mcu/nordic/nrfx

include $(TOP)/$(BOARD_PATH)/board.mk

ifeq (${MCU_VARIANT},nrf54h20)
	CPU_CORE = cortex-m33
	CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_NRF54
	LD_FILE_DEFAULT = ${FAMILY_PATH}/linker/${MCU_VARIANT}_xxaa_application.ld
	SRC_C += ${NRFX_PATH}/mdk/system_nrf54h.c
	SRC_S += ${NRFX_PATH}/mdk/gcc_startup_$(MCU_VARIANT)_application.S
	JLINK_DEVICE ?= $(MCU_VARIANT)_xxaa_app

else
ifeq (${MCU_VARIANT},nrf5340)
	CPU_CORE = cortex-m33
	CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_NRF5X
	LD_FILE_DEFAULT = ${FAMILY_PATH}/linker/${MCU_VARIANT}_xxaa_application.ld
	SRC_C += ${NRFX_PATH}/mdk/system_$(MCU_VARIANT)_application.c \
					 ${NRFX_PATH}/drivers/src/nrfx_usbreg.c
	SRC_S += ${NRFX_PATH}/mdk/gcc_startup_$(MCU_VARIANT)_application.S
  JLINK_DEVICE ?= $(MCU_VARIANT)_xxaa_app

else

	CPU_CORE = cortex-m4
	CFLAGS += -DCFG_TUSB_MCU=OPT_MCU_NRF5X
	LD_FILE_DEFAULT = ${FAMILY_PATH}/linker/${MCU_VARIANT}_xxaa.ld
	SRC_C += ${NRFX_PATH}/mdk/system_$(MCU_VARIANT).c
	SRC_S += ${NRFX_PATH}/mdk/gcc_startup_$(MCU_VARIANT).S
	JLINK_DEVICE ?= $(MCU_VARIANT)_xxaa
endif
endif

CFLAGS += \
  -DNRF_APPLICATION \
  -DCONFIG_GPIO_AS_PINRESET \
  -D__STARTUP_CLEAR_BSS

#CFLAGS += -nostdlib
#CFLAGS += -D__START=main

# suppress warning caused by vendor mcu driver
CFLAGS_GCC += \
  -flto \
  -Wno-error=undef \
  -Wno-error=unused-parameter \
  -Wno-error=unused-variable \
  -Wno-error=cast-align \
  -Wno-error=cast-qual \
  -Wno-error=redundant-decls \

LDFLAGS_GCC += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \
  -L$(TOP)/${NRFX_PATH}/mdk

LDFLAGS_CLANG += \
  -L$(TOP)/${NRFX_PATH}/mdk \

ifndef LD_FILE
LD_FILE = ${LD_FILE_DEFAULT}
endif

SRC_C += \
  src/portable/nordic/nrf5x/dcd_nrf5x.c \
  src/portable/synopsys/dwc2/dwc2_common.c \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	${NRFX_PATH}/helpers/nrfx_flag32_allocator.c \
	${NRFX_PATH}/drivers/src/nrfx_gpiote.c \
  ${NRFX_PATH}/drivers/src/nrfx_power.c \
  ${NRFX_PATH}/drivers/src/nrfx_spim.c \
  ${NRFX_PATH}/drivers/src/nrfx_uarte.c \
  ${NRFX_PATH}/soc/nrfx_atomic.c

INC += \
  $(TOP)/$(BOARD_PATH) \
  $(TOP)/$(FAMILY_PATH)/nrfx_config \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/${NRFX_PATH} \
  $(TOP)/${NRFX_PATH}/mdk \
  $(TOP)/${NRFX_PATH}/hal \
  $(TOP)/${NRFX_PATH}/drivers/include \
  $(TOP)/${NRFX_PATH}/drivers/src \

ASFLAGS += -D__HEAP_SIZE=0
