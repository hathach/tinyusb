# Examples

## Clone this repo

```
$ git clone https://github.com/hathach/tinyusb tinyusb
$ cd tinyusb
```

## Fetch submodule MCUs drivers

TinyUSB examples includes external repos aka submodules to provide low-level MCU peripheral's driver to compile with. Therefore we will firstly fetch those mcu driver repo by running this command in the top folder repo

```
$ git submodule update --init --recursive
```

It will takes a bit of time due to the number of supported MCUs, luckily we only need to do this once. Or if you only want to test with a specific mcu, you could only update its driver submodule 

## Build

[Here is the list of supported Boards](docs/boards.md) that should work out of the box with provided examples (hopefully).
To build example, first change directory to example folder. 

```
$ cd examples/device/cdc_msc
```

Then compile with `make BOARD=[your_board] all`, for example

```
$ make BOARD=feather_nrf52840_express all
```

### Debug Log

### Log Level

Should you have an issue running example and/or submitting an bug report. You could enable TinyUSB built-in debug logging with optional `LOG=`. LOG=1 will only print out error message, LOG=2 print more information with on-going events. LOG=3 or higher is not used yet. 

```
$ make LOG=2 BOARD=feather_nrf52840_express all
```

### Logger

By default log message is printed via on-board UART which is slow and take lots of CPU time comparing to USB speed. If your board support on-board/external JLink debugger, it would be more efficient to use it with [RTT protocol](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/) for logging. To do that add option `LOGGER=rtt` to build command e.g

```
$ make LOG=2 LOGGER=rtt BOARD=feather_nrf52840_express all
```

The log can be retrieved by JLink RTT Viewer/Client/Logger software which is bundled with their JLink driver.

## Flash

`flash` target will use the default on-board debugger (jlink/cmsisdap/stlink/dfu) to flash the binary, please install those support software in advance. Some board use bootloader/DFU via serial which is required to pass to make command

```
$ make BOARD=feather_nrf52840_express flash
$ make SERIAL=/dev/ttyACM0 BOARD=feather_nrf52840_express flash
```

Since jlink can be used with most of the boards, there is also `flash-jlink` target for your convenience.

```
$ make BOARD=feather_nrf52840_express flash-jlink
```

Some board use uf2 bootloader for drag & drop in to mass storage device, uf2 can be generated with `uf2` target

```
$ make BOARD=feather_nrf52840_express all uf2
```
