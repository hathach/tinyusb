*********
Changelog
*********

0.18.0
======

General
-------

- New MCUs:

  - Add esp32p4 OTG highspeed support
  - Add stm32 u0, c0, h7rs

- Better support dcache, make sure all usb-transferred buffer are cache line aligned and occupy full cache line
- Build ARM IAR with CircleCI
- Improve HIL with dual/host_info_to_device_cdc optional for pico/pico2, enable dwc2 dma test


API Changes
-----------

- Change signature of ``tusb_init(rhport, tusb_rhport_init_t*)``, tusb_init(void) is now deprecated but still available for backward compatibility
- Add new ``tusb_int_handler(rhport, in_isr)``
- Add time-related APIs: ``tusb_time_millis_api()`` and ``tusb_time_delay_ms_api()`` for non-RTOS, required for some ports/configuration
- New configuration macros:

  - ``CFG_TUD/TUH_MEM_DCACHE_ENABLE`` enable data cache sync for endpoint buffer
  - ``CFG_TUD/TUH_MEM_DCACHE_LINE_SIZE`` set cache line size
  - ``CFG_TUD/TUH_DWC2_SLAVE_ENABLE`` enable dwc2 slave mode
  - ``CFG_TUD/TUH_DWC2_DMA_ENABLE`` enable dwc2 dma mode


Controller Driver (DCD & HCD)
-----------------------------

- DWC2
  - Add DMA support for both device and host controller
  - Add host driver support including: full/high speed, control/bulk/interrupt (CBI) transfer, split CBI i.e FS/LS attached via highspeed hub, hub support

- RP2: implement dcd_edpt_iso_alloc() and dcd_edpt_iso_activate() for isochronous endpoint
- iMXRT1170 support M4 core

Device Stack
------------

- Vendor Fix  class reset
- NCM fix recursions in tud_network_recv_renew()
- Audio fix align issue of _audiod_fct.alt_setting
- UVC support format frame based
- Change dcd_dcache_() return type from void to bool
- HID add Usage Table for Physical Input Device Page (0x0F)

Host Stack
----------

- Fix an duplicated attach issue which cause USBH Defer Attach until current enumeration complete message

0.17.0
======

General
-------

- Improved CI: build both cmake and make. Make use of CircleCI for part of build process to speed up CI
- Add CodeQL Workflow for Code Security Analysis
- Add Clang compiler support
- Add default implementation for weak callbacks functions for better Keil compatibility
- Upgrade hardware-in-the-loop (HIL) testing with more boards and examples: including dual stack example

Controller Driver (DCD & HCD)
-----------------------------

- Chipidea

  - Support MCXA

- DWC2

  - Fix tickless issue with stm32f7: disable ULPI clock during sleep when using internal phy
  - Fix SOF interrupt handling
  - Fix fifo level half/empty issue
  - Add DWC2 Test Mode support.
  - for esp32 force disconnect/connect using USB_WRAP otg pad override

- FSDEV

  - Rewrite and Generalize driver to support non-stm32 mcu such as wch
  - Simplify PMA, HW FIFO access and bit manipulation for different access scheme 1x16, 2x16 and 32 bit
  - Add support for ch32 usbd e.g ch32v203
  - Add support for STM32G4 and STM32U5 microcontrollers.
  - Fix h5 (32-bit) errata 2.15.1: Buffer description table update completes after CTR interrupt triggers
  - ISO EP buffer allocation improvements, implement dcd_edpt_close_all()

  - Fix ch32v203 race condition and stability issue with

    - fix ch32v203 seems to unconditionally accept ZLP on EP0 OUT.
    - fix v203 race condition between rx bufsize and RX_STAT which cause PMAOVR, occurs with WRITE10
    - correctly handle setup prepare at dcd_edpt0_status_complete(), which fixes the race condition with windows where we could miss setup packet (setup bit set, but count = 0)

- MAX3421E

  - Add support for rp2040, esp32 (c3, c6, h2, etc..)
  - Add hcd_deinit() for max3421
  - Retry NAK handling next frame to reduce CPU and SPI bus usage
  - add cpuctl and pinctl to tuh_configure() option for max3421
  - Implement hcd abort transfer for Max3421
  - Properly Handle NAK Response in MAX3421E driver: correctly switch and skip writing to 2 FIFOs when NAK received. Otherwise, the driver may hang in certain conditions.

