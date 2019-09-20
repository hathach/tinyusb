include ../../../tools/top.mk
include ../../make.mk

INC += \
  src \
  $(TOP)/hw \

# stop on the first build error, which is quite a lot of porting new board
CFLAGS += -Wfatal-errors

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../rules.mk
