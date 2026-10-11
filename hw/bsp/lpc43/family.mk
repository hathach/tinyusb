SDK_DIR = hw/mcu/nxp/lpcopen/lpc43xx/lpc_chip_43xx

include ${TOP}/${BOARD_PATH}/board.mk
CPU_CORE ?= cortex-m4

CFLAGS += \
  -DCORE_M4 \
  -D__USE_LPCOPEN \
  -DCFG_TUSB_MCU=OPT_MCU_LPC43XX

# mcu driver cause following warnings
CFLAGS += \
  -flto \
  -nostdlib \
  -Wno-error=unused-parameter \
  -Wno-error=cast-qual \
  -Wno-error=incompatible-pointer-types \

LDFLAGS += --specs=nosys.specs --specs=nano.specs

# Flash bank the image is linked to: A (default) or B. Bank B boots only while bank A holds no
# valid image, see linker/flash_bank_B/lpc43_flash_bank.ld
FLASH_BANK ?= A
ifeq ($(filter A B,$(FLASH_BANK)),)
  $(error FLASH_BANK must be A or B, not '$(FLASH_BANK)')
endif
LDFLAGS += -L$(TOP)/hw/bsp/lpc43/linker/flash_bank_$(FLASH_BANK)

# J-Link patches the boot checksum for bank A only: a bank-B image carries its own
ifeq ($(FLASH_BANK),B)
  POST_LINK = $(PYTHON) $(TOP)/tools/lpc_vector_checksum.py --address 0x1B000000 $@
endif

SRC_C += \
	src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	src/portable/chipidea/ci_hs/hcd_ci_hs.c \
	src/portable/ehci/ehci.c \
	${SDK_DIR}/../gcc/cr_startup_lpc43xx.c \
	${SDK_DIR}/src/chip_18xx_43xx.c \
	${SDK_DIR}/src/clock_18xx_43xx.c \
	${SDK_DIR}/src/fpu_init.c \
	${SDK_DIR}/src/gpio_18xx_43xx.c \
	${SDK_DIR}/src/iap_18xx_43xx.c \
	${SDK_DIR}/src/sysinit_18xx_43xx.c \
	${SDK_DIR}/src/uart_18xx_43xx.c \

INC += \
  $(TOP)/$(BOARD_PATH) \
	${TOP}/${SDK_DIR}/inc \
	${TOP}/${SDK_DIR}/inc/config_43xx \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
