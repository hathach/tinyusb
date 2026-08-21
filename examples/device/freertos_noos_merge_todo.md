# FreeRTOS/No-OS Example Merge TODO

Goal: shrink paired no-OS and FreeRTOS examples into one directory where the build selects the RTOS with `RTOS=freertos` / `-DRTOS=freertos`.

- [x] Add RTOS-aware `skip.txt` entries with `rtos:<name> <selector>`.
- [x] Merge `audio_test_freertos` into `audio_test`.
- [x] Merge `audio_4_channel_mic_freertos` into `audio_4_channel_mic`.
- [x] Add FreeRTOS support to `uac2_speaker_fb`.
- [x] Merge `cdc_msc_freertos` into `cdc_msc`.
- [x] Merge `hid_composite_freertos` into `hid_composite`.
- [x] Merge `midi_test_freertos` into `midi_test`.
- [x] Add ESP-IDF component metadata for merged examples that support Espressif.
- [x] Update Espressif build discovery for merged example names.
- [x] Remove merged `_freertos` entries from the device CMake example list.
