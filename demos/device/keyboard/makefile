#######################################################################################
# Software License Agreement (BSD License)
# Copyright (c) 2012, hathach (tinyusb.net)
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
# OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#######################################################################################

##################################################
# Keyboard makefile
##################################################

############ Toolchain ############
toolchain  := lpcxpresso
CC         := arm-none-eabi-gcc
RM         := rm -rf
#AR LD

toolchain_def    = __REDLIB__ __CODE_RED __USE_CMSIS=CMSISv2p00_LPC11Uxx
mcu              = lpc11uxx
build_path       = ../build/

#helper function
rel2abs          = $(shell cd $(1); pwd)

#path
tinyusb_path     = ../../../tinyusb
bsp_path         = ../../bsp
cmsis_path       = ../../../../CMSISv2p00_LPC11Uxx

build_path_abs   = $(CURDIR)/$(build_path_relative)
tinyusb_path_abs = $(CURDIR)/$(tinyusb_path)
bsp_path_abs     = $(CURDIR)/$(bsp_path)
cmsis_path_abs   = $(CURDIR)/$(cmsis_path)

############ Sources ############
src              = $(shell find $(CURDIR) -type f -name "*.c") $(tinyusb_src) $(bsp_src)
objects          = $(subst .c,.o,$(src))
dependencies     = $(subst .c,.d,$(src))

tinyusb_src      = $(shell find $(tinyusb_path_abs) \( ! -name "*hal*" \) -type f -name "*.c") $(tinyusb_path_abs)/hal/hal_$(mcu).c
bsp_src          = $(shell find $(bsp_path_abs)/boards -type f -name "*.c") $(shell find $(bsp_path_abs)/$(mcu) -type f -name "*.c")
cmsis_src        = $(shell find $(cmsis_path_abs) -type f -name "*.c")

############ CFLAGS C Compiler Flag ##############
#CFLAGS = $(addprefix -D,$(toolchain_def) $(macros_def)) $(addprefix -I,$(inc_path))

############ LDFLAGS Linker Flag ##############
#LDFLAGS

############ CPPFLAGS C Preprocessor Flag ##############
CPPFLAGS = $(addprefix -D,$(toolchain_def) $(macros_def)) $(addprefix -I,$(inc_path))

macros_def += BOARD=BOARD_AT86RF2XX
macros_def += MCU=MCU_LPC11UXX

inc_path     = $(CURDIR) $(tinyusb_path_abs) $(bsp_path_abs) $(cmsis_path_abs)/inc

############ Compile Rules ##############
#%.o : %.c  

############ Target ##############
all: keyboard.axf

keyboard.axf : $(objects)

clean:
	-$(RM) $(objects) $(dependencies) keyboard.axf

.PHONY: all clean

############ Dependencies Generate and Inlcude ############
ifneq ($(MAKECMDGOALS),clean)
include $(dependencies)
endif

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
