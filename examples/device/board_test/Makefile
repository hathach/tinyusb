include ../../../tools/top.mk
include ../../make.mk

INC += \
  src \
  $(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

# board_test example is special example that doesn't enable device or host stack
# This can cause some TinyUSB API missing, this hack to allow us to fill those API
# to pass the compilation process
CFLAGS +=	\
	-D"tud_int_handler(x)= " \

include ../../rules.mk
