/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// Minimal configuration to build the NCM device driver for the receive-path
// fuzz harness. Only the NCM class is enabled; no root hub / DCD is needed
// because the harness drives recv_validate_datagram() directly.
#define CFG_TUSB_MCU            OPT_MCU_NONE
#define CFG_TUSB_OS             OPT_OS_NONE

#define CFG_TUD_ENABLED        1
#define CFG_TUD_ENDPOINT0_SIZE  64

// Network class has 2 drivers: ECM/RNDIS and NCM. Fuzz the NCM parser here.
#define CFG_TUD_ECM_RNDIS      0
#define CFG_TUD_NCM            1

#endif
