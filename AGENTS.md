# TinyUSB Agent Instructions

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded systems, designed to be memory-safe with no
dynamic allocation and thread-safe with all interrupt events deferred to non-ISR task functions.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected
information that does not match the info here.

## Shared Ground Rules
- Keep TinyUSB memory-safe: avoid dynamic allocation, defer ISR work to task context, and follow C99 with two-space indentation/no tabs.
- Match file organization: core stack under `src`, MCU/BSP support in `hw/{mcu,bsp}`, examples under `examples/{device,host,dual}`, docs in `docs`, tests under `test/{unit-test,fuzz,hil}`.
- Use descriptive snake_case for helpers, reserve `tud_`/`tuh_` for public APIs, `TU_` for macros, and keep headers self-contained with `#if CFG_TUSB_MCU` guards where needed.
- Prefer `.clang-format` for C/C++ formatting, run `pre-commit run --all-files` before submitting, and document board/HIL coverage when applicable.
- Commit in imperative mood, keep changes scoped, and supply PRs with linked issues plus test/build evidence.


## Bootstrap and Build Setup

- Install ARM GCC toolchain: `sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi`
- Fetch core dependencies: `python3 tools/get_deps.py` -- takes <1 second. NEVER CANCEL.
- For specific board families: `python3 tools/get_deps.py FAMILY_NAME` (e.g., rp2040, stm32f4)
- Dependencies are cached in `lib/` and `hw/mcu/` directories

## Build Examples

Choose ONE of these approaches:

**Option 1: Individual Example with CMake (RECOMMENDED)**

```bash
cd examples/device/cdc_msc
mkdir -p build && cd build
cmake -DBOARD=raspberry_pi_pico -DCMAKE_BUILD_TYPE=MinSizeRel ..
cmake --build . -j4
```

-- takes 1-2 seconds. NEVER CANCEL. Set timeout to 5+ minutes.

**CMake with Ninja (Alternative)**

```bash
cd examples/device/cdc_msc
mkdir build && cd build
cmake -G Ninja -DBOARD=raspberry_pi_pico ..
ninja
```

**Option 2: Individual Example with Make**

```bash
cd examples/device/cdc_msc
make BOARD=raspberry_pi_pico all
```

-- takes 2-3 seconds. NEVER CANCEL. Set timeout to 5+ minutes.

**Option 3: All Examples for a Board**

```bash
python3 tools/build.py -b BOARD_NAME
```

-- takes 15-20 seconds, may have some objcopy failures that are non-critical. NEVER CANCEL. Set timeout to 30+ minutes.

## Build Options

- **Debug build**:
    - CMake: `-DCMAKE_BUILD_TYPE=Debug`
    - Make: `DEBUG=1`
- **With logging**:
    - CMake: `-DLOG=2`
    - Make: `LOG=2`
- **With RTT logger**:
    - CMake: `-DLOG=2 -DLOGGER=rtt`
    - Make: `LOG=2 LOGGER=rtt`
- **RootHub port selection**:
    - CMake: `-DRHPORT_DEVICE=1`
    - Make: `RHPORT_DEVICE=1`
- **Port speed**:
    - CMake: `-DRHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED`
    - Make: `RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED`

## Flashing and Deployment

- **Flash with JLink**:
    - CMake: `ninja cdc_msc-jlink`
    - Make: `make BOARD=raspberry_pi_pico flash-jlink`
- **Flash with OpenOCD**:
    - CMake: `ninja cdc_msc-openocd`
    - Make: `make BOARD=raspberry_pi_pico flash-openocd`
- **Generate UF2**:
    - CMake: `ninja cdc_msc-uf2`
    - Make: `make BOARD=raspberry_pi_pico all uf2`
- **List all targets** (CMake/Ninja): `ninja -t targets`

## Unit Testing

- Install Ceedling: `sudo gem install ceedling`
- Run all unit tests: `cd test/unit-test && ceedling` or `cd test/unit-test && ceedling test:all` -- takes 4 seconds.
  NEVER CANCEL. Set timeout to 10+ minutes.
- Run specific test: `cd test/unit-test && ceedling test:test_fifo`
- Tests use Unity framework with CMock for mocking

## Documentation

- Install requirements: `pip install -r docs/requirements.txt`
- Build docs: `cd docs && sphinx-build -b html . _build` -- takes 2-3 seconds. NEVER CANCEL. Set timeout to 10+ minutes.

