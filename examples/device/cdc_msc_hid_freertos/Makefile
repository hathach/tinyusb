include ../../../tools/top.mk
include ../../make.mk

INC += \
	src \
	$(TOP)/hw \
	$(TOP)/lib/FreeRTOS/Source/include \
	$(TOP)/lib/FreeRTOS/Source/portable/GCC/$(FREERTOS_PORT)
	
# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

# FreeRTOS source
SRC_C += \
	lib/FreeRTOS/Source/list.c \
	lib/FreeRTOS/Source/queue.c \
	lib/FreeRTOS/Source/tasks.c \
	lib/FreeRTOS/Source/timers.c \
	lib/FreeRTOS/Source/portable/MemMang/heap_4.c \
	lib/FreeRTOS/Source/portable/GCC/$(FREERTOS_PORT)/port.c \

# FreeRTOS (lto + Os) linker issue
LDFLAGS += -Wl,--undefined=vTaskSwitchContext

include ../../rules.mk
