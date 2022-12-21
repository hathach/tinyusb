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

#ifndef _TUSB_CDC_HOST_H_
#define _TUSB_CDC_HOST_H_

#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// Set DTR ( bit 0), RTS (bit 1) on enumeration/mounted
#ifndef CFG_TUH_CDC_SET_DTRRTS_ON_ENUM
#define CFG_TUH_CDC_SET_DTRRTS_ON_ENUM    0
#endif

// RX FIFO size
#ifndef CFG_TUH_CDC_RX_BUFSIZE
#define CFG_TUH_CDC_RX_BUFSIZE USBH_EPSIZE_BULK_MAX
#endif

// RX Endpoint size
#ifndef CFG_TUH_CDC_RX_EPSIZE
#define CFG_TUH_CDC_RX_EPSIZE USBH_EPSIZE_BULK_MAX
#endif

// TX FIFO size
#ifndef CFG_TUH_CDC_TX_BUFSIZE
#define CFG_TUH_CDC_TX_BUFSIZE USBH_EPSIZE_BULK_MAX
#endif

// TX Endpoint size
#ifndef CFG_TUH_CDC_TX_EPSIZE
#define CFG_TUH_CDC_TX_EPSIZE USBH_EPSIZE_BULK_MAX
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

typedef struct
{
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
} tuh_cdc_itf_info_t;

// Get Interface index from device address + interface number
// return TUSB_INDEX_INVALID (0xFF) if not found
uint8_t tuh_cdc_itf_get_index(uint8_t daddr, uint8_t itf_num);

// Get Interface information
// return true if index is correct and interface is currently mounted
bool tuh_cdc_itf_get_info(uint8_t idx, tuh_cdc_itf_info_t* info);

// Check if a interface is mounted
bool tuh_cdc_mounted(uint8_t idx);

// Get current DTR status
bool tuh_cdc_get_dtr(uint8_t idx);

// Get current RTS status
bool tuh_cdc_get_rts(uint8_t idx);

// Check if interface is connected (DTR active)
TU_ATTR_ALWAYS_INLINE static inline bool tuh_cdc_connected(uint8_t idx)
{
  return tuh_cdc_get_dtr(idx);
}

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+

// Get the number of bytes available for writing
uint32_t tuh_cdc_write_available(uint8_t idx);

// Write to cdc interface
uint32_t tuh_cdc_write(uint8_t idx, void const* buffer, uint32_t bufsize);

// Force sending data if possible, return number of forced bytes
uint32_t tuh_cdc_write_flush(uint8_t idx);

// Clear the transmit FIFO
bool tuh_cdc_write_clear(uint8_t idx);

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+

// Get the number of bytes available for reading
uint32_t tuh_cdc_read_available(uint8_t idx);

// Read from cdc interface
uint32_t tuh_cdc_read (uint8_t idx, void* buffer, uint32_t bufsize);

// Clear the received FIFO
bool tuh_cdc_read_clear (uint8_t idx);

//--------------------------------------------------------------------+
// Control Endpoint (Request) API
//--------------------------------------------------------------------+

// Send control request to Set Control Line State: DTR (bit 0), RTS (bit 1)
bool tuh_cdc_set_control_line_state(uint8_t idx, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Connect by set both DTR, RTS
static inline bool tuh_cdc_connect(uint8_t idx, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  return tuh_cdc_set_control_line_state(idx, CDC_CONTROL_LINE_STATE_DTR | CDC_CONTROL_LINE_STATE_RTS, complete_cb, user_data);
}

// Disconnect by clear both DTR, RTS
static inline bool tuh_cdc_disconnect(uint8_t idx, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  return tuh_cdc_set_control_line_state(idx, 0x00, complete_cb, user_data);
}

//------------- Application Callback -------------//

// Invoked when a device with CDC interface is mounted
// idx is index of cdc interface in the internal pool.
TU_ATTR_WEAK extern void tuh_cdc_mount_cb(uint8_t idx);

// Invoked when a device with CDC interface is unmounted
TU_ATTR_WEAK extern void tuh_cdc_umount_cb(uint8_t idx);

// Invoked when received new data
TU_ATTR_WEAK extern void tuh_cdc_rx_cb(uint8_t idx);

// Invoked when a TX is complete and therefore space becomes available in TX buffer
TU_ATTR_WEAK extern void tuh_cdc_tx_complete_cb(uint8_t idx);

//--------------------------------------------------------------------+
// CDC APPLICATION CALLBACKS
//--------------------------------------------------------------------+

/** \brief      Callback function that is invoked when an transferring event occurred
 * \param[in]		dev_addr	Address of device
 * \param[in]   event an value from \ref xfer_result_t
 * \param[in]   pipe_id value from \ref cdc_pipeid_t indicate the pipe
 * \param[in]   xferred_bytes Number of bytes transferred via USB bus
 * \note        event can be one of following
 *              - XFER_RESULT_SUCCESS : previously scheduled transfer completes successfully.
 *              - XFER_RESULT_FAILED   : previously scheduled transfer encountered a transaction error.
 *              - XFER_RESULT_STALLED : previously scheduled transfer is stalled by device.
 * \note
 */
// void tuh_cdc_xfer_isr(uint8_t dev_addr, xfer_result_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes);

/// @} // group CDC_Serial_Host
/// @}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void cdch_init       (void);
bool cdch_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool cdch_set_config (uint8_t dev_addr, uint8_t itf_num);
bool cdch_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void cdch_close      (uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_HOST_H_ */
