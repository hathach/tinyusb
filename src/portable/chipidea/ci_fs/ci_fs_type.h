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

#ifndef _CI_FS_TYPE_H_
#define _CI_FS_TYPE_H_

#ifdef __cplusplus
 extern "C" {
#endif

// reserved 3 bytes
#define _rsvd3   uint8_t TU_RESERVED[3]

// Note Kinetis requires byte write access to usb register
// declare it as uint32_t will cause data error
typedef struct {
  volatile uint8_t perid;        _rsvd3; // 00 Peripheral ID register
  volatile uint8_t idcomp;       _rsvd3; // 04 Peripheral ID complement register
  volatile uint8_t rev;          _rsvd3; // 08 Peripheral revision register
  volatile uint8_t add_info;     _rsvd3; // 0C Peripheral additional info register
  volatile uint8_t otg_int_stat; _rsvd3; // 10 OTG Interrupt Status Register
  volatile uint8_t otg_int_en;   _rsvd3; // 14 OTG Interrupt Control Register
  volatile uint8_t otg_stat;     _rsvd3; // 18 OTG Status Register
  volatile uint8_t OTG_CTRL;     _rsvd3; // 1C OTG Control register
  volatile uint8_t reserved_20[24*4];    // 20..7F Reserved
  volatile uint8_t int_stat;     _rsvd3; // 80 Interrupt status register
  volatile uint8_t int_en;       _rsvd3; // 84 Interrupt enable register
  volatile uint8_t err_stat;     _rsvd3; // 88 Error interrupt status register
  volatile uint8_t err_en;       _rsvd3; // 8C Error interrupt enable register
  volatile uint8_t stat;         _rsvd3; // 90 Status register
  volatile uint8_t ctl;          _rsvd3; // 94 Control register
  volatile uint8_t addr;         _rsvd3; // 98 Address register
  volatile uint8_t bdt_page1;    _rsvd3; // 9C BDT page register 1
  volatile uint8_t frm_numl;     _rsvd3; // A0 Frame number register
  volatile uint8_t frm_numh;     _rsvd3; // A4 Frame number register
  volatile uint8_t token;        _rsvd3; // A8 Token register
  volatile uint8_t sof_thld;     _rsvd3; // AC SOF threshold register
  volatile uint8_t bdt_page2;    _rsvd3; // B0 BDT page register 2
  volatile uint8_t bdt_page3;    _rsvd3; // B4 BDT page register 3
  volatile uint8_t reserved_b8;  _rsvd3; // B8 Reserved
  volatile uint8_t reserved_bc;  _rsvd3; // BC Reserved

  struct {
    volatile uint8_t endpt;      _rsvd3; // C0..FF Endpoint control register
  }ep[16];

  // Kinetis specific extension
  volatile uint8_t usbctrl;      _rsvd3; // 100
  volatile uint8_t observe;      _rsvd3; // 104
  volatile uint8_t control;      _rsvd3; // 108
  volatile uint8_t usbtrc0;      _rsvd3; // 10C
  volatile uint8_t reserved_110[4];      // 110
  volatile uint8_t usbfrmadjust; _rsvd3; // 114
} ci_fs_reg_t;


TU_VERIFY_STATIC(offsetof(ci_fs_reg_t, bdt_page1) == 0x9C, "incorrect size");
TU_VERIFY_STATIC(offsetof(ci_fs_reg_t, bdt_page2) == 0xB0, "incorrect size");
TU_VERIFY_STATIC(offsetof(ci_fs_reg_t, bdt_page3) == 0xB4, "incorrect size");
TU_VERIFY_STATIC(offsetof(ci_fs_reg_t, ep) == 0xC0, "incorrect size");

#ifdef __cplusplus
 }
#endif

#endif
