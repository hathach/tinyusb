include ../../build_system/make/make.mk

INC += \
  src \
  $(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../build_system/make/rules.mk
