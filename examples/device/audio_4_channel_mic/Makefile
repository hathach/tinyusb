include ../../make.mk

INC += \
  src \
  $(TOP)/hw \
  $(TOP)/lib/CMSIS_5/CMSIS/DSP/Include \

# Example source
EXAMPLE_SOURCE += \
  src/main.c \
  src/usb_descriptors.c \

# CMSIS sources
SRC_C += \
  lib/CMSIS_5/CMSIS/DSP/Source/CommonTables/arm_common_tables.c \
  lib/CMSIS_5/CMSIS/DSP/Source/FastMathFunctions/arm_sin_q15.c \

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../rules.mk
