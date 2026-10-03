# Host driver tests

These fixtures run through CTest and the host-test pre-commit hook:

```sh
cmake -S test/unit-test/host -B build/host-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-test
ctest --test-dir build/host-test --output-on-failure
```

Use GCC or Clang (MinGW on Windows, with runtime DLLs on PATH).

Four EHCI configurations cover generic EHCI, ChipIdea with ISO disabled/enabled,
and ChipIdea ISO with payload D-cache maintenance enabled.
The enabled fixture uses the actual HCD with simulated DMA writeback and
FRINDEX. It checks native FS, split transfers, HS, single-request ownership,
shared QH/qTD allocation and exhaustion, early completion, TD reuse, errors,
cancellation, endpoint phase, and counter/frame-list wrap. Every-microframe
HS service is checked with exactly one TD per endpoint.

Assertions remain enabled in release builds; each test has a 10-second limit.
Static simulated DMA addresses are below 4 GiB. These tests do not emulate
DMA/cache coherency or real USB timing; hardware validation is still needed.
The EHCI fixtures are skipped on macOS, which cannot guarantee low static
addresses.
