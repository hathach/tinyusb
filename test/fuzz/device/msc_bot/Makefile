TOP := ../../../..
BUILD := _build

# Select clang when CC has make's built-in default (cc), which may be gcc and
# cannot handle -fsanitize=fuzzer. An explicit CC (environment or command
# line) still takes precedence.
ifeq ($(origin CC),default)
CC := clang
endif

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