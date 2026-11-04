/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// test_mtp_ss_device config (CFG_TUSB_CONFIG_FILE, project.yml): the shared one, with an MTP
// buffer of two SuperSpeed bulk packets, since 512 bytes is not a whole 1024-byte packet
#ifndef TUSB_CONFIG_MTP_SS_H_
#define TUSB_CONFIG_MTP_SS_H_

#include "tusb_config.h"

#undef CFG_TUD_MTP_EP_BUFSIZE
#define CFG_TUD_MTP_EP_BUFSIZE 2048

#endif
