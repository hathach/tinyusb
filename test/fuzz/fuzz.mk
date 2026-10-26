# Make entry points kept for OSS-Fuzz's projects/tinyusb/build.sh, which runs `make -C <harness> get-deps all` and
# copies _build/<harness>; the build itself is test/fuzz/CMakeLists.txt.
TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
HARNESS := $(notdir $(CURDIR))
BUILD := _build

# make's built-in cc must not reach CMake; an environment or command-line CC still wins
ifeq ($(origin CC),default)
export CC := clang
endif
ifeq ($(origin CXX),default)
export CXX := clang++
endif

.PHONY: all get-deps clean

# --fresh so CC/CXX/CFLAGS/CXXFLAGS/LDFLAGS changes are never masked by CMake's cache
all:
	cmake --fresh -S $(TOP)/test/fuzz -B $(BUILD) -DTINYUSB_FUZZ_TARGET=$(HARNESS)
	cmake --build $(BUILD) --target $(HARNESS) --parallel

get-deps:

clean:
	cmake -E rm -rf $(BUILD)
