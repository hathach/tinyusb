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

## Flash

`flash` target will use the on-board debugger (jlink/cmsisdap/stlink/dfu) to flash the binary. We should install those debugger/programmer software in advance. Furthermore, since external jlink can be used with most of the board, there is also `flash-jlink` target for your convenience.

```
$ make BOARD=feather_nrf52840_express flash
$ make BOARD=feather_nrf52840_express flash-jlink
```

Some board use uf2 bootloader for drag & drop in to mass storage device, uf2 can be generated with `uf2` target

```
$ make BOARD=feather_nrf52840_express all uf2
```