- MSP430: support non-bus-powered

- MUSB

  - Add support for Analoog devices: max32650, max32666, max32690, max3278002

- nRF

  - Fix dcd_edpt_open for iso endpoint
  - Handle ISOOUT CRC errors
  - Add compile support with old nordic sdk
  - Fix a few race conditions

- OHCI

  - Allow more than 16 devices

- RP2040

  - Correctly abort control transfer when new setup arrived. Due to RP2040-E2 only able to fix B2 or later
  - Implement hcd abort transfer for rp2040
  - Add support for rp2350

- RUSB2

  - Support ra2a1 pipe number scheme

- WCH CH32

  - Added support for USB OTG/FS and FSDev Driver. Update CH32V307 to allow manual select FS or HS driver.
  - Fixed various bugs in CH32v307 usbhs driver: endpoint handling and data transfer management.

Device Stack
------------

- Add tud_deinit() and class driver deinit() to deinitialize TinyUSB device stack.
- Add support for generic SOF callback.
- Add set address recovery time 2ms per USB spec.

- Audio

  - Add audio_test_freertos & audio_4_channel_mic_freertos
  - Improved support for Audio Class 2.0 (UAC2) with various bug fixes.
  - Add feedback by fifo counting.

- Bluetooth HCI

  - Issue ZLP on ACL IN ep when transfer is multiple of endpoint max packet size

- CDC

  - Add tud_cdc_configure_fifo() to make RX/TX buffer persistent (not clear when disconnected)
  - Add missing capability bit for CDC ACM serial break support
  - Enhanced CDC class with better handling of large data transmissions.
  - Add missing capability bit for CDC ACM serial break support

- HID

  - Added missing key codes for keypad
  - Added HID Lighting and Illumination functionality
  - Fixed issues in the HID class for more reliable device enumeration.
  - Support HID Mouse with absolute positioning
  - Use separate buffer for control SET_REPORT, fix conflict with interrupt endpoint out

- MSC: Added support for SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL

- Net

  - Rewrite of NCM device driver to improve throughput
  - removed obsolete tud_network_link_state_cb()

- USBTMC Added notification support

- Vendor

  - Migrate to new endpoint stream API, support non-buffered TX/RX
  - Add ZLP for write() when needed

- Video

  - Enhance UVC descriptors and example
  - Video Added support for USB Video Class (UVC) with MJPEG.
  - Fix multiple interfaces, add an example of 2ch video capture.
  - Fix race for tud_video_n_streaming check

Host Stack
----------

- Added tuh_deinit() to de-initialize TinyUSB host stack.
- Added support for new USB mass storage class APIs.
- Improved error handling and retry mechanisms for unstable devices.

- CDC Serial

  - Add support for ch34x
  - Allow to overwrite CFG_TUH_CDC_FTDI/CP210X/CH32X_VID_PID_LIST
  - Enhanced stability of CDC-ACM devices during enumeration.

- HID

  - Add tuh_hid_receive_abort()
  - Add tuh_hid_get_report()

- Hub

  - Prevent status request to invalid ep_num
  - Fix double status xfer
  - unroll hub removal

0.16.0
======

- New controller driver: MAX3421e (usb host shield), rusb2 (Renesas USB2.0), ChipIdea fullspeed
- New MCUs: MCXn9, nRF5340, STM32: G0, G4, L5, U575, U5A5, RA6m5, CH32F20x
- Add initial TypeC PowerDelivery support with STM32G4
- Remove submodules and use python script to manage repo dependencies #1947
- Add CMake support for most families and boards, move build file from tools/ to examples/build_system
- Add ETM trace support with JTrace for nrf52840, nrf5340, mcb1857, stm32h743eval, ra6m5
- [osal] Make it possible to override the osal_task_delay() in osal_none
- Add CDC+UAC2 composite device example
- Enhance Hardware-in-the-loop (HIL) testing with more boards: rp2040, stm32l412nucleo, stm32f746disco, lpcxpresso43s67

Controller Driver (DCD & HCD)
-----------------------------

- Add new ISO endpoint API: dcd_edpt_iso_alloc() and dcd_edpt_iso_activate()
- Remove legacy driver st/synopsys