## Code Quality and Validation

- Format code: `clang-format -i path/to/file.c` (uses `.clang-format` config)
- Check spelling: `pip install codespell && codespell` (uses `.codespellrc` config)
- Pre-commit hooks validate unit tests and code quality automatically

## Static Analysis with PVS-Studio

- **Analyze whole project**:
  ```bash
  pvs-studio-analyzer analyze -f examples/cmake-build-raspberry_pi_pico/compile_commands.json -R .PVS-Studio/.pvsconfig -o pvs-report.log -j12 --dump-files --misra-cpp-version 2008 --misra-c-version 2023 --use-old-parser
  ```
- **Analyze specific source files**:
  ```bash
  pvs-studio-analyzer analyze -f examples/cmake-build-raspberry_pi_pico/compile_commands.json -R .PVS-Studio/.pvsconfig -S path/to/file.c -o pvs-report.log -j12 --dump-files --misra-cpp-version 2008 --misra-c-version 2023 --use-old-parser
  ```
- **Multiple specific files**:
  ```bash
  pvs-studio-analyzer analyze -f examples/cmake-build-raspberry_pi_pico/compile_commands.json -R .PVS-Studio/.pvsconfig -S src/file1.c -S src/file2.c -o pvs-report.log -j12 --dump-files --misra-cpp-version 2008 --misra-c-version 2023 --use-old-parser
  ```
