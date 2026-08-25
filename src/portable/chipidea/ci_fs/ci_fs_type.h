/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _CI_FS_TYPE_H
#define _CI_FS_TYPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Note: some MCUs can only access these registers in 8-bit mode
// align 4 is used to get rid of reserved fields
#define _va32     volatile TU_ATTR_ALIGNED(4)

//--------------------------------------------------------------------+
// Buffer Descriptor Table (BDT) - shared by the device (dcd) and host (hcd) drivers
// since both target the same ChipIdea-FS silicon. Keep the layout in one place so a
// fix cannot silently drift between the two drivers.
//--------------------------------------------------------------------+

// Token PID values reported in the BDT tok_pid field / written to the TOKEN register.
// The device driver only uses OUT/IN/SETUP; the rest are host-only.
enum {
  TOK_PID_OUT   = 0x1u,
  TOK_PID_IN    = 0x9u,
  TOK_PID_SETUP = 0xDu,
  TOK_PID_DATA0 = 0x3u,
  TOK_PID_DATA1 = 0xbu,
  TOK_PID_ACK   = 0x2u,
  TOK_PID_STALL = 0xeu,
  TOK_PID_NAK   = 0xau,
  TOK_PID_BUSTO = 0x0u,
  TOK_PID_ERR   = 0xfu,
};

// Note: this header is included before the CMSIS device header, so use plain `volatile`
// rather than CMSIS `__IO`.
typedef struct TU_ATTR_PACKED
{
  union {
    uint32_t head;
    struct {
      union {
        struct {
                   uint16_t           :  2;
          volatile uint16_t tok_pid   :  4;
                   uint16_t data      :  1;
          volatile uint16_t own       :  1;
                   uint16_t           :  8;
        };
        struct {
                   uint16_t           :  2;
                   uint16_t bdt_stall :  1;
                   uint16_t dts       :  1;
                   uint16_t ninc      :  1;
                   uint16_t keep      :  1;
                   uint16_t           : 10;
        };
      };
      volatile uint16_t bc : 10;
               uint16_t    :  6;
    };
  };
  uint8_t *addr;
}buffer_descriptor_t;

TU_VERIFY_STATIC( sizeof(buffer_descriptor_t) == 8, "size is not correct" );

typedef struct {
  _va32 uint8_t PER_ID;                 // [00] Peripheral ID register
  _va32 uint8_t ID_COMP;                // [04] Peripheral ID complement register
  _va32 uint8_t REV;                    // [08] Peripheral revision register
  _va32 uint8_t ADD_INFO;               // [0C] Peripheral additional info register
  _va32 uint8_t OTG_ISTAT;              // [10] OTG Interrupt Status Register
  _va32 uint8_t OTG_ICTRL;              // [14] OTG Interrupt Control Register
  _va32 uint8_t OTG_STAT;               // [18] OTG Status Register
  _va32 uint8_t OTG_CTRL;               // [1C] OTG Control register
  uint32_t reserved_20[24];             // [20]
  _va32 uint8_t INT_STAT;               // [80] Interrupt status register
  _va32 uint8_t INT_EN;                 // [84] Interrupt enable register
  _va32 uint8_t ERR_STAT;               // [88] Error interrupt status register
  _va32 uint8_t ERR_ENB;                // [8C] Error interrupt enable register
  _va32 uint8_t STAT;                   // [90] Status register
  _va32 uint8_t CTL;                    // [94] Control register
  _va32 uint8_t ADDR;                   // [98] Address register
  _va32 uint8_t BDT_PAGE1;              // [9C] BDT page register 1
  _va32 uint8_t FRM_NUML;               // [A0] Frame number register
  _va32 uint8_t FRM_NUMH;               // [A4] Frame number register
  _va32 uint8_t TOKEN;                  // [A8] Token register
  _va32 uint8_t SOF_THLD;               // [AC] SOF threshold register
  _va32 uint8_t BDT_PAGE2;              // [B0] BDT page register 2
  _va32 uint8_t BDT_PAGE3;              // [B4] BDT page register 3

  uint32_t reserved_b8;                 // [B8]
  uint32_t reserved_bc;                 // [BC]

  struct {
    _va32 uint8_t CTL;
  }EP[16];                              // [C0] Endpoint control register

  //----- Following is only found available in NXP Kinetis
  _va32 uint8_t USBCTRL;                // [100] USB Control register,
  _va32 uint8_t OBSERVE;                // [104] USB OTG Observe register,
  _va32 uint8_t CONTROL;                // [108] USB OTG Control register,
  _va32 uint8_t USBTRC0;                // [10C] USB Transceiver Control Register 0,
  uint32_t reserved_110;                // [110]
  _va32 uint8_t USBFRMADJUST;           // [114] Frame Adjust Register,

  //----- Following is only found available in NXP MCX
  uint32_t reserved_118[3];             // [118]
  _va32 uint8_t KEEP_ALIVE_CTRL;        // [124] Keep Alive Mode Control,
  _va32 uint8_t KEEP_ALIVE_WKCTRL;      // [128] Keep Alive Mode Wakeup Control,
  _va32 uint8_t MISCCTRL;               // [12C] Miscellaneous Control,
  _va32 uint8_t STALL_IL_DIS;           // [130] Peripheral Mode Stall Disable for Endpoints[ 7..0] IN
  _va32 uint8_t STALL_IH_DIS;           // [134] Peripheral Mode Stall Disable for Endpoints[15..8] IN
  _va32 uint8_t STALL_OL_DIS;           // [138] Peripheral Mode Stall Disable for Endpoints[ 7..0] OUT
  _va32 uint8_t STALL_OH_DIS;           // [13C] Peripheral Mode Stall Disable for Endpoints[15..8] OUT
  _va32 uint8_t CLK_RECOVER_CTRL;       // [140] USB Clock Recovery Control,
  _va32 uint8_t CLK_RECOVER_IRC_EN;     // [144] FIRC Oscillator Enable,
  uint32_t reserved_148[3];             // [148]
  _va32 uint8_t CLK_RECOVER_INT_EN;     // [154] Clock Recovery Combined Interrupt Enable,
  uint32_t reserved_158;                // [158]
  _va32 uint8_t CLK_RECOVER_INT_STATUS; // [15C] Clock Recovery Separated Interrupt Status,
} ci_fs_regs_t;

TU_VERIFY_STATIC(sizeof(ci_fs_regs_t) == 0x160, "Size is not correct");

typedef struct
{
  uint32_t reg_base;
  uint32_t irqnum;
} ci_fs_controller_t;

#ifdef __cplusplus
 }
#endif

#endif
