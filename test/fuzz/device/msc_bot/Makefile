TOP := ../../../..
BUILD := _build
CC ?= clang

CFLAGS += -I$(TOP)/src -I. -g -O1 -fsanitize=address
FUZZ_FLAGS := -fsanitize=fuzzer

.PHONY: all get-deps clean
all: $(BUILD)/msc_bot
get-deps:

$(BUILD)/msc_bot: fuzz.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) -o $@ $<

clean:
	$(RM) -rf $(BUILD)