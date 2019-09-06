# Examples

## Clone this repo

```
$ git clone https://github.com/hathach/tinyusb tinyusb
$ cd tinyusb
```

## Fetch submodule MCUs drivers

TinyUSB examples includes external repos aka submodules to provide low-level MCU peripheral's driver to compile with. Therefore we will firstly fetch those mcu driver repo by running this command in the top folder repo

```
$ git submodule update --init --rescursive
```

It will takes a bit of time due to the number of supported MCUs, luckily we only need to do this once.

## Build

[Here is the list of supported Boards](docs/boards.md) that should work out of the box with provided examples. 
 
To build example, go to its folder project then type `make BOARD=[our_board] all` e.g

```
$ cd examples/device/cdc_msc_hid
$ make BOARD=feather_nrf52840_express all
```

## Flash

TODO: write more on flashing

```
$ make BOARD=feather_nrf52840_express flash
```

