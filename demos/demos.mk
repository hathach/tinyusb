#Compiler Flags = Include Path + Macros Defines
CFLAGS = $(MACROS_DEF) $(INC_PATH)

#Macros is Board and Mcu
MACROS_DEF += -DBOARD=$(board) -DMCU=MCU_$(MCU)

#MCU currently supported
mcu_support = 13UXX

################ Board and MCU determination ################
#all configuration build's name must be named after the macro BOARD_NAME defined in the tinyusb/demos/boards/board.h
buildname :=  $(shell echo $(notdir $(build_dir)) | tr a-z A-Z)
board = $(buildname)
mcu = $(shell echo $(MCU) | tr A-Z a-z)

$(info board $(board) )

ifeq (,$(findstring BOARD_,$(board)))
 $(error build's name must be name exactly the same as the macro BOARD_NAME defined in the tinyusb/demos/boards/board.h)
endif

MCU := LPC$(findstring $(mcu_support),$(board))

ifeq (,$(MCU))
$(error build name must contain one of supported mcu: $(mcu_support))
endif

$(info MCU $(MCU) $(mcu))

################ Build Manipulate ################
#Remove all other mcu in demos/bsp and keep only needed one
OBJS := $(filter-out ./bsp/lpc%,$(OBJS)) $(filter ./bsp/$(mcu)%,$(OBJS))

#remove all other toolchain startup script
toolchain = xpresso
OBJS := $(filter-out ./bsp/$(mcu)/startup%,$(OBJS)) $(filter ./bsp/$(mcu)/startup_$(toolchain)%,$(OBJS))

#CMSIS include path & lib path
cmsis_proj = CMSISv2p10_$(shell echo $(MCU) | tr X x)
rel_include +=  $(cmsis_proj)/inc
rel_include +=  demos/bsp/boards
rel_include +=  demos/bsp/$(mcu)/inc
INC_PATH = $(addprefix  -I"$(workspace_dir)/, $(addsuffix ",$(rel_include)))

LIBS += -l$(cmsis_proj) -L"$(workspace_dir)/$(cmsis_proj)/Debug"
#$(warning $(OBJS))

#generate makefiles.def containing MCU define for tinyusb lib
$(shell echo CFLAGS = -DMCU=MCU_$(MCU) > $(workspace_dir)/tinyusb/makefile.defs) 







#startup_objs := $(subst $(workspace_dir)/demos,.,$(shell find $(workspace_dir)/demos/ -type f))
#startup_objs := $(patsubst %.c,%.o,$(startup_objs))
#startup_objs := $(patsubst %.s,%.o,$(startup_objs))
#$(error $(startup_objs))
#OBJS := $(filter-out $(startup_objs),$(OBJS))

# add required startup script $(mcu) serves as sub folders inside startup
#startup_lpxpresso = $(mcu).*cr_startup
#startup_require += $(strip $(foreach entry,$(startup_objs), $(shell echo $(entry) | grep -i $(startup_lpxpresso))))
#OBJS += $(startup_require)
#$(warning $(startup_require))

#Remove Other MCU source, start 
#$(warning $(CFLAGS) )
#$(warning $(S_SRCS) )

