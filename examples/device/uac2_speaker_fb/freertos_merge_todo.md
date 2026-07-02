# UAC2 Speaker FreeRTOS Merge TODO

Goal: keep `uac2_speaker_fb` as one example that can build in no-OS mode by default and FreeRTOS mode with the existing `RTOS=freertos` / `-DRTOS=freertos` build switch.

- [x] Keep the existing no-OS speaker behavior unchanged.
- [x] Fold the FreeRTOS task setup from `uac2_speaker_freertos` into `src/main.c`.
- [x] Add ESP-IDF component metadata and FreeRTOS sdkconfig defaults.
- [x] Let CMake choose the RTOS through `${RTOS}` instead of hardcoding `noos`.
- [x] Keep Make defaulting to no-OS while allowing `RTOS=freertos`.
- [x] Keep Espressif build discovery aware of the merged example.
- [x] Verify at least CMake configure/build targets for no-OS and FreeRTOS.
