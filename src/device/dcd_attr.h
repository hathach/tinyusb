/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifndef TUSB_DCD_ATTR_H_
#define TUSB_DCD_ATTR_H_

#include "tusb_option.h"

// Attribute includes
// - ENDPOINT_MAX: max (logical) number of endpoint
// - ENDPOINT_EXCLUSIVE_NUMBER: endpoint number with different direction IN and OUT aren't allowed,
//                              e.g EP1 OUT & EP1 IN cannot exist together
// - PORT_HIGHSPEED: mask to indicate which port support highspeed mode, bit0 for port0 and so on.

//------------- NXP -------------//
#if   TU_CHECK_MCU(LPC11UXX, LPC13XX, LPC15XX)
  #define DCD_ATTR_ENDPOINT_MAX   5

#elif TU_CHECK_MCU(LPC175X_6X, LPC177X_8X, LPC40XX)
  #define DCD_ATTR_ENDPOINT_MAX   16

#elif TU_CHECK_MCU(LPC18XX, LPC43XX)
  // TODO USB0 has 6, USB1 has 4
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(LPC51UXX)
   #define DCD_ATTR_ENDPOINT_MAX   5

#elif TU_CHECK_MCU(LPC54XXX)
  // TODO USB0 has 5, USB1 has 6
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(LPC55XX)
  // TODO USB0 has 5, USB1 has 6
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(MIMXRT10XX)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(MKL25ZXX, K32L2BXX)
  #define DCD_ATTR_ENDPOINT_MAX   16

#elif TU_CHECK_MCU(MM32F327X)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Nordic -------------//
#elif TU_CHECK_MCU(NRF5X)
  // 8 CBI + 1 ISO
  #define DCD_ATTR_ENDPOINT_MAX   9

//------------- Microchip -------------//
#elif TU_CHECK_MCU(SAMD21, SAMD51, SAME5X) || \
      TU_CHECK_MCU(SAMD11, SAML21, SAML22)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(SAMG)
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(SAMX7X)
  #define DCD_ATTR_ENDPOINT_MAX   10
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

//------------- ST -------------//
#elif TU_CHECK_MCU(STM32F0)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(STM32F1)
  #if defined (STM32F105x8) || defined (STM32F105xB) || defined (STM32F105xC) || \
      defined (STM32F107xB) || defined (STM32F107xC)
    #define DCD_ATTR_ENDPOINT_MAX   4
    #define DCD_ATTR_DWC2_STM32
  #else
    #define DCD_ATTR_ENDPOINT_MAX   8
  #endif

#elif TU_CHECK_MCU(STM32F2)
  // FS has 4 ep, HS has 5 ep
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(STM32F3)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(STM32F4)
  // For most mcu, FS has 4, HS has 6. TODO 446/469/479 HS has 9
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(STM32F7)
  // FS has 6, HS has 9
  #define DCD_ATTR_ENDPOINT_MAX   9
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(STM32H7)
  #define DCD_ATTR_ENDPOINT_MAX   9
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(STM32L0, STM32L1)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(STM32L4)
  #if defined (STM32L475xx) || defined (STM32L476xx) ||                          \
      defined (STM32L485xx) || defined (STM32L486xx) || defined (STM32L496xx) || \
      defined (STM32L4R5xx) || defined (STM32L4R7xx) || defined (STM32L4R9xx) || \
      defined (STM32L4S5xx) || defined (STM32L4S7xx) || defined (STM32L4S9xx)
    #define DCD_ATTR_ENDPOINT_MAX   6
    #define DCD_ATTR_DWC2_STM32
  #else
    #define DCD_ATTR_ENDPOINT_MAX   8
  #endif

//------------- Sony -------------//
#elif TU_CHECK_MCU(CXD56)
  #define DCD_ATTR_ENDPOINT_MAX   7
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

//------------- TI -------------//
#elif TU_CHECK_MCU(MSP430x5xx)
  #define DCD_ATTR_ENDPOINT_MAX   8

//------------- ValentyUSB -------------//
#elif TU_CHECK_MCU(VALENTYUSB_EPTRI)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Nuvoton -------------//
#elif TU_CHECK_MCU(NUC121) || TU_CHECK_MCU(NUC126)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(NUC120)
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(NUC505)
  #define DCD_ATTR_ENDPOINT_MAX   12

//------------- Espressif -------------//
#elif TU_CHECK_MCU(ESP32S2, ESP32S3)
  #define DCD_ATTR_ENDPOINT_MAX   6

//------------- Dialog -------------//
#elif TU_CHECK_MCU(DA1469X)
  #define DCD_ATTR_ENDPOINT_MAX   4

//------------- Raspberry Pi -------------//
#elif TU_CHECK_MCU(RP2040)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Silabs -------------//
#elif TU_CHECK_MCU(EFM32GG) || TU_CHECK_MCU(EFM32GG11) || TU_CHECK_MCU(EFM32GG12)
  #define DCD_ATTR_ENDPOINT_MAX   7

//------------- Renesas -------------//
#elif TU_CHECK_MCU(RX63X) || TU_CHECK_MCU(RX65X) || TU_CHECK_MCU(RX72N)
  #define DCD_ATTR_ENDPOINT_MAX   10

//#elif TU_CHECK_MCU(MM32F327X)
//  #define DCD_ATTR_ENDPOINT_MAX not known yet

//------------- GigaDevice -------------//
#elif TU_CHECK_MCU(GD32VF103)
  #define DCD_ATTR_ENDPOINT_MAX   4

#else
  #warning "DCD_ATTR_ENDPOINT_MAX is not defined for this MCU, default to 8"
  #define DCD_ATTR_ENDPOINT_MAX   8
#endif

// Default to fullspeed if not defined
//#ifndef PORT_HIGHSPEED
//  #define DCD_ATTR_PORT_HIGHSPEED 0x00
//#endif

#endif
