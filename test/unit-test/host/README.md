# Host driver tests

These fixtures are registered with CTest by the unit-test CMake project.
The `host-test` pre-commit hook also runs them in CI when source or test files
change. They can be built separately from the Ceedling suites:

```sh
cmake -S test/unit-test/host -B build/host-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-test --parallel 2
ctest --test-dir build/host-test --output-on-failure
```

Use GCC or Clang on Linux, or MinGW on Windows with its runtime DLLs on PATH.
Set `CMAKE_C_COMPILER` when configuring to select a compiler explicitly.

- **EHCI:** actual driver code with simulated descriptor writeback and
  FRINDEX. Covers native FS, split transactions, HS, descriptor reuse,
  completion ordering/errors, cancellation, reset debounce and frame wrap.
  Builds with ISO disabled and queue depths 1, 2 and 4. Static DMA fixtures
  are linked below 4 GiB; hardware descriptor layouts are asserted separately
  from the native-pointer software tail.
- **USBH/audio:** actual stack and audio drivers with a stub HCD. Covers
  buffer ownership, queue capacity, callback dispatch, failed submissions,
  abort/close, capture/playback refill, underrun silence, feedback and reset
  timing. Builds generic, EHCI and MAX3421 configurations at depths 1, 2 and 4.

The 13 CTest cases retain assertions in release builds and have bounded
execution times. They do not emulate DMA/cache coherency or USB wire timing;
those still require hardware tests.