- EHCI

  - [iMXRT] Add dache clean/invalidate when memory is in cacheable memory
  - Fix portsc write issue which cause problem with enumeration
  - Fix an issue when doing port reset write to portsc
  - Fix port change detect is not recognized when power on with attached device
  - Fix xfer failed with disconnected device as stalled
  - Fix error on EHCI causes xfer error in non-queued qhd which cause memory fault
  - Un-roll recursive hub removal with usbh queue
  - Fix issue when removing queue head
  - Implement hcd_edpt_abort_xfer()
  - use standard USB complete interrupt instead of custom chipidea async/period interrupt to be more compatible with other ehci implementation
  - refactor usb complete & error isr processing, merge, update. Fix EHCI QHD reuses QTD on wrong endpoint
  - Improve bus reset, fix send_setup() not carried out if halted previously
  - Fix clear qhd halted bit if not caused by STALL protocol to allow for next transfer

- ChipIdea Highspeed

  - Fix control transfer issue when previous status and new setup complete in the same isr frame
  - [imxrt] Add dcache support for cache region

- ChipIdea Fullspeed

  - Generalize ChipIdea Fullspeed driver for mcxn9 (port 0), kinetis

- nrf

  - Fix DMA race condition with ISO OUT transfer #1946
  - Add support for nRF5340 with pca10095 board

- Renesas rusb2

  - Generalize rusb2 driver for ra, rx mcus
  - rework both dcd and hcd for better multiple ports support
  - Add support for board with HS USB port: ra6m5 port1

- rp2040

  - [dcd] Make writes to SIE_CTRL aware of concurrent access
  - [hcd] add hcd_frame_number(), hcd_edpt_abort_xfer() for pio-usb host

- stm32 fsdev:

  - Add STM32L5 support
  - Implement dcd_edpt_iso_alloc() and dcd_edpt_iso_activate()

- OHCI

  - Allows configurable root hub ports, handles SMM mode (Ref OHCI spec 5.1.1.3.3) and Bios mode (Ref OHCI spec 5.1.1.3.4)
  - Fix FrameIntervalToggle must be toggled after we write the FrameInterval (Ref OHCI Spec 7.3.1)
  - Wait PowerOnToPowerGoodTime after we enable power of the RH ports (Ref OHCI Spec 7.4.1)
  - Generate port interrupts for devices already connected during init.
  - Fix issue when removing queue head
  - Disable MIE during IRQ processing and clear HccaDoneHead on completion as per OCHI Spec Page 80

Device Stack
------------

- Add optional hooks tud_event_hook_cb()
- Audio (UAC2)

  - Fix feedback EP buffer alignment.
  - Fix encoding, update example
  - Improve IN transfer

- Bluetooth

  - Add historical EP compatibility for Bluetooth HCI

- CDC

  - Fix line_coding alignment
  - Fix typo in cdc line coding enum

- MIDI

  - Fix stream_write() always writes system messages to cable 0
  - Fix incorrect NOTE_ON, NOTE_OFF definitions

- USBTMC: Fix tmc488 bit order

- Vendor: fix read()/write() race condition

- Video (UVC)

  - Add the capability for video class to handle a bulk endpoint in the streaming interface.

Host Stack
----------

- USBH

  - Add new APIs: tuh_interface_set(), tuh_task_event_ready(), tuh_edpt_abort_xfer(), tuh_rhport_reset_bus(), tuh_rhport_is_active()
  - Fix issue when device generate multiple attach/detach/attach when plugging in
  - Prefer application callback over built-in driver on transfer complete event
  - Correct hcd_edpt_clear_stall() API signature
  - Separate bus reset delay and contact debouncing delay in enumeration
  - Support usbh_app_driver_get_cb() for application drivers
  - Fix usbh enumeration removal race condition
  - Add optional hooks tuh_event_hook_cb()

- CDC

  - Breaking: change tuh_cdc_itf_get_info() to use tuh_itf_info_t instead of tuh_cdc_info_t
  - Fix cdc host enumeration issue when device does not support line request
  - Add support for vendor usb2uart serial: ftdi, cp210x, ch9102f
  - Improve sync control API e.g  tuh_cdc_set_control_line_state(), tuh_cdc_set_line_coding()

- HID

  - Add new APIs tuh_hid_send_report(), tuh_hid_itf_get_info(), tuh_hid_receive_ready(), tuh_hid_send_ready(), tuh_hid_set_default_protocol()
  - Change meaning of CFG_TUH_HID to total number of HID interfaces supported. Previously CFG_TUH_HID is max number of interfaces per device which is rather limited and consume more resources than needed.

