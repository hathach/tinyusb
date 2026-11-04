/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_DCD_CH32H417_H_
#define TUSB_DCD_CH32H417_H_

#include "common/tusb_common.h"
#include "device/dcd.h"

// USB2.0 high-speed controller internals (dcd_ch32h417_usbhs.c). Exposed so the USB3.0 dcd
// (dcd_ch32h417_usb30.c) can drive the USB2 controller on the same rhport for runtime fallback
// when CFG_TUD_WCH_USB30_FALLBACK is enabled. The CH32H417's USB DMA reaches all of the shared
// SRAM (0x20100000), so no bounce buffers are needed - unlike the CH56x port.
bool ch32h417_usb2_init(uint8_t rhport);
void ch32h417_usb2_deinit(void);
void ch32h417_usb2_connect(void);
void ch32h417_usb2_disconnect(void);
void ch32h417_usb2_remote_wakeup(void);
void ch32h417_usb2_int_enable(void);
void ch32h417_usb2_int_disable(void);
void ch32h417_usb2_int_handler(uint8_t rhport);
void ch32h417_usb2_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request);
bool ch32h417_usb2_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt);
void ch32h417_usb2_edpt_close(uint8_t rhport, uint8_t ep_addr);
void ch32h417_usb2_edpt_close_all(uint8_t rhport);
bool ch32h417_usb2_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
void ch32h417_usb2_edpt_stall(uint8_t rhport, uint8_t ep_addr);
void ch32h417_usb2_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);
void ch32h417_usb2_sof_enable(uint8_t rhport, bool en);

#endif // TUSB_DCD_CH32H417_H_
