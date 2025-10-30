# Agent Handbook

## Shared TinyUSB Ground Rules
- Keep TinyUSB memory-safe: avoid dynamic allocation, defer ISR work to task context, and follow C99 with two-space indentation/no tabs.
- Match file organization: core stack under `src`, MCU/BSP support in `hw/{mcu,bsp}`, examples under `examples/{device,host,dual}`, docs in `docs`, tests under `test/{unit-test,fuzz,hil}`.
- Use descriptive snake_case for helpers, reserve `tud_`/`tuh_` for public APIs, `TU_` for macros, and keep headers self-contained with `#if CFG_TUSB_MCU` guards where needed.
- Prefer `.clang-format` for C/C++ formatting, run `pre-commit run --all-files` before submitting, and document board/HIL coverage when applicable.
- Commit in imperative mood, keep changes scoped, and supply PRs with linked issues plus test/build evidence.

## Build and Test Cheatsheet
- Fetch dependencies once with `python3 tools/get_deps.py [FAMILY]`; assets land in `lib/` and `hw/mcu/`.
- CMake (preferred): `cmake -G Ninja -DBOARD=<board> -DCMAKE_BUILD_TYPE={MinSizeRel|Debug}` inside an example `build/` dir, then `ninja` or `cmake --build .`.
- Make (alt): `make BOARD=<board> [DEBUG=1] [LOG=2 LOGGER=rtt] all` from the example root; add `uf2`, `flash-openocd`, or `flash-jlink` targets as needed.
- Bulk builds: `python3 tools/build.py -b <board>` to sweep all examples; expect occasional non-critical objcopy warnings.
- Unit tests: `cd test/unit-test && ceedling test:all` (or a specific `test_<module>`), honor Unity/CMock fixtures under `test/support`.
- Docs: `pip install -r docs/requirements.txt` then `sphinx-build -b html . _build` from `docs/`.

## Validation Checklist
1. `pre-commit run --all-files` after edits (install with `pip install pre-commit && pre-commit install`).
2. Build at least one representative example (e.g., `examples/device/cdc_msc`) via CMake+Ninja or Make.
3. Run unit tests relevant to touched modules; add fuzz/HIL coverage when modifying parsers or protocol state machines.

## Copilot Agent Notes (`.github/copilot-instructions.md`)
- Treat this handbook as authoritative before searching or executing speculative shell commands; unexpected conflicts justify additional probing.
- Respect build timing guidance: allow ≥5 minutes for single example builds and ≥30 minutes for bulk runs; never cancel dependency fetches or builds mid-flight.
- Support optional switches: `-DRHPORT_DEVICE[_SPEED]`, logging toggles (`LOG=2`, `LOGGER=rtt`), and board selection helpers from `tools/get_deps.py`.
- Flashing shortcuts: `ninja <target>-jlink|openocd|uf2` or `make BOARD=<board> flash-{jlink,openocd}`; list Ninja targets with `ninja -t targets`.
- Keep Ceedling installed (`sudo gem install ceedling`) and available for per-test or full suite runs triggered from `test/unit-test`.

## Claude Agent Notes (`CLAUDE.md`)
- Default to CMake+Ninja for builds, but align with Make workflows when users rely on legacy scripts; provide DEBUG/LOG/LOGGER knobs consistently.
- Highlight dependency helpers (`tools/get_deps.py rp2040`) and reference core locations: `src/`, `hw/`, `examples/`, `test/`.
- Run `clang-format` on all touched files to ensure consistent formatting.
- Use `TU_ASSERT` for all fallible calls to enforce runtime checks.
- Ensure header comments retain the MIT license notice.
- Add descriptive comments for non-trivial code paths to aid maintainability.
- Release flow primer: bump `tools/make_release.py` version, run the script (updates `src/tusb_option.h`, `repository.yml`, `library.json`), refresh `docs/info/changelog.rst`, then tag.
- Testing reminders: Ceedling full or targeted runs, specify board/OS context, and ensure logging of manual hardware outcomes when available.
