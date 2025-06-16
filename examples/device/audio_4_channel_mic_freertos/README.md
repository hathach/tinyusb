# How to build example for Esp32s3
1. Load idf environment variables (eg. using the esp-idf alias `get_idf` if configured)

2. cd into examples directory
```
$ cd /tinyusb/examples/device/audio_4_channel_mic_freertos
```

3. Run cmake in project directory specifying the board
```
$ cmake -DBOARD=espressif_s3_devkitc -B build -G Ninja .
$ ninja.exe -C build
```

4. Flash the binary onto the esp32-s3 by copy-paste of the full command output by the esp-idf build system replacing **(PORT)** with eg. /dev/ttyUSB0

eg.

> /home/kaspernyhus/.espressif/python_env/idf4.4_py3.8_env/bin/python ../../../../esp-idf/components/esptool_py/esptool/esptool.py -p /dev/ttyUSB0 -b 460800 --before default_reset --after hard_reset --chip esp32s3  write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x0 _build/espressif_s3_devkitc/bootloader/bootloader.bin 0x8000 _build/espressif_s3_devkitc/partition_table/partition-table.bin 0x10000 _build/espressif_s3_devkitc/audio_4_channel_mic_freertos.bin
