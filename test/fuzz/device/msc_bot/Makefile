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

# Emit dependency files so that changes to the inlined MSC driver code
# (class/msc/msc_device.c and the headers it pulls in, including this
# directory's tusb_config.h) rebuild the fuzzer instead of silently
# running a stale binary.
DEPFLAGS := -MMD -MP

.PHONY: all get-deps clean
all: $(BUILD)/msc_bot
get-deps:

$(BUILD)/msc_bot: fuzz.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) $(DEPFLAGS) -o $@ $<

clean:
	$(RM) -rf $(BUILD)

# Dependency files from the previous build (-MMD names them after the
# output); missing files are ignored, e.g. on the first build.
-include $(wildcard $(BUILD)/*.d)
