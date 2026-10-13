include ../../fuzz.mk

.PHONY: regression

# replays the seed archive (the crafted out-of-bounds NTB) once under ASan
regression: all
	ctest --test-dir $(BUILD) --output-on-failure --no-tests=error -R '^net_ncm_seed_replay$$'
