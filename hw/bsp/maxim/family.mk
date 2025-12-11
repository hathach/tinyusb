MSDK_LIB = hw/mcu/analog/msdk/Libraries

# Add any board specific make rules
include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4
PORT ?= 0
JLINK_DEVICE = ${MAX_DEVICE}
MAX_DEVICE_UPPER = $(call to_upper,${MAX_DEVICE})

ifeq ($(MAX_DEVICE),max32650)
  PERIPH_ID = 10
  PERIPH_SUFFIX = me
endif

ifneq ($(filter $(MAX_DEVICE),max32665 max32666),)
  PERIPH_ID = 14
  PERIPH_SUFFIX = me
endif

ifeq ($(MAX_DEVICE),max32690)
  PERIPH_ID = 18
  PERIPH_SUFFIX = me
endif

ifeq ($(MAX_DEVICE),max78002)
  PERIPH_ID = 87
  PERIPH_SUFFIX = ai
endif

ifndef PERIPH_ID
  $(error Unsupported MAX device: ${MAX_DEVICE})
endif

# Configure the flash rule. By default, use JLink.
SIGNED_BUILD ?= 0
DEFAULT_FLASH = flash-jlink

# --------------
# Compiler Flags
# --------------
CFLAGS += \
  -DTARGET=${MAX_DEVICE_UPPER}\
  -DTARGET_REV=0x4131 \
	-DMXC_ASSERT_ENABLE \
	-D${MAX_DEVICE_UPPER} \
	-DIAR_PRAGMAS=0 \
	-DMAX_PERIPH_ID=${PERIPH_ID} \
  -DCFG_TUSB_MCU=OPT_MCU_${MAX_DEVICE_UPPER} \
  -DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED

# mcu driver cause following warnings
CFLAGS += \
	-Wno-error=old-style-declaration \
	-Wno-error=redundant-decls \
  -Wno-error=strict-prototypes \
	-Wno-error=unused-parameter \
	-Wno-error=cast-align \
	-Wno-error=cast-qual \
	-Wno-error=sign-compare \
	-Wno-error=enum-conversion \

LDFLAGS_GCC += -nostartfiles --specs=nosys.specs --specs=nano.specs
LD_FILE_GCC ?= $(FAMILY_PATH)/linker/${MAX_DEVICE}.ld

# If the applications needs to be signed (for the MAX32651), sign it first and
# then need to use MSDK's OpenOCD to flash it
# Also need to include the __SLA_FWK__ define to enable the signed header into
# memory
ifeq ($(SIGNED_BUILD), 1)
# Extra definitions to build for the secure part
CFLAGS += -D__SLA_FWK__
DEFAULT_FLASH := sign-build flash-msdk
endif

# -----------------
# Sources & Include
# -----------------

