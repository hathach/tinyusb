/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_PRINTER_H_
#define TUSB_PRINTER_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Printer Class Specific Control Request
typedef enum {
  TUSB_PRINTER_REQUEST_GET_DEVICE_ID   = 0x00, ///< Get device ID
  TUSB_PRINTER_REQUEST_GET_PORT_STATUS = 0x01, ///< Get port status
  TUSB_PRINTER_REQUEST_SOFT_RESET      = 0x02, ///< Soft reset
} tusb_printer_request_type_t;

/// Printer Port Status (returned by GET_PORT_STATUS request)
/// USB Printer Class spec 1.1, Section 4.2
typedef union TU_ATTR_PACKED {
  uint8_t status;
  struct TU_ATTR_PACKED {
    uint8_t reserved0   : 3; ///< Reserved (bits 0-2)
    uint8_t not_error   : 1; ///< 1 = no error, 0 = error
    uint8_t selected    : 1; ///< 1 = selected (online), 0 = not selected
    uint8_t paper_empty : 1; ///< 1 = paper empty, 0 = paper not empty
    uint8_t reserved6   : 2; ///< Reserved (bits 6-7)
  } status_bm;
} tusb_printer_port_status_t;

TU_VERIFY_STATIC(sizeof(tusb_printer_port_status_t) == 1, "size is not correct");

#ifdef __cplusplus
}
#endif

#endif
