# Renesas Synergy

Synergy builds require the Renesas Synergy Software Package (SSP), which is not
distributed with TinyUSB. Obtain and install the SSP under its license terms.

Set `RENESAS_SSP_PATH` to the SSP root directory, containing `inc/` and `src/`.
The directory can be anywhere and doesn't need to keep the `synergy/ssp`
nesting from the packs. For Make builds the path must be relative to the
TinyUSB root directory; CMake accepts an absolute path or one relative to the
directory `cmake` is run from. Alternatively, place it at
`hw/mcu/renesas/synergy/ssp/`; that directory is ignored by Git.

## Getting the SSP files

The SSP is split into several CMSIS packs. The core pack and the chip-specific
BSP files come in separate packs. Board packs (`Renesas.Synergy_board_*`) are
not needed: each TinyUSB board directory carries its own configuration.

1. Download the SSP release from Renesas (Synergy Software Package on the
   Renesas website), or install it with e² studio. With e² studio, the packs
   end up in `internal/projectgen/arm/packs/` under the e² studio install
   directory.
2. Pick out these packs (`.pack` files):
   - the core SSP pack, `Renesas.Synergy.2.7.0.pack`
   - the BSP pack for the `S-124` MCU, `Renesas.Synergy_mcu_s124.2.7.0.pack`
3. A `.pack` file is a zip archive. Extract both into one directory so
   that their `synergy/ssp/` trees merge:

   ```sh
   mkdir -p ~/ssp-2.7.0
   for p in /path/to/packs/*.pack; do unzip -o -q "$p" -d ~/ssp-2.7.0; done
   ```

4. Point `RENESAS_SSP_PATH` at the merged `synergy/ssp` directory, for example
   `~/ssp-2.7.0/synergy/ssp`.

## Building

For CMake:

```sh
cmake -S examples -B examples/cmake-build-dk_s124 -DBOARD=dk_s124 \
  -DRENESAS_SSP_PATH=/path/to/ssp -G Ninja
cmake --build examples/cmake-build-dk_s124
```

For Make:

```sh
make -C examples/device/hid_composite BOARD=dk_s124 \
  RENESAS_SSP_PATH=../ssp-2.7.0/synergy/ssp all
```

If the SSP path is missing or does not contain the expected files for the
selected board, configuration stops with an error.

## Toolchain

Build with GCC (`arm-none-eabi-gcc`). Clang is not supported: the SSP
sources don't compile with it, so selecting the Clang toolchain stops the
build at configuration with an error.
