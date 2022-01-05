CFLAGS += -mcpu=cortex-a53 \
          -DBCM_VERSION=2837 \
          -DCFG_TUSB_MCU=OPT_MCU_BCM2837

CROSS_COMPILE = aarch64-none-elf-

SUFFIX = 8

INC += $(TOP)/lib/CMSIS_5/CMSIS/Core_A/Include
