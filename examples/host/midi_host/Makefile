# quick'n'dirty

NAME=midi_host
ELF=b/$(NAME).elf

DBG =
DBG = -DCMAKE_BUILD_TYPE=Debug

#===============================================================================
# yeah, this is all hacky code right now
.PHONY: all
all : $(ELF)

$(ELF): b $(wildcard src/*) $(wildcard ../../../src/class/midi/*) $(wildcard ../../../src/host/*) $(wildcard ../../../src/common/*)
	rm -f b/CMakeFiles/$(NAME).dir/${HOME}/src/pico/_test/_tusb/tinyusb.fork/src/class/midi/*.obj
	rm -f b/$(NAME).*
	(cd b ;\
		cmake .. -DFAMILY=rp2040 $(DBG) ;\
		make ;\
		cd .. ;\
	)

#===============================================================================
.PHONY: clean
clean:
	rm -rf b

#===============================================================================
b:
	mkdir b

#===============================================================================
.PHONY: dbg
dbg: $(ELF)
	./dbg-tusbh.sh DOIT $(ELF)

#===============================================================================
# Push the firmware via picoprobe
#
OCD   := /home/pico/openocd.picoprobe/
OCDIF := interface/picoprobe.cfg

.PHONY: push
push: $(ELF)
	@echo -e "$(LGRN)# Uploading via picoprobe...$(NRM)"
	$(OCD)/src/openocd  -s $(OCD)/tcl/  -f $(OCDIF)  -f target/rp2040.cfg \
		-c "program $(ELF) verify reset exit"
	@echo -e "\n$(YEL)* Finished$(NRM)"

#===============================================================================
# Push the firmware via picoprobe
#
.PHONY: mc
.PHONY: minicom
mc minicom: $(ELF)
	TERM=linux minicom -b 115200 -o -D /dev/ttyACM0 -C `date +%Y.%m.%d-%H.%M.%S`.log
