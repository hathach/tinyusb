include ../../../tools/top.mk
include ../../make.mk

INC += \
  src \
  $(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

# Board source
SRC_C += hw/bsp/$(BOARD)/board_$(BOARD).c

include ../../rules.mk
