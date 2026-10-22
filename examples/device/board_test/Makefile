include ../../../hw/bsp/family_support.mk

INC += \
  src \


# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)

# SWO trace fixture (src/swo_fixture): stm32f407disco and stm32h743nucleo without RTOS only
ifeq ($(SWO_FIXTURE),1)
  ifneq ($(filter $(BOARD),stm32f407disco stm32h743nucleo)-$(RTOS),$(BOARD)-)
    $(error SWO_FIXTURE supports stm32f407disco and stm32h743nucleo without RTOS only)
  endif
  INC += src/swo_fixture
  EXAMPLE_SOURCE += src/swo_fixture/swo_fixture.c
  CFLAGS += -DSWO_FIXTURE
endif

SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

include ../../../hw/bsp/family_rules.mk
