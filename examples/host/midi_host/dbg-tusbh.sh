#!/bin/bash
# quick'n'dirty

OCDMODE=picoprobe
OCD=/home/pico/openocd.picoprobe/
OCDIF=interface/picoprobe.cfg
SESS=pico_debug_console
TTY=/dev/ttyACM0
BAUD=115200

ELF=$2
[[ -z ${ELF} ]] && ELF=b/midi_controller.elf

case $1 in
	OCD)
		${OCD}/src/openocd  -s ${OCD}/tcl/  -f ${OCDIF}  -f target/rp2040.cfg
		;;
	CGDB)
		cgdb -ex "target remote localhost:3333" \
			-ex "load ${ELF}" \
			-ex "set confirm off" \
			-ex "file ${ELF}" \
			-ex "set confirm on" \
			-ex "monitor reset init"
    	;;
	DKILL)
		tmux kill-window -t "${SESS}"
		;;
	minicom)
		minicom -b 115200 -o -D ${TTY}
    	;;
	*)
		tmux new-session -s "${SESS}" \;\
			set -g pane-border-status top \;\
			set -g pane-border-format "=[ #T ]=" \;\
		\
			select-pane -T "OpenOCD (${OCDMODE})" \;\
			send-keys "$0 OCD" C-m \;\
		\
			split-window -v \;\
		\
			select-pane -T "Debugger" \;\
			send-keys "sleep 1 ; $0 CGDB ; $0 DKILL" C-m \;\
		\
			select-pane -U \;\
			split-window -h \;\
		\
			select-pane -T "minicom ${TTY}@${BAUD}" \;\
			send-keys "$0 minicom -w -C minicom.log" C-m \;\
		\
			select-pane -D \;\
		\
			set -g mouse on \
		 || true
esac

#			resize-pane -y 60 \;\
#			resize-pane -x 120 \;\
