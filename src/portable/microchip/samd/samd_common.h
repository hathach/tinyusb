/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_SAMD_COMMON_H_
#define TUSB_SAMD_COMMON_H_

#include "common/tusb_common.h"

#if TU_CHECK_MCU(OPT_MCU_SAMD11, OPT_MCU_SAMD21, OPT_MCU_SAML2X, OPT_MCU_SAMD51, OPT_MCU_SAME5X)

  #include "sam.h"

  // Device EPCFG.EPTYPE0/1 values
  #define USB_DEVICE_EPTYPE_DIS  0x0
  #define USB_DEVICE_EPTYPE_CTRL 0x1
  #define USB_DEVICE_EPTYPE_ISO  0x2
  #define USB_DEVICE_EPTYPE_BULK 0x3
  #define USB_DEVICE_EPTYPE_INT  0x4

  // Host PCFG.PTYPE values
  #define USB_HOST_PTYPE_DIS  0x0
  #define USB_HOST_PTYPE_CTRL 0x1
  #define USB_HOST_PTYPE_ISO  0x2
  #define USB_HOST_PTYPE_BULK 0x3
  #define USB_HOST_PTYPE_INT  0x4
  #define USB_HOST_PTYPE_EXT  0x5

// EPTYPE of a device endpoint from its USB transfer type
TU_VERIFY_STATIC(USB_DEVICE_EPTYPE_CTRL == TUSB_XFER_CONTROL + 1 &&
                   USB_DEVICE_EPTYPE_ISO == TUSB_XFER_ISOCHRONOUS + 1 && USB_DEVICE_EPTYPE_BULK == TUSB_XFER_BULK + 1 &&
                   USB_DEVICE_EPTYPE_INT == TUSB_XFER_INTERRUPT + 1,
                 "EPTYPE is the USB transfer type + 1");
TU_ATTR_ALWAYS_INLINE static inline uint32_t samd_eptype(uint32_t xfer_type) {
  return xfer_type + 1;
}

// PCKSIZE.SIZE code of the smallest bank that holds `size` bytes: 8 << code, except 7 = 1023 bytes.
// Callers reject sizes above 1023.
TU_ATTR_ALWAYS_INLINE static inline uint32_t samd_pcksize_code(uint32_t size) {
  uint32_t code = 0;
  while (code < 7 && (8u << code) < size) {
    code++;
  }
  return code;
}

  #if TU_CHECK_MCU(OPT_MCU_SAMD51, OPT_MCU_SAME5X)
TU_ATTR_ALWAYS_INLINE static inline void samd_usb_int_enable(void) {
  NVIC_EnableIRQ(USB_0_IRQn);
  NVIC_EnableIRQ(USB_1_IRQn);
  NVIC_EnableIRQ(USB_2_IRQn);
  NVIC_EnableIRQ(USB_3_IRQn);
}

TU_ATTR_ALWAYS_INLINE static inline void samd_usb_int_disable(void) {
  NVIC_DisableIRQ(USB_3_IRQn);
  NVIC_DisableIRQ(USB_2_IRQn);
  NVIC_DisableIRQ(USB_1_IRQn);
  NVIC_DisableIRQ(USB_0_IRQn);
}

  #else
TU_ATTR_ALWAYS_INLINE static inline void samd_usb_int_enable(void) {
  NVIC_EnableIRQ(USB_IRQn);
}

TU_ATTR_ALWAYS_INLINE static inline void samd_usb_int_disable(void) {
  NVIC_DisableIRQ(USB_IRQn);
}
  #endif

#endif

#endif
