/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_EHCI_API_H_
#define TUSB_EHCI_API_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// API Implemented by EHCI
//--------------------------------------------------------------------+

// Initialize EHCI driver
bool ehci_init(uint8_t rhport, uint32_t capability_reg, uint32_t operatial_reg);

// De-initialize EHCI driver
bool ehci_deinit(uint8_t rhport);

#ifdef __cplusplus
 }
#endif

#endif