# common
SRC_C += \
	src/portable/mentor/musb/dcd_musb.c \
	${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/heap.c \
	${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/system_${MAX_DEVICE}.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_assert.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_delay.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/mxc_lock.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/nvic_table.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/pins_${PERIPH_SUFFIX}${PERIPH_ID}.c \
	${MSDK_LIB}/PeriphDrivers/Source/SYS/sys_${PERIPH_SUFFIX}${PERIPH_ID}.c \
	${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_common.c \
	${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_${PERIPH_SUFFIX}${PERIPH_ID}.c \
	${MSDK_LIB}/PeriphDrivers/Source/FLC/flc_reva.c \
	${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_common.c \
	${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_${PERIPH_SUFFIX}${PERIPH_ID}.c \
	${MSDK_LIB}/PeriphDrivers/Source/GPIO/gpio_reva.c \
	${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_${PERIPH_SUFFIX}${PERIPH_ID}.c \
	${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_reva.c \
	${MSDK_LIB}/PeriphDrivers/Source/UART/uart_common.c \
	${MSDK_LIB}/PeriphDrivers/Source/UART/uart_${PERIPH_SUFFIX}${PERIPH_ID}.c \

SRC_S_GCC += ${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Source/GCC/startup_${MAX_DEVICE}.S

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/${MSDK_LIB}/CMSIS/5.9.0/Core/Include \
	$(TOP)/${MSDK_LIB}/CMSIS/Device/Maxim/${MAX_DEVICE_UPPER}/Include \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Include/${MAX_DEVICE_UPPER} \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Source/SYS \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Source/GPIO \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Source/ICC \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Source/FLC \
	$(TOP)/${MSDK_LIB}/PeriphDrivers/Source/UART \

# device specific
ifneq ($(filter $(MAX_DEVICE),max32650 max32665 max32666),)
  SRC_C += \
		${MSDK_LIB}/PeriphDrivers/Source/ICC/icc_common.c \
		${MSDK_LIB}/PeriphDrivers/Source/TPU/tpu_${PERIPH_SUFFIX}${PERIPH_ID}.c \
		${MSDK_LIB}/PeriphDrivers/Source/TPU/tpu_reva.c \
		${MSDK_LIB}/PeriphDrivers/Source/UART/uart_reva.c \

  INC += $(TOP)/${MSDK_LIB}/PeriphDrivers/Source/TPU
endif

ifeq (${MAX_DEVICE},max32690)
  SRC_C += \
		${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_${PERIPH_SUFFIX}${PERIPH_ID}.c \
		${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_reva.c \
		${MSDK_LIB}/PeriphDrivers/Source/CTB/ctb_common.c \
		${MSDK_LIB}/PeriphDrivers/Source/UART/uart_revb.c \

  INC += ${TOP}/${MSDK_LIB}/PeriphDrivers/Source/CTB
endif

ifeq (${MAX_DEVICE},max78002)
  SRC_C += \
		${MSDK_LIB}/PeriphDrivers/Source/AES/aes_${PERIPH_SUFFIX}${PERIPH_ID}.c \
		${MSDK_LIB}/PeriphDrivers/Source/AES/aes_revb.c \
		${MSDK_LIB}/PeriphDrivers/Source/TRNG/trng_${PERIPH_SUFFIX}${PERIPH_ID}.c \
		${MSDK_LIB}/PeriphDrivers/Source/TRNG/trng_revb.c \
		${MSDK_LIB}/PeriphDrivers/Source/UART/uart_revb.c \

  INC += \
		${TOP}/${MSDK_LIB}/PeriphDrivers/Source/AES \
		${TOP}/${MSDK_LIB}/PeriphDrivers/Source/TRNG
endif


# The MAX32651EVKIT is pin for pin identical to the MAX32650EVKIT, however the
# MAX32651 has a secure bootloader which requires the image to be signed before
# loading into flash. All MAX32651EVKIT's have the same key for evaluation
# purposes, so create a special flash rule to sign the binary and flash using
# the MSDK.
MCU_PATH = $(TOP)/hw/mcu/analog/msdk/
# Assume no extension for sign utility
SIGN_EXE = sign_app
ifeq ($(OS), Windows_NT)
# Must use .exe extension on Windows, since the binaries
# for Linux may live in the same place.
SIGN_EXE := sign_app.exe
else
UNAME = $(shell uname -s)
ifneq ($(findstring MSYS_NT,$(UNAME)),)
# Must also use .exe extension for MSYS2
SIGN_EXE := sign_app.exe
endif
endif

# Rule to sign the build.  This will in-place modify the existing .elf file
# an populate the .sig section with the signature value
sign-build: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) $(BUILD)/$(PROJECT).elf -R .sig -O binary $(BUILD)/$(PROJECT).bin
	$(MCU_PATH)/Tools/SBT/bin/$(SIGN_EXE) -c MAX32651 \
		key_file="$(MCU_PATH)/Tools/SBT/devices/MAX32651/keys/maximtestcrk.key" \
		ca=$(BUILD)/$(PROJECT).bin sca=$(BUILD)/$(PROJECT).sbin
	$(OBJCOPY) $(BUILD)/$(PROJECT).elf --update-section .sig=$(BUILD)/$(PROJECT).sig

# Optional flash option when running within an installed MSDK to use OpenOCD
# Mainline OpenOCD does not yet have the MAX32's flash algorithm integrated.
# If the MSDK is installed, flash-msdk can be run to utilize the the modified
# openocd with the algorithms
MAXIM_PATH := $(subst \,/,$(MAXIM_PATH))
flash-msdk: $(BUILD)/$(PROJECT).elf
	$(MAXIM_PATH)/Tools/OpenOCD/openocd -s $(MAXIM_PATH)/Tools/OpenOCD/scripts \
		-f interface/cmsis-dap.cfg -f target/max32650.cfg \
		-c "program $(BUILD)/$(PROJECT).elf verify; init; reset; exit"

# Configure the flash rule
flash: $(DEFAULT_FLASH)
