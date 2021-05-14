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

/** \addtogroup ClassDriver_HID
 *  @{ */

#ifndef _TUSB_HID_HOST_H_
#define _TUSB_HID_HOST_H_

#include "common/tusb_common.h"
#include "host/usbh.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUH_HID_REPORT_MAX
#define CFG_TUH_HID_REPORT_MAX 4
#endif

// TODO Highspeed interrupt can be up to 512 bytes
#ifndef CFG_TUH_HID_EP_BUFSIZE
#define CFG_TUH_HID_EP_BUFSIZE 64
#endif

typedef struct
{
  uint8_t count;        // number of info

  struct
  {
    uint8_t usage_page;
    uint8_t usage;

    // TODO still use the endpoint size for now
    uint8_t in_len;      // length of IN report
    uint8_t out_len;     // length of OUT report
  } info[CFG_TUH_HID_REPORT_MAX];
} tuh_hid_report_info_t;

//--------------------------------------------------------------------+
// Application API (Multiple devices)
// Note:
//  - tud_n   : is multiple devices API
//  - class_n : is multiple instances API
//--------------------------------------------------------------------+

// Get the number of HID instances
uint8_t tuh_n_hid_instance_count(uint8_t dev_addr);

// Check if HID instance is mounted
bool tuh_n_hid_n_mounted(uint8_t dev_addr, uint8_t instance);

// Get boot protocol check out hid_protocol_type_t for possible value
uint8_t tuh_n_hid_n_boot_protocol(uint8_t dev_addr, uint8_t instance);

// Check if current mode is Boot (true) or Report (false)
bool tuh_n_hid_n_boot_mode(uint8_t dev_addr, uint8_t instance);

// Get Report information parsed from report descriptor. Data must not be modified by application
// If report information does not match the actual device descriptor, that is because the built-in parser
// has its limit. Application could use tuh_hid_descriptor_report_cb() callback to parse descriptor by itself.
tuh_hid_report_info_t const* tuh_n_hid_n_get_report_info(uint8_t dev_addr, uint8_t instance);

// Check if the interface is ready to use
bool tuh_n_hid_n_ready(uint8_t dev_addr, uint8_t instance);

// Set Report using control endpoint
//bool tuh_n_hid_n_set_report_control(uint8_t dev_addr, uint8_t instance, void* report, uint16_t len);

//-------------  -------------//

// Check if HID instance with Keyboard is mounted
bool tuh_n_hid_n_keyboard_mounted(uint8_t dev_addr, uint8_t instance);

// Check if HID instance with Mouse is mounted
bool tuh_n_hid_n_mouse_mounted(uint8_t dev_addr, uint8_t instance);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when report descriptor is received
// Note: enumeration is still not complete yet at this time
TU_ATTR_WEAK void tuh_hid_descriptor_report_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report_desc, uint16_t desc_len);

// Invoked when device with hid interface is mounted
TU_ATTR_WEAK void tuh_hid_mounted_cb  (uint8_t dev_addr, uint8_t instance);

// Invoked when device with hid interface is un-mounted
TU_ATTR_WEAK void tuh_hid_unmounted_cb(uint8_t dev_addr, uint8_t instance);

// Invoked when received Report from device via either regular or control endpoint
TU_ATTR_WEAK void tuh_hid_get_report_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

// Invoked when Sent Report to device via either regular or control endpoint
TU_ATTR_WEAK void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t xferred_bytes);

//--------------------------------------------------------------------+
// Application API (Single device)
//--------------------------------------------------------------------+

// Get the number of HID instances
TU_ATTR_ALWAYS_INLINE static inline
uint8_t tuh_hid_instance_count(void)
{
  return tuh_n_hid_instance_count(1);
}


//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void hidh_init(void);
bool hidh_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t *p_length);
bool hidh_set_config(uint8_t dev_addr, uint8_t itf_num);
bool hidh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void hidh_close(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_HID_HOST_H_ */

/** @} */ // ClassDriver_HID