- HUB

  - Fix handling of empty "status change" interrupt
  - Fix issue with hub status_change is not aligned

- MSC

  - Fix bug in tuh_msc_ready()
  - Fix host msc get maxlun not using aligned section memory

0.15.0
======

- Add codespell to detect typo
- Add support for fuzzing and bagde for oss-fuzz
- [osal]

  - Allow the use of non-static allocation for FreeRTOS
  - Fix FreeRTOS wrong task switch in some cases

- Fix tu_fifo memory overflown when repeatedly write to overwritable fifo (accumulated more than 2 depths)
- Better support for IAR (ARM) with ci build check for stm32 mcus.
- Fix Windows build for some mingw gnu make situations

Controller Driver (DCD & HCD)
-----------------------------

- Add new port support (WIP) for WCH CH32V307 USB Highspeed
- Add new port support (WIP) for PIC32MM/MX & PIC24

- [nRF]

  - Fix endpoint internal state when closed
  - Fix reception of large ISO packets

- [rp2040]

  - [dcd] Implement workaround for Errata 15. This enable SOF when bulk-in endpoint is in use and reduce its bandwidth to only 80%
  - [hcd] Fix shared irq slots filling up when hcd_init() is called multiple times
  - [hcd] Support host bulk endpoint using hw "interrupt" endpoint. Note speed limit is 64KB/s

- [samd][dcd] Add support for ISO endpoint
- [dwc2][dcd] Add support for stm32u5xx
- [esp32sx] Fix Isochronous transfers only transmitted on even frame
- [lpc_ip3511][dcd] Add isochronous support and fix endpoint accidental write
- [ft90x] Improve and enhance support for FT9xx MCU, tested with more examples

Device Stack
------------

- [Video]

  - Add support for MJPEG
  - Fix probe on macOS

- [MIDI]

  - Support port name strings
  - fix MS Header wTotalLength computation

- [HID]

  - Add FIDO descriptor template
  - change length in tud_hid_report_complete_cb() from uint8 to uint16

- [CDC]

  - Fix autoflush for FIFO < MPS
  - Fix tx fifo memory overflown when DTR is not set and tud_cdc_write() is called repeatedly with large enough data

- [USBTMC] Fix packet size with highspeed

Host Stack
----------

- Retry a few times with transfers in enumeration since device can be unstable when starting up
- [MSC] Rework host masstorage API. Add new **host/msc_file_explorer** example
- [CDC]

  - Add support for host cdc
  - Fix host cdc with device without IAD e.g Arduino Due

0.14.0
======

- Improve compiler support for CCRX and IAR
- Add timeout to osal_queue_receive()
- Add tud_task_ext(timeout, in_isr) as generic version of tud_task(). Same as tuh_task_ext(), tuh_task()
- Enable more warnings -Wnull-dereference -Wuninitialized -Wunused -Wredundant-decls -Wconversion
- Add new examples

  - host/bare_api to demonstrate generic (app-level) enumeration and endpoint transfer
  - dual/host_hid_to_device_cdc to run both device and host stack concurrently, get HID report from host and print out to device CDC. This example only work with multiple-controller MCUs and rp2040 with the help of pio-usb as added controller.

Controller Driver (DCD & HCD)
-----------------------------

- Enhance rhports management to better support dual roles

  - CFG_TUD_ENABLED/CFG_TUH_ENABLED, CFG_TUD_MAX_SPEED/CFG_TUH_MAX_SPEED can be used to replace CFG_TUSB_RHPORT0_MODE/CFG_TUSB_RHPORT1_MODE
  - tud_init(rphort), tuh_init(rhport) can be used to init stack on specified roothub port (controller) instead of tusb_init(void)
- Add dcd/hcd port specific defines `TUP_` (stand for tinyusb port-specific)
- [dwc2]

  - Update to support stm32 h72x, h73x with only 1 otg controller
  - Fix overwrite with grstctl when disable endpoint
- [EHCI] Fix an issue with EHCI driver
- [msp430] Fix for possible bug in msp430-elf-gcc 9.3.0
- [nrf5x] Fix DMA access race condition using atomic function
- [pic32] Fix PIC32 santiy
- [rp2040]

  - Add PICO-PIO-USB as controller (device/host) support for rp2040
  - Use shared IRQ handlers, so user can also hook the USB IRQ
  - Fix resumed signal not reported to device stack
