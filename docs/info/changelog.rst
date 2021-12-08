*********
Changelog
*********

0.12.0
======

- add CFG_TUSB_OS_INC_PATH for os include path

Device Controller Driver (DCD)
------------------------------

- Getting device stack to pass USB Compliance Verification test (chapter9, HID, MSC). Ports are tested:
  nRF, SAMD 21/51, rp2040, stm32f4, Renesas RX, iMXRT, ESP32-S2/3, Kinetic KL25/32, DA146xx
- Added dcd_edpt_close_all() for switching configuration
- [Transdimension] Support dcd_edpt_xfer_fifo() with auto wrap over if fifo buffer is 4K aligned and size is multiple of 4K.
- [DA146xx] Improve vbus, reset, suspend, resume detection, and remote wakeup.

Device Stack
------------

- Add new network driver Network Control Model (CDC-NCM), update net_lwip_webserver to work with NCM (need re-configure example)
- Add new USB Video Class UVC 1.5 driver and video_capture example ((work in progress)
- Fix potential buffer overflow for HID, bluetooth drivers

Host Controller Driver (HCD)
----------------------------

No notable changes

Host Stack
----------

No notable changes

0.11.0 (2021-08-29)
===================

- Add host/hid_controller example: only worked/tested with Sony PS4 DualShock controller
- Add device/hid_boot_interface example
- Add support for Renesas CCRX toolchain for RX mcu

Device Controller Driver (DCD)
------------------------------

- Add new DCD port for SAMx7x (E70, S70, V70, V71)
- Add new mcu K32L2Bxx
- Add new mcu GD32VF103
- Add new mcu STM32l151
- Add new mcu SAML21
- Add new mcu RX65n RX72n
- Fix NUC120/121/126 USBRAM can only be accessed in byte manner. Also improve set_address & disable sof
- Add Suspend/Resume handling for Renesas RX family.
- Fix DA1469x no VBUS startup

Synopsys
^^^^^^^^

- Fix Synopsys set address bug which could cause re-enumeration failed
- Fix dcd_synopsys driver integer overflow in HS mode (issue #968)

nRF5x
^^^^^

- Add nRF5x suspend, resume and remote wakeup
- Fix nRF5x race condition with TASKS_EP0RCVOUT

RP2040
^^^^^^

- Add RP2040 suspend & resume support
- Implement double buffer for both host and device (#891). Howver device EPOUT is still single bufferred due to techinical issue with short packet 

Device Stack
------------

USBD
^^^^

- Better support big endian mcu
- Add tuh_inited() and tud_inited(), will separte tusb_init/inited() to tud/tuh init/inited
- Add dcd_attr.h for defining common controller attribute such as max endpoints

Bluetooth
^^^^^^^^^

- Fix stridx error in descriptor template

DFU
^^^

- Enhance DFU implementation to support multiple alternate interface and better support bwPollTimeout
- Rename CFG_TUD_DFU_MODE to simply CFG_TUD_DFU 

HID
^^^

- Fix newline usage keyboard (ENTER 0x28)
- Better support Hid Get/Set report
- Change max gamepad support from 16 to 32 buttons

MIDI
^^^^

- Fix midi available
- Fix midi data
- Fix an issue when calling midi API when not enumerated yet

UAC2
^^^^

- Fix bug and enhance of UAC2
 
Vendor
^^^^^^

- Fix vendor fifo deadlock in certain case
- Add tud_vendor_n_read_flush

Host Controller Driver (HCD)
----------------------------

RP2040
^^^^^^

- Implement double bufferred to fix E4 errata and boost performance
- Lots of rp2040 update and enhancment

Host Stack
----------

- Major update and rework most of host stack, still needs more improvement
- Lots of improvement and update in parsing configuration and control
- Rework and major update to HID driver. Will default to enable boot interface if available
- Sepearate CFG_TUH_DEVICE_MAX and CFG_TUH_HUB for better management and reduce SRAM usage

0.10.1 (2021-06-03)
===================

- rework rp2040 examples and CMake build, allow better integration with pico-sdk

Host Controller Driver (HCD)
----------------------------

- Fix rp2040 host driver: incorrect PID with low speed device with max packet size of 8 bytes
- Improve hub driver
- Remove obsolete hcd_pipe_queue_xfer()/hcd_pipe_xfer()
- Use hcd_frame_number() instead of micro frame
- Fix OHCI endpoint address and xferred_bytes in xfer complete event

0.10.0 (2021-05-28)
===================

- Rework tu_fifo_t with separated mutex for read and write, better support DMA with read/write buffer info. And constant address mode
- Improve audio_test example and add audio_4_channel_mic example
- Add new dfu example
- Remove pico-sdk from submodule

Device Controller Driver (DCD)
------------------------------

- Add new DCD port for Silabs EFM32GG12 with board Thunderboard Kit (SLTB009A)
- Add new DCD port Renesas RX63N, board GR-CITRUS
- Add new (optional) endpoint API dcd_edpt_xfer_fifo
- Fix build with nRF5340
- Fix build with lpc15 and lpc54
- Fix build with lpc177x_8x
- STM32 Synopsys: greatly improve Isochronous transfer with edpt_xfer_fifo API
- Support LPC55 port1 highspeed
- Add support for Espressif esp32s3
- nRF: fix race condition that could cause drop packet of Bulk OUT transfer

USB Device Driver (USBD)
------------------------

- Add new (optional) endpoint ADPI usbd_edpt_xfer_fifo

Device Class Driver
-------------------

CDC

- [Breaking] tud_cdc_peek(), tud_vendor_peek() no longer support random offset and dropped position parameter.

DFU

- Add new DFU 1.1 class driver (WIP)

HID

- Fix keyboard report descriptor template
- Add more hid keys constant from 0x6B to 0xA4

- [Breaking] rename API
  - HID_PROTOCOL_NONE/KEYBOARD/MOUST to HID_ITF_PROTOCOL_NONE/KEYBOARD/MOUSE
  - tud_hid_boot_mode() to tud_hid_get_protocol()
  - tud_hid_boot_mode_cb() to tud_hid_set_protocol_cb()

MIDI

- Fix MIDI buffer overflow issue

- [Breaking] rename API
  - Rename tud_midi_read() to tud_midi_stream_read()
  - Rename tud_midi_write() to tud_midi_stream_write()
  - Rename tud_midi_receive() to tud_midi_packet_read()
  - Rename tud_midi_send() to tud_midi_packet_write()

Host Controller Driver (HCD)
----------------------------

- No noticable changes

USB Host Driver (USBH)
----------------------

- No noticable changes

Host Class Driver
-----------------

- HID: Rework host hid driver, basically everything changes


0.9.0 (2021-03-12)
==================

Device Stack
------------

Device Controller Driver (DCD)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

RP2040

- Fix endpoint buffer reallocation overrun problem
- Fix osal_pico queue overflow in initialization
- Fix Isochronous endpoint buffer size in transfer
- Optimize hardware endpoint struct to reduce RAM usage
- Fix enum walkaround forever check for SE0 when pull up is disabled

Sony CXD56

- Pass the correct speed on Spresense
- Fix setup processed flag

NXP Transdimention

- Update dcd_init() to reset controller to device mode

USB Device Driver (USBD)
^^^^^^^^^^^^^^^^^^^^^^^^

- Fix issue with status zlp (tud_control_status) is returned by class driver with SET/CLEAR_FEATURE for endpoint.
- Correct endpoint size check for fullspeed bulk, can be 8, 16, 32, 64
- Ack SET_INTERFACE even if it is not implemented by class driver.

Device Class Driver
^^^^^^^^^^^^^^^^^^^

DFU Runtime

- rename dfu_rt to dfu_runtime for easy reading

CDC

- Add tud_cdc_send_break_cb() to support break request
- Improve CDC receive, minor behavior changes: when tud_cdc_rx_wanted_cb() is invoked wanted_char may not be the last byte in the fifo

HID

- [Breaking] Add itf argument to hid API to support multiple instances, follow API has signature changes

  - tud_hid_descriptor_report_cb()
  - tud_hid_get_report_cb()
  - tud_hid_set_report_cb()
  - tud_hid_boot_mode_cb()
  - tud_hid_set_idle_cb()

- Add report complete callback tud_hid_report_complete_cb() API
- Add DPad/Hat support for HID Gamepad

  - `TUD_HID_REPORT_DESC_GAMEPAD()` now support 16 buttons, 2 joysticks, 1 hat/dpad
  - Add hid_gamepad_report_t along with `GAMEPAD_BUTTON_` and `GAMEPAD_HAT_` enum
  - Add Gamepad to hid_composite / hid_composite_freertos example

MIDI

- Fix dropping MIDI sysex message when fifo is full
- Fix typo in tud_midi_write24(), make example less ambigous for cable and channel
- Fix incorrect endpoint descriptor length, MIDI v1 use Audio v1 which has 9-byte endpoint descriptor (instead of 7)

Host Stack
----------

Host Controller Driver (HCD)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Add rhport to hcd_init()
- Improve EHCI/OHCI driver abstraction

  - Move echi/ohci files to portable/
  - Rename hcd_lpc18_43 to hcd_transdimension
  - Sub hcd API with hcd_ehci_init(), hcd_ehci_register_addr()
  
- Update NXP transdimention hcd_init() to reset controller to host mode

  - Ported hcd to rt10xx

USB Host Driver (USBH)
^^^^^^^^^^^^^^^^^^^^^^

- No noticeable changes to usbh

Host Class Driver
^^^^^^^^^^^^^^^^^

MSC

- Rename tuh_msc_scsi_inquiry() to tuh_msc_inquiry()
- Rename tuh_msc_mounted_cb/tuh_msc_unmounted_cb to tuh_msc_mount_cb/tuh_msc_unmount_cb to match device stack naming
- Change tuh_msc_is_busy() to tuh_msc_ready()
- Add read10 and write10 function: tuh_msc_read10(), tuh_msc_write10()
- Read_Capacity is invoked as part of enumeration process
- Add tuh_msc_get_block_count(), tuh_msc_get_block_size()
- Add CFG_TUH_MSC_MAXLUN (default to 4) to hold lun capacities

Others
------

- Add basic support for rt-thread OS
- Change zero bitfield length to more explicit padding
- Build example now fetch required submodules on the fly while running `make` without prio submodule init for mcu drivers
- Update pico-sdk to v1.1.0

**New Boards**

- Microchip SAM E54 Xplained Pro
- LPCXpresso 55s28
- LPCXpresso 18s37


0.8.0 (2021-02-05)
==================

Device Controller Driver
------------------------

- Added new device support for Raspberry Pi RP2040
- Added new device support for NXP Kinetis KL25ZXX
- Use dcd_event_bus_reset() with link speed to replace bus_signal

- ESP32-S2:
  - Add bus suspend and wakeup support
  
- SAMD21:
  - Fix (walkaround) samd21 setup_packet overflow by USB DMA
  
- STM32 Synopsys:
  - Rework USB FIFO allocation scheme and allow RX FIFO size reduction
  
- Sony CXD56
  - Update Update Spresense SDK to 2.0.2
  - Fix dcd issues with setup packets
  - Correct EP number for cdc_msc example

USB Device
----------

**USBD**

- Rework usbd control transfer to have additional stage parameter for setup, data, status
- Fix tusb_init() return true instead of TUSB_ERROR_NONE
- Added new API tud_connected() that return true after device got out of bus reset and received the very first setup packet

**Class Driver**

- CDC
  - Allow to transmit data, even if the host does not support control line states i.e set DTR
  
- HID
  - change default CFG_TUD_HID_EP_BUFSIZE from 16 to 64
  
- MIDI
  - Fix midi sysex sending bug
  
- MSC
  - Invoke only scsi complete callback after status transaction is complete.
  - Fix scsi_mode_sense6_t padding, which cause IAR compiler internal error.
  
- USBTMC
  - Change interrupt endpoint example size to 8 instead of 2 for better compatibility with mcu

**Example**

- Support make from windows cmd.exe
- Add HID Consumer Control (media keys) to hid_composite & hid_composite_freertos examples


USB Host
--------

No noticeable changes to host stack

New Boards
----------

- NXP/Freescale Freedom FRDM-KL25Z
- Feather Double M33 express
- Raspberry Pi Pico
- Adafruit Feather RP2040
- Adafruit Itsy Bitsy RP2040
- Adafruit QT RP2040
- Adfruit Feather ESP32-S2
- Adafruit Magtag 29" Eink
- Adafruit Metro ESP32-S2
- Adafruit PyBadge
- Adafruit PyPortal
- Great Scott Gadgets' LUNA D11 & D21


0.7.0 (2020-11-08)
==================

Device Controller Driver
------------------------

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


USB Device
----------

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
  - CFG_TUD_HID_BUFSIZE to CFG_TUD_HID_EP_BUFSIZE
  - CFG_TUD_CDC_EPSIZE to CFG_TUD_CDC_EP_BUFSIZE
  - CFG_TUD_MSC_BUFSIZE to CFG_TUD_MSC_EP_BUFSIZE
  - CFG_TUD_MIDI_EPSIZE to CFG_TUD_MIDI_EP_BUFSIZE
  
- HID:
  - Fix gamepad template descriptor
  - Add multiple HID interface API
  - Add extra comma to HID_REPORT_ID

USB Host
--------

- Rework USB host stack (still work in progress)
   - Fix compile error with pipehandle
   - Rework usbh control and enumeration as non-blocking
   
- Improve Hub, MSC, HID host driver

Examples
--------

- Add new hid_composite_freertos
- Add new dynamic_configuration to demonstrate how to switch configuration descriptors
- Add new hid_multiple_interface

- Enhance `net_lwip_webserver` example
  - Add multiple configuration: RNDIS for Windows, CDC-ECM for macOS (Linux will work with both)
  - Update lwip to STABLE-2_1_2_RELEASE for net_lwip_webserver
  
- Added new Audio example: audio_test uac2_headsest

New Boards
----------

- Espressif ESP32-S2: saola_1, kaluga_1
- STM32: F746 Nucleo, H743 Eval, H743 Nucleo, F723 discovery, stlink v3 mini, STM32L4r5 Nucleo
- Dialog DA1469x dk pro and dk usb
- Microchip: Great Scoot Gadgets' LUNA, samd11_xplained, D5035-01, atsamd21 xplained pro
- nRF: ItsyBitsy nRF52840


0.6.0 (2020-03-30)
==================

Added **CONTRIBUTORS.md** to give proper credit for contributors to the stack. Special thanks to `Nathan Conrad <https://github.com/pigrew>`__ , `Peter Lawrence <https://github.com/majbthrd>`__ , `William D. Jones <https://github.com/cr1901>`__ and `Sean Cross <https://github.com/xobs>`__ and others for spending their precious time to add lots of features and ports for this release.

Added
-----

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

Changed
-------

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


0.5.0 (2019-06)
===============

First release, device stack works great, host stack works but still need improvement.

- Special thanks to @adafruit team, especially @tannewt to help out immensely to rework device stack: simplify osal & control transfer, adding SAMD21/SAMD51 ports, writing porting docs, adding MIDI class support etc...
- Thanks to @cr1901 for adding STM32F4 port.
- Thanks to @PTS93 and @todbot for HID raw API