- Requires `compile_commands.json` in the build directory (generated by CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`)
- Use `-f` option to specify path to `compile_commands.json`
- Use `-R .PVS-Studio/.pvsconfig` to specify rule configuration file
- Use `-j12` for parallel analysis with 12 threads
- `--dump-files` saves preprocessed files for debugging
- `--misra-c-version 2023` enables MISRA C:2023 checks
- `--misra-cpp-version 2008` enables MISRA C++:2008 checks
- `--use-old-parser` uses legacy parser for compatibility
- Analysis takes ~10-30 seconds depending on project size. Set timeout to 5+ minutes.
- View results: `plog-converter -a GA:1,2 -t errorfile pvs-report.log` or open in PVS-Studio GUI

## Validation Checklist

### ALWAYS Run These After Making Changes

1. **Pre-commit validation** (RECOMMENDED): `pre-commit run --all-files`
    - Install pre-commit: `pip install pre-commit && pre-commit install`
    - Runs all quality checks, unit tests, spell checking, and formatting
    - Takes 10-15 seconds. NEVER CANCEL. Set timeout to 15+ minutes.
2. **Build validation**: Build at least one example that exercises your changes
   ```bash
   cd examples/device/cdc_msc
   make BOARD=raspberry_pi_pico all
   ```
3. Run unit tests relevant to touched modules; add fuzz/HIL coverage when modifying parsers or protocol state machines.

### Manual Testing Scenarios
- **Device examples**: Cannot be fully tested without real hardware, but must build successfully
- **Unit tests**: Exercise core stack functionality - ALL tests must pass
- **Build system**: Must be able to build examples for multiple board families

### Board Selection for Testing
- **STM32F4**: `stm32f407disco` - no external SDK required, good for testing
- **RP2040**: `raspberry_pi_pico` - requires Pico SDK, commonly used
- **Other families**: Check `hw/bsp/FAMILY/boards/` for available boards

## Release Instructions

**DO NOT commit files automatically - only modify files and let the maintainer review before committing.**

1. Bump the release version variable at the top of `tools/make_release.py`.
2. Execute `python3 tools/make_release.py` to refresh:
   - `src/tusb_option.h` (version defines)
   - `repository.yml` (version mapping)
   - `library.json` (PlatformIO version)
   - `sonar-project.properties` (SonarQube version)
   - `docs/reference/boards.rst` (generated board documentation)
   - `hw/bsp/BoardPresets.json` (CMake presets)
3. Generate release notes for `docs/info/changelog.rst`:
   - Get commit list: `git log <last-release-tag>..HEAD --oneline`
   - **Visit GitHub PRs** for merged pull requests to understand context and gather details
   - Use GitHub tools to search/read PRs: `github-mcp-server-list_pull_requests`, `github-mcp-server-pull_request_read`
   - Extract key changes, API modifications, bug fixes, and new features from PR descriptions
   - Add new changelog entry following the existing format:
     - Version heading with equals underline (e.g., `0.20.0` followed by `======`)
     - Release date in italics (e.g., `*November 19, 2024*`)
     - Major sections: General, API Changes, Controller Driver (DCD & HCD), Device Stack, Host Stack, Testing
     - Use bullet lists with descriptive categorization
     - Reference function names, config macros, and file paths using RST inline code (double backticks)
     - Include meaningful descriptions, not just commit messages
4. **Validation before commit**:
   - Run unit tests: `cd test/unit-test && ceedling test:all`
   - Build at least one example: `cd examples/device/cdc_msc && make BOARD=stm32f407disco all`
   - Verify changed files look correct: `git diff --stat`
5. **Leave files unstaged** for maintainer to review, modify if needed, and commit with message: `Bump version to X.Y.Z`
6. **After maintainer commits**: Create annotated tag with `git tag -a vX.Y.Z -m "Release X.Y.Z"`
7. Push commit and tag: `git push origin <branch> && git push origin vX.Y.Z`
8. Create GitHub release from the tag with changelog content

## Repository Structure Quick Reference
```
├── src/                  # Core TinyUSB stack
│   ├── class/            # USB device classes (CDC, HID, MSC, Audio, etc.)
│   ├── portable/         # MCU-specific drivers (organized by vendor)
│   ├── device/           # USB device stack core
│   ├── host/             # USB host stack core
│   └── common/           # Shared utilities (FIFO, etc.)
├── examples/             # Example applications
│   ├── device/           # Device examples (cdc_msc, hid_generic, etc.)
│   ├── host/             # Host examples
│   └── dual/             # Dual-role examples
├── hw/bsp/               # Board Support Packages
│   └── FAMILY/boards/    # Board-specific configurations
├── test/unit-test/       # Unit tests using Ceedling
├── tools/                # Build and utility scripts
└── docs/                 # Sphinx documentation
```

#### Build Time Reference
- **Dependency fetch**: <1 second
- **Single example build**: 1-3 seconds
- **Unit tests**: ~4 seconds
- **Documentation build**: ~2.5 seconds
- **Full board examples**: 15-20 seconds
- **Toolchain installation**: 2-5 minutes (one-time)

#### Key Files to Know
- `tools/get_deps.py`: Manages dependencies for MCU families
- `tools/build.py`: Builds multiple examples, supports make/cmake
- `src/tusb.h`: Main TinyUSB header file
- `src/tusb_config.h`: Configuration template
- `examples/device/cdc_msc/`: Most commonly used example for testing
- `test/unit-test/project.yml`: Ceedling test configuration

#### Debugging Build Issues
- **Missing compiler**: Install `gcc-arm-none-eabi` package
- **Missing dependencies**: Run `python3 tools/get_deps.py FAMILY`
- **Board not found**: Check `hw/bsp/FAMILY/boards/` for valid board names
- **objcopy errors**: Often non-critical in full builds, try individual example builds

#### Working with USB Device Classes
- **CDC (Serial)**: `src/class/cdc/` - Virtual serial port
- **HID**: `src/class/hid/` - Human Interface Device (keyboard, mouse, etc.)
- **MSC**: `src/class/msc/` - Mass Storage Class (USB drive)
- **Audio**: `src/class/audio/` - USB Audio Class
- Each class has device (`*_device.c`) and host (`*_host.c`) implementations

#### MCU Family Support
- **STM32**: Largest support (F0, F1, F2, F3, F4, F7, G0, G4, H7, L4, U5, etc.)
- **Raspberry Pi**: RP2040, RP2350 with PIO-USB host support
- **NXP**: iMXRT, Kinetis, LPC families
- **Microchip**: SAM D/E/G/L families
- Check `hw/bsp/` for complete list and `docs/reference/boards.rst` for details

### Code Style Guidelines

#### General Coding Standards
- Use C99 standard
- Memory-safe: no dynamic allocation
- Thread-safe: defer all interrupt events to non-ISR task functions
- 2-space indentation, no tabs
- Use snake_case for variables/functions
- Use UPPER_CASE for macros and constants
- Follow existing variable naming patterns in files you're modifying
- Include proper header comments with MIT license
- Add descriptive comments for non-obvious functions

#### Best Practices
- When including headers, group in order: C stdlib, tusb common, drivers, classes
- Always check return values from functions that can fail
- Use TU_ASSERT() for error checking with return statements
- Follow the existing code patterns in the files you're modifying

Remember: TinyUSB is designed for embedded systems - builds are fast, tests are focused, and the codebase is optimized for resource-constrained environments.
