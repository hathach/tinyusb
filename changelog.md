# TinyUSB Changelog

## Master branch (WIP)

### Breaking

- TinyUSB does not directly implement USB IRQ Handler function anymore. Application must implement IRQ Handler and invoke `tud_irq_handler(rhport)`. This is due to:
  
  - IRQ Handler name can be different across system depending on the startup
  - Some OS need to execute enterISR()/exitISR() to work properly, also tracing tool may need to insert trace ISR enter/exit to record usb event
  - Give application full control of IRQ handler, can be useful e.g signaling there is new usb event without constant polling

### MCU

- All default IRQ Handler is renamed to `dcd_irq_handler()`

## 0.6.0 - 2019.03.30

Added **CONTRIBUTORS.md** to give proper credit for contributors to the stack. Special thanks to [Nathan Conrad](https://github.com/pigrew), [Peter Lawrence](https://github.com/majbthrd) and [William D. Jones](https://github.com/cr1901) and others for spending their precious time to add lots of features and ports for this release.

### Added

**MCU**

- Added support for Microchip SAMG55
- Added support for Nordic nRF52833
- Added support for Nuvoton: NUC120, NUC121/NUC125, NUC126, NUC505
- Added support for NXP LPC: 51Uxx, 54xxx, 55xx
- Added support for NXP iMXRT: RT1011, RT1015, RT1021, RT1052, RT1062, RT1064
- Added support for Sony CXD56 (Spresense)
- Added support for STM32: L0, F0, F1, F2, F3, F4, F7, H7
- Added support for TI MSP430
- Added support for ValentyUSB's eptri

**Class Driver**

- Added DFU Runtime class driver
- Added Network class driver with RNDIS, CDC-ECM, CDC-EEM (work in progress)
- Added USBTMC class driver
- Added WebUSB class driver using vendor-specific class
- Added multiple instances support for CDC and MIDI
- Added a handful of unit test with Ceedling.
- Added LOG support for debugging with CFG_TUSB_DEBUG
- Added `tud_descriptor_bos_cb()` for BOS descriptor (required for USB 2.1)
- Added `dcd_edpt0_status_complete()` as optional API for DCD

**Examples**

Following examples are added:

- board_test
- cdc_dual_ports
- dfu_rt
- hid_composite
- net_lwip_webserver
- usbtmc
- webusb_serial

**Boards**

Following boards are added:

- adafruit_clue
- arduino_nano33_ble
- circuitplayground_bluefruit
- circuitplayground_express
- feather_m0_express
- feather_nrf52840_sense
- feather_stm32f405
- fomu
- itsybitsy_m0
- itsybitsy_m4
- lpcxpresso11u37
- lpcxpresso1549
- lpcxpresso51u68
- lpcxpresso54114
- lpcxpresso55s69
- mbed1768
- mimxrt1010_evk
- mimxrt1015_evk
- mimxrt1020_evk
- mimxrt1050_evkb
- mimxrt1060_evk
- mimxrt1064_evk
- msp_exp430f5529lp
- ngx4330
- nrf52840_mdk_dongle
- nutiny_nuc121s
- nutiny_nuc125s
- nutiny_nuc126v
- nutiny_sdk_nuc120
- nutiny_sdk_nuc505
- pca10059
- pca10100
- pyboardv11
- raytac_mdbt50q_rx
- samg55xplained
- seeeduino_xiao
- spresense
- stm32f070rbnucleo
- stm32f072disco
- stm32f103bluepill
- stm32f207nucleo
- stm32f401blackpill
- stm32f411blackpill
- stm32f411disco
- stm32f412disco
- stm32f767nucleo
- stm32h743nucleo
- stm32l0538disco
- stm32l476disco
- teensy_40

### Changed

- Changed `tud_descriptor_string_cb()` to have additional Language ID argument
- Merged hal_nrf5x.c into dcd_nrf5x.c
- Merged dcd_samd21.c and dcd_samd51.c into dcd_samd.c
- Generalized dcd_stm32f4.c to dcd_synopsys.c
- Changed cdc_msc_hid to cdc_msc (drop hid) due to limited endpoints number of some MCUs 
- Improved DCD SAMD stability, fix missing setup packet occasionally
- Improved usbd/usbd_control with proper hanlding of zero-length packet (ZLP)
- Improved STM32 DCD FSDev
- Improved STM32 DCD Synopsys
- Migrated CI from Travis to Github Action
- Updated nrfx submodule to 2.1.0
- Fixed mynewt osal queue definition
- Fixed cdc_msc_freertos example build for all MCUs

## 0.5.0 (Initial Release) - 2019.07.10

First release, device stack works great, host stack works but still need improvement. 
- Special thanks to @adafruit team, especially @tannewt to help out immensely to rework device stack: simplify osal & control transfer, adding SAMD21/SAMD51 ports, writing porting docs, adding MIDI class support etc... 
- Thanks to @cr1901 for adding STM32F4 port.
- Thanks to @PTS93 and @todbot for HID raw API
