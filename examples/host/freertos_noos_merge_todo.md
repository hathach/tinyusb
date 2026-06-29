# Host FreeRTOS/No-OS Example Merge TODO

Goal: shrink paired host no-OS and FreeRTOS examples into one directory where the build selects the RTOS with `RTOS=freertos` / `-DRTOS=freertos`.

- [x] Merge `cdc_msc_hid_freertos` into `cdc_msc_hid`.
- [x] Merge `msc_file_explorer_freertos` into `msc_file_explorer`.
- [x] Add RTOS-aware `skip.txt` entries for host FreeRTOS-only exclusions.
- [x] Add ESP-IDF component metadata to the merged host examples.
- [x] Remove merged `_freertos` entries from the host CMake example list.
- [x] Keep Make defaulting to no-OS while allowing `RTOS=freertos`.
- [x] Verify CMake builds for no-OS and FreeRTOS host examples.