- [stm32fsdev] Add support for stm32wb55

Device Stack
------------

- [Audio] Add support for feedback endpoint computation

  - New API tud_audio_feedback_params_cb(), tud_audio_feedback_interval_isr().
  - Supported computation method are: frequency with fixed/float or power of 2. Feedback with fifo count is not yet supported.
  - Fix nitfs (should be 3) in TUD_AUDIO_HEADSET_STEREO_DESCRIPTOR
  - Fix typo in audiod_rx_done_cb()

- [DFU] Fix coexistence with other interfaces BTH, RNDIS
- [MSC] Fix inquiry response additional length field
- [Venndor] Improve write performance

Host Stack
----------

- Add new API tuh_configure(rhport, cfg_id, cfg_param) for dynamnic port specific behavior configuration
- [HID] Open OUT endpoint if available
- [Hub] hub clear port and device interrupts
- [USBH] Major improvement

  - Rework usbh control transfer with complete callback. New API tuh_control_xfer() though still only carry 1 usbh (no queueing) at a time.
  - Add generic endpoint transfer with tuh_edpt_open(), tuh_edpt_xfer(). Require `CFG_TUH_API_EDPT_XFER=1`
  - Support app-level enumeration with new APIs

    - tuh_descriptor_get(), tuh_descriptor_get_device(), tuh_descriptor_get_configuration(), tuh_descriptor_get_hid_report()
    - tuh_descriptor_get_string(), tuh_descriptor_get_manufacturer_string(), tuh_descriptor_get_product_string(), tuh_descriptor_get_serial_string()
    - Also add _sync() as sync/blocking version for above APIs

0.13.0
======

- [tu_fifo] Fix locked mutex when full, and return type in peek_n()

Controller Driver (DCD & HCD)
-----------------------------

- [DWC2] Generalize synopsys dwc2 with synopsys/dwc2 which support both FS and HS phy (UTMI and ULPI) for various MCUs.
  - Broadcom 28/27xx on raspberrypi SBC
  - Silicon Labs EFM32
  - Espressif ESP32 Sx
  - GigaDevice GD32
  - ST STM32
  - Infineon XMC
- [KL25] Add new HCD for NXP KL25
- [MUSB] Add new DCD and HCD for Mentor musb with TI MSP432E4
- [F1C100s] Add new DCD for Allwinner F1C100s family
- [PIC32MZ] Add new DCD for PIC32MZ
- [nRF] Fix/Enhance various race condition with: EASY DMA, request HFXO, EPOUT
- [ChipIdea] rename Transdimension to more popular ChipIdea Highspeed,
- [RP2040] various update/fix for hcd/dcd
- [FT9XX] new DCD port for Bridgetek FT90x and FT93x devices
- [DA1469X] Fix resume
- [OHCI] Fix device array out of bound

Note: legacy drivers such as st/synopsys, nxp/transdimension are still present in this release but won't receive more update and could be removed in the future.

Device Stack
------------

- [Audio] Support disabling feedback format correction (16.16 <-> 10.14 format)
- [MSC] Add tud_msc_request_sense_cb() callback, change most default sense error to medium not present (0x02, 0x3A, 0x00)
- [Video] Fix video_capture example fails enumeration when 8FPS

Host Stack
----------

No notable changes

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
- Implement double buffer for both host and device (#891). However device EPOUT is still single buffered due to techinical issue with short packet

Device Stack
------------

USBD
^^^^

- Better support big endian mcu
- Add tuh_inited() and tud_inited(), will separate tusb_init/inited() to tud/tuh init/inited
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

- Implement double buffered to fix E4 errata and boost performance
- Lots of rp2040 update and enhancement

Host Stack
----------

- Major update and rework most of host stack, still needs more improvement
- Lots of improvement and update in parsing configuration and control
- Rework and major update to HID driver. Will default to enable boot interface if available
- Separate CFG_TUH_DEVICE_MAX and CFG_TUH_HUB for better management and reduce SRAM usage

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

- No noticeable changes

USB Host Driver (USBH)
----------------------

- No noticeable changes

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
- Fix typo in tud_midi_write24(), make example less ambiguous for cable and channel
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
- Improved usbd/usbd_control with proper handling of zero-length packet (ZLP)
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
