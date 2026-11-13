/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// Host MSC driver alone for the msch_open() descriptor-walk harness; no HCD or usbh core is linked.
// CMake builds a second target with CFG_TUSB_VALIDATION_LEVEL=TUSB_VALIDATION_NONE.
#define CFG_TUSB_MCU          OPT_MCU_NONE
#define CFG_TUSB_OS           OPT_OS_NONE
#define TUP_DCD_ENDPOINT_MAX  8 // no controller; tusb_option.h's endpoint-limit check needs it

#define CFG_TUH_ENABLED       1
#define CFG_TUD_ENABLED       0
#define CFG_TUH_DEVICE_MAX    1
#define CFG_TUH_HUB           0
#define CFG_TUH_MSC           1

#endif
