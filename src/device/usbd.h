/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

/** \ingroup group_usbd
 *  @{ */

#ifndef _TUSB_USBD_H_
#define _TUSB_USBD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/tusb_common.h"
#include "dcd.h"

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Init device stack
bool tud_init (void);

// Task function should be called in main/rtos loop
void tud_task (void);

// Check if device is connected and configured
bool tud_mounted(void);

// Check if device is suspended
bool tud_suspended(void);

// Check if device is ready to transfer
static inline bool tud_ready(void)
{
  return tud_mounted() && !tud_suspended();
}

// Remote wake up host, only if suspended and enabled by host
bool tud_remote_wakeup(void);

// Carry out Data and Status stage of control transfer
// - If len = 0, it is equivalent to sending status only
// - If len > wLength : it will be truncated
bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len);

// Send STATUS (zero length) packet
bool tud_control_status(uint8_t rhport, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// Application Callbacks (WEAK is optional)
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR request
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void);

// Invoked when received GET BOS DESCRIPTOR request
// Application return pointer to descriptor
TU_ATTR_WEAK uint8_t const * tud_descriptor_bos_cb(void);

// Invoked when received GET CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index);

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index);

// Invoked when device is mounted (configured)
TU_ATTR_WEAK void tud_mount_cb(void);

// Invoked when device is unmounted
TU_ATTR_WEAK void tud_umount_cb(void);

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
TU_ATTR_WEAK void tud_suspend_cb(bool remote_wakeup_en);

// Invoked when usb bus is resumed
TU_ATTR_WEAK void tud_resume_cb(void);

// Invoked when received control request with VENDOR TYPE
TU_ATTR_WEAK bool tud_vendor_control_request_cb(uint8_t rhport, tusb_control_request_t const * request);
TU_ATTR_WEAK bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request);


//--------------------------------------------------------------------+
// Binary Device Object Store (BOS) Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_BOS_DESC_LEN      5

// total length, number of device caps
#define TUD_BOS_DESCRIPTOR(_total_len, _caps_num) \
  5, TUSB_DESC_BOS, U16_TO_U8S_LE(_total_len), _caps_num

// Device Capability Platform 128-bit UUID + Data
#define TUD_BOS_PLATFORM_DESCRIPTOR(...) \
  4+TU_ARGS_NUM(__VA_ARGS__), TUSB_DESC_DEVICE_CAPABILITY, DEVICE_CAPABILITY_PLATFORM, 0x00, __VA_ARGS__

//------------- WebUSB BOS Platform -------------//

// Descriptor Length
#define TUD_BOS_WEBUSB_DESC_LEN         24

// Vendor Code, iLandingPage
#define TUD_BOS_WEBUSB_DESCRIPTOR(_vendor_code, _ipage) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_WEBUSB_UUID, U16_TO_U8S_LE(0x0100), _vendor_code, _ipage)

#define TUD_BOS_WEBUSB_UUID   \
  0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, \
  0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65

//------------- Microsoft OS 2.0 Platform -------------//
#define TUD_BOS_MICROSOFT_OS_DESC_LEN   28

// Total Length of descriptor set, vendor code
#define TUD_BOS_MS_OS_20_DESCRIPTOR(_desc_set_len, _vendor_code) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_MS_OS_20_UUID, U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(_desc_set_len), _vendor_code, 0)

#define TUD_BOS_MS_OS_20_UUID \
  0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, \
  0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F

//--------------------------------------------------------------------+
// Configuration & Interface Descriptor Templates
//--------------------------------------------------------------------+

//------------- Configuration -------------//
#define TUD_CONFIG_DESC_LEN   (9)

// Interface count, string index, total length, attribute, power in mA
#define TUD_CONFIG_DESCRIPTOR(_itfcount, _stridx, _total_len, _attribute, _power_ma) \
  9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(_total_len), _itfcount, 1, _stridx, TU_BIT(7) | _attribute, (_power_ma)/2

//------------- CDC -------------//

// Length of template descriptor: 66 bytes
#define TUD_CDC_DESC_LEN  (8+9+5+5+4+5+7+9+7+7)

// CDC Descriptor Template
// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#define TUD_CDC_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize) \
  /* Interface Associate */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, _stridx,\
  /* CDC Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),\
  /* CDC Call */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1),\
  /* CDC ACM: support line request */\
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 2,\
  /* CDC Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 16,\
  /* CDC Data Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//------------- MSC -------------//

// Length of template descriptor: 23 bytes
#define TUD_MSC_DESC_LEN    (9 + 7 + 7)

// Interface number, string index, EP Out & EP In address, EP size
#define TUD_MSC_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, _stridx,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//------------- HID -------------//

// Length of template descriptor: 25 bytes
#define TUD_HID_DESC_LEN    (9 + 9 + 7)

// HID Input only descriptor
// Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
#define TUD_HID_DESCRIPTOR(_itfnum, _stridx, _boot_protocol, _report_desc_len, _epin, _epsize, _ep_interval) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_HID, (_boot_protocol) ? HID_SUBCLASS_BOOT : 0, _boot_protocol, _stridx,\
  /* HID descriptor */\
  9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(_report_desc_len),\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval

// Length of template descriptor: 32 bytes
#define TUD_HID_INOUT_DESC_LEN    (9 + 9 + 7 + 7)

