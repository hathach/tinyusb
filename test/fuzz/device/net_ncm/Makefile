include ../../fuzz.mk

.PHONY: regression

# replays the crafted seed under ASan without a fuzzing engine
regression:
	cmake --fresh -S $(TOP)/test/fuzz -B $(CMAKE_BUILD) -DTINYUSB_FUZZ_TARGET=$(HARNESS)
	cmake --build $(CMAKE_BUILD) --target net_ncm_regression
	$(CMAKE_BUILD)/net_ncm_regression
