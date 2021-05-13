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

#ifndef CFG_TUH_HID_REPORT_DESCRIPTOR_BUFSIZE
#define CFG_TUH_HID_REPORT_DESCRIPTOR_BUFSIZE 256
#endif

//--------------------------------------------------------------------+
// Application API (Multiple devices)
// Note:
//  - tud_n   : is multiple devices API
//  - class_n : is multiple instances API
//--------------------------------------------------------------------+

// Get the number of HID instances
uint8_t tuh_n_hid_instance_count(uint8_t daddr);

// Check if HID instance is mounted
bool tuh_n_hid_n_mounted(uint8_t daddr, uint8_t instance);

// Check if the interface is ready to use
bool tuh_n_hid_n_ready(uint8_t dev_addr, uint8_t instance);

bool tuh_n_hid_n_get_report(uint8_t daddr, uint8_t instance, void* report, uint16_t len);

// Check if HID instance with Keyboard is mounted
bool tuh_n_hid_n_keyboard_mounted(uint8_t daddr, uint8_t instance);

// Check if HID instance with Mouse is mounted
bool tuh_n_hid_n_mouse_mounted(uint8_t dev_addr, uint8_t instance);


//--------------------------------------------------------------------+
// Application API (Single device)
//--------------------------------------------------------------------+

// Get the number of HID instances
TU_ATTR_ALWAYS_INLINE static inline
uint8_t tuh_hid_instance_count(void)
{
  return tuh_n_hid_instance_count(1);
}

//bool tuh_hid_get_report(uint8_t dev_addr, uint8_t report_id, void * p_report, uint8_t len);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when report descriptor is received
// Note: enumeration is still not complete yet
TU_ATTR_WEAK void tuh_hid_descriptor_report_cb(uint8_t daddr, uint8_t instance, uint8_t const* report_desc, uint16_t desc_len);

TU_ATTR_WEAK void tuh_hid_mounted_cb  (uint8_t dev_addr, uint8_t instance);
TU_ATTR_WEAK void tuh_hid_unmounted_cb(uint8_t dev_addr, uint8_t instance);



//--------------------------------------------------------------------+
// KEYBOARD Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Keyboard Keyboard
 *  @{ */

/** \defgroup Keyboard_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

/** \brief      Check if device supports Keyboard interface or not
 * \param[in]   dev_addr    device address
 * \retval      true if device supports Keyboard interface
 * \retval      false if device does not support Keyboard interface or is not mounted
 */
bool tuh_hid_keyboard_mounted(uint8_t dev_addr);

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to device
 * \note        This function is primarily used for polling/waiting result after \ref tuh_hid_keyboard_get_report.
 *              Alternatively, asynchronous event API can be used
 */

//------------- Application Callback -------------//
/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref xfer_result_t
 * \note        event can be one of following
 *              - XFER_RESULT_SUCCESS : previously scheduled transfer completes successfully.
 *              - XFER_RESULT_FAILED   : previously scheduled transfer encountered a transaction error.
 *              - XFER_RESULT_STALLED : previously scheduled transfer is stalled by device.
 * \note        Application should schedule the next report by calling \ref tuh_hid_keyboard_get_report within this callback
 */
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event);

/** \brief 			Callback function that will be invoked when a device with Keyboard interface is mounted
 * \param[in] 	dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with Keyboard interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr);

/** @} */ // Keyboard_Host
/** @} */ // ClassDriver_HID_Keyboard

//--------------------------------------------------------------------+
// MOUSE Application API
//--------------------------------------------------------------------+
/** \addtogroup ClassDriver_HID_Mouse Mouse
 *  @{ */

/** \defgroup Mouse_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

//------------- Application Callback -------------//
/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref xfer_result_t
 * \note        event can be one of following
 *              - XFER_RESULT_SUCCESS : previously scheduled transfer completes successfully.
 *              - XFER_RESULT_FAILED   : previously scheduled transfer encountered a transaction error.
 *              - XFER_RESULT_STALLED : previously scheduled transfer is stalled by device.
 * \note        Application should schedule the next report by calling \ref tuh_hid_mouse_get_report within this callback
 */
void tuh_hid_mouse_isr(uint8_t dev_addr, xfer_result_t event);

/** \brief 			Callback function that will be invoked when a device with Mouse interface is mounted
 * \param[in]	  dev_addr Address of newly mounted device
 * \note        This callback should be used by Application to set-up interface-related data
 */
void tuh_hid_mouse_mounted_cb(uint8_t dev_addr);

/** \brief 			Callback function that will be invoked when a device with Mouse interface is unmounted
 * \param[in] 	dev_addr Address of newly unmounted device
 * \note        This callback should be used by Application to tear-down interface-related data
 */
void tuh_hid_mouse_unmounted_cb(uint8_t dev_addr);

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
