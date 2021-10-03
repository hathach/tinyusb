BDIR = b/

INC  = $(wildacrd *.h)
SRC  = $(wildcard *.c)
OBJ  = $(SRC:%.c=$(BDIR)/%.o)
EXE  = $(BDIR)/foo

#---------------------------------------------------------------------------
aBLK=\e[30m
aRED=\e[31m
aGRN=\e[32m
aYEL=\e[33m
aBLU=\e[34m
aMAG=\e[35m
aCYN=\e[36m
aWHT=\e[37m

aONBLK=\e[40m
aONRED=\e[41m
aONGRN=\e[42m
aONYEL=\e[43m
aONBLU=\e[44m
aONMAG=\e[45m
aONCYN=\e[46m
aONWHT=\e[47m

aNRM=\e[0m
aBRT=\e[1m
aDIM=\e[2m
aUNL=\e[4m
aFLS=\e[5m
aINV=\e[7m
aHID=\e[8m

aERROR=$(aBRT)$(aYEL)$(aONRED)

MD      = mkdir -p
ECHO    = /bin/echo -e
CC      = gcc -c -g

tPRE =$(aNRM)$(aBRT):-----[
tPOST=$(aNRM)$(aBRT)]-----:$(aNRM)

#---------------------------------------------------------------------------
all: common/tusb_common.h $(OBJ)
	gcc -o $(EXE) $(OBJ)

$(BDIR)/%.o : %.c
	@$(ECHO) "$(tPRE)$(aCYN) COMPILE: $@ $(tPOST)"
	@$(MD) $(BDIR)
	$(CC) -o $@ $(notdir $(<:.d=.c))

common/tusb_common.h:
	mkdir -p common
	@$(ECHO) "#ifndef TUSB_COMMON_" >$@
	@$(ECHO) "#define TUSB_COMMON_" >>$@
	@$(ECHO) "" >>$@
	@$(ECHO) "#define TU_ATTR_PACKED  __attribute__ ((packed))" >>$@
	@$(ECHO) "" >>$@
	@$(ECHO) "#endif" >>$@

clean:
	rm -rf $(BDIR) common
