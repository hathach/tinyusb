/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_DCD_WCH_HS2_H_
#define TUSB_DCD_WCH_HS2_H_

#include "common/tusb_common.h"
#include "device/dcd.h"

// With TUP_USBIP_WCH_HS2_FALLBACK a USB3 dcd owns the dcd_* API and hands rhport 0 to HS2 when
// SuperSpeed does not train, so dcd_wch_hs2.c builds its dcd_* entry points under these names.
bool wch_hs2_init(uint8_t rhport, const tusb_rhport_init_t *rh_init);
bool wch_hs2_deinit(uint8_t rhport);
void wch_hs2_int_handler(uint8_t rhport);
void wch_hs2_int_enable(uint8_t rhport);
void wch_hs2_int_disable(uint8_t rhport);
void wch_hs2_set_address(uint8_t rhport, uint8_t dev_addr);
void wch_hs2_remote_wakeup(uint8_t rhport);
void wch_hs2_connect(uint8_t rhport);
void wch_hs2_disconnect(uint8_t rhport);
void wch_hs2_sof_enable(uint8_t rhport, bool en);
bool wch_hs2_edpt0_setup_begin(uint8_t rhport, uint8_t setup_tagid);
void wch_hs2_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request);
bool wch_hs2_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt);
void wch_hs2_edpt_close(uint8_t rhport, uint8_t ep_addr);
void wch_hs2_edpt_close_all(uint8_t rhport);
bool wch_hs2_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr);
void wch_hs2_edpt_stall(uint8_t rhport, uint8_t ep_addr);
void wch_hs2_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);

#endif // TUSB_DCD_WCH_HS2_H_
