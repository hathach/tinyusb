# TinyUSB Changelog

## 0.7.0 - 2020.11.08

### Device Controller Driver

- Added new support for Espressif ESP32-S2
- Added new support for Dialog DA1469x
- Enhance STM32 Synopsys
  - Support bus events disconnection/suspend/resume/wakeup
  - Improve transfer performance with optimizing xfer and fifo size
  - Support Highspeed port (OTG_HS) with both internal and external PHY
  - Support multiple usb ports with rhport=1 is highspeed on selected MCUs e.g H743, F23. It is possible to have OTG_HS to run on Fullspeed PHY (e.g lacking external PHY)
  - Add ISO transfer, fix odd/even frame
  - Fix FIFO flush during stall
  - Implement dcd_edpt_close() API
  - Support F105, F107
- Enhance STM32 fsdev
  - Improve dcd fifo allocation
  - Fix ISTR race condition
  - Support remap USB IRQ on supported MCUs
  - Implement dcd_edpt_close() API
- Enhance NUC 505: enhance set configure behavior
- Enhance SAMD
  - Fix race condition with setup packet
  - Add SAMD11 option `OPT_MCU_SAMD11`
  - Add SAME5x option `OPT_MCU_SAME5X`
- Fix SAMG control data toggle and stall race condition
- Enhance nRF
  - Fix hanged when tud_task() is called within critical section (disabled interrupt)
  - Fix disconnect bus event not submitted
  - Implement ISO transfer and dcd_edpt_close()

### USB Device

**USBD**

- Add new class driver for **Bluetooth HCI** class driver with example can be found in [mynewt-tinyusb-example](https://github.com/hathach/mynewt-tinyusb-example) since it needs mynewt OS to run with.
- Fix USBD endpoint usage racing condition with `usbd_edpt_claim()/usbd_edpt_release()`
- Added `tud_task_event_ready()` and `osal_queue_empty()`. This API is needed to check before enter low power mode with WFI/WFE
- Rename USB IRQ Handler to `dcd_int_handler()`. Application must define IRQ handler in which it calls this API.
- Add `dcd_connect()` and `dcd_disconnect()` to enable/disable internal pullup on D+/D- on supported MCUs.
- Add `usbd_edpt_open()`
- Remove `dcd_set_config()`
- Add *OPT_OS_CUMSTOM* as hook for application to overwrite and/or add their own OS implementation
- Support SET_INTERFACE, GET_INTERFACE request
- Add Logging for debug with optional uart/rtt/swo printf retarget or `CFG_TUSB_DEBUG_PRINTF` hook
- Add IAR compiler support
- Support multiple configuration descriptors. `TUD_CONFIG_DESCRIPTOR()` template has extra config_num as 1st argument
- Improve USB Highspeed support with actual link speed detection with `dcd_event_bus_reset()`
- Enhance class driver management
  - `usbd_driver_open()` add max length argument, and return length of interface (0 for not supported). Return value is used for finding appropriate driver
  - Add application implemented class driver via `usbd_app_driver_get_cb()`
  - IAD is handled to assign driver id
- Added `tud_descriptor_device_qualifier_cb()` callback
- Optimize `tu_fifo` bulk write/read transfer
- Forward non-std control request to class driver
- Let application handle Microsoft OS 1.0 Descriptors (the 0xEE index string)
- Fix OSAL FreeRTOS yield from ISR

**Class Drivers**

- USBNET: remove ACM-EEM due to lack of support from host
- USBTMC: fix descriptors when INT EP is disabled
- CDC:
  - Send zero length packet for end of data when needed
  - Add `tud_cdc_tx_complete_cb()` callback
  - Change tud_cdc_n_write_flush() return number of bytes forced to transfer, and flush when writing enough data to fifo
- MIDI:
  - Add packet interface
  - Add multiple jack descriptors
  - Fix MIDI driver for sysex
- DFU Runtime: fix response to SET_INTERFACE and DFU_GETSTATUS request
- Rename some configure macro to make it clear that those are used directly for endpoint transfer
  - CFG_TUD_HID_BUFSIZE to `CFG_TUD_HID_EP_BUFSIZE
  - CFG_TUD_CDC_EPSIZE to CFG_TUD_CDC_EP_BUFSIZE
  - CFG_TUD_MSC_BUFSIZE to CFG_TUD_MSC_EP_BUFSIZE
  - CFG_TUD_MIDI_EPSIZE to CFG_TUD_MIDI_EP_BUFSIZE
- HID:
  - Fix gamepad template descriptor
  - Add multiple HID interface API
  - Add extra comma to HID_REPORT_ID

### USB Host

- Rework USB host stack (still work in progress)
  - Fix compile error with pipehandle
  - Rework usbh control and enumeration as non-blocking
- Improve Hub, MSC, HID host driver

### Examples

- Add new hid_composite_freertos
- Add new dynamic_configuration to demonstrate how to switch configuration descriptors
- Add new hid_multiple_interface
- Enhance `net_lwip_webserver` example
  - Add multiple configuration: RNDIS for Windows, CDC-ECM for macOS (Linux will work with both)
  - Update lwip to STABLE-2_1_2_RELEASE for net_lwip_webserver
- Added new Audio example: audio_test uac2_headsest

### New Boards

- Espressif ESP32-S2: saola_1, kaluga_1
- STM32: F746 Nucleo, H743 Eval, H743 Nucleo, F723 discovery, stlink v3 mini, STM32L4r5 Nucleo
- Dialog DA1469x dk pro and dk usb
- Microchip: Great Scoot Gadgets' LUNA, samd11_xplained, D5035-01, atsamd21 xplained pro
- nRF: ItsyBitsy nRF52840

## 0.6.0 - 2020.03.30

Added **CONTRIBUTORS.md** to give proper credit for contributors to the stack. Special thanks to [Nathan Conrad](https://github.com/pigrew), [Peter Lawrence](https://github.com/majbthrd) and [William D. Jones](https://github.com/cr1901) and others for spending their precious time to add lots of features and ports for this release.

### Added

**MCUs**

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