// HID Input & Output descriptor
// Interface number, string index, protocol, report descriptor len, EP OUT & IN address, size & polling interval
#define TUD_HID_INOUT_DESCRIPTOR(_itfnum, _stridx, _boot_protocol, _report_desc_len, _epout, _epin, _epsize, _ep_interval) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_HID, (_boot_protocol) ? HID_SUBCLASS_BOOT : 0, _boot_protocol, _stridx,\
  /* HID descriptor */\
  9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(_report_desc_len),\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval, \
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval

//------------- MIDI -------------//

// Length of template descriptor (96 bytes)
#define TUD_MIDI_DESC_LEN (9 + 9 + 9 + 7 + 6 + 6 + 9 + 9 + 7 + 5 + 7 + 5)

// MIDI simple descriptor
// - 1 Embedded Jack In connected to 1 External Jack Out
// - 1 Embedded Jack out connected to 1 External Jack In
#define TUD_MIDI_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Audio Control (AC) Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_PROTOCOL_V1, _stridx,\
  /* AC Header */\
  9, TUSB_DESC_CS_INTERFACE, AUDIO_CS_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(0x0009), 1, (uint8_t)((_itfnum)+1),\
  /* MIDI Streaming (MS) Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_MIDI_STREAMING, AUDIO_PROTOCOL_V1, 0,\
  /* MS Header */\
  7, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(0x0025),\
  /* MS In Jack (Embedded) */\
  6, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_IN_JACK, MIDI_JACK_EMBEDDED, 1, 0,\
  /* MS In Jack (External) */\
  6, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_IN_JACK, MIDI_JACK_EXTERNAL, 2, 0,\
  /* MS Out Jack (Embedded), connected to In Jack External */\
  9, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_OUT_JACK, MIDI_JACK_EMBEDDED, 3, 1, 2, 1, 0,\
  /* MS Out Jack (External), connected to In Jack Embedded */\
  9, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_OUT_JACK, MIDI_JACK_EXTERNAL, 4, 1, 1, 1, 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* MS Endpoint (connected to embedded jack in) */\
  5, TUSB_DESC_CS_ENDPOINT, MIDI_CS_ENDPOINT_GENERAL, 1, 1,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* MS Endpoint (connected to embedded jack out) */\
  5, TUSB_DESC_CS_ENDPOINT, MIDI_CS_ENDPOINT_GENERAL, 1, 3

//------------- TUD_USBTMC/USB488 -------------//
#define TUD_USBTMC_APP_CLASS    (TUSB_CLASS_APPLICATION_SPECIFIC)
#define TUD_USBTMC_APP_SUBCLASS 0x03u

#define TUD_USBTMC_PROTOCOL_STD    0x00u
#define TUD_USBTMC_PROTOCOL_USB488 0x01u

//   Interface number, number of endpoints, EP string index, USB_TMC_PROTOCOL*, bulk-out endpoint ID,
//   bulk-in endpoint ID
#define TUD_USBTMC_IF_DESCRIPTOR(_itfnum, _bNumEndpoints, _stridx, _itfProtocol) \
/* Interface */ \
  0x09, TUSB_DESC_INTERFACE, _itfnum, 0x00, _bNumEndpoints, TUD_USBTMC_APP_CLASS, TUD_USBTMC_APP_SUBCLASS, _itfProtocol, _stridx

#define TUD_USBTMC_IF_DESCRIPTOR_LEN 9u

#define TUD_USBTMC_BULK_DESCRIPTORS(_epout, _epin, _bulk_epsize) \
/* Endpoint Out */ \
7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_epsize), 0u, \
/* Endpoint In */ \
7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_epsize), 0u

#define TUD_USBTMC_BULK_DESCRIPTORS_LEN (7u+7u)

/* optional interrupt endpoint */ \
// _int_pollingInterval : for LS/FS, expressed in frames (1ms each). 16 may be a good number?
#define TUD_USBTMC_INT_DESCRIPTOR(_ep_interrupt, _ep_interrupt_size, _int_pollingInterval ) \
7, TUSB_DESC_ENDPOINT, _ep_interrupt, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_interrupt_size), 0x16

#define TUD_USBTMC_INT_DESCRIPTOR_LEN (7u)


//------------- Vendor -------------//
#define TUD_VENDOR_DESC_LEN  (9+7+7)

// Interface number, string index, EP Out & IN address, EP size
#define TUD_VENDOR_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _stridx,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//------------- DFU Runtime -------------//
#define TUD_DFU_APP_CLASS    (TUSB_CLASS_APPLICATION_SPECIFIC)
#define TUD_DFU_APP_SUBCLASS 0x01u

// Length of template descriptr: 18 bytes
#define TUD_DFU_RT_DESC_LEN (9 + 9)

// DFU runtime descriptor
// Interface number, string index, attributes, detach timeout, transfer size
#define TUD_DFU_RT_DESCRIPTOR(_itfnum, _stridx, _attr, _timeout, _xfer_size) \
  /* Interface */ \
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUD_DFU_APP_CLASS, TUD_DFU_APP_SUBCLASS, DFU_PROTOCOL_RT, _stridx, \
  /* Function */ \
  9, DFU_DESC_FUNCTIONAL, _attr, U16_TO_U8S_LE(_timeout), U16_TO_U8S_LE(_xfer_size), U16_TO_U8S_LE(0x0101)


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_H_ */

/** @} */
