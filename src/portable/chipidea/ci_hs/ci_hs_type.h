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

#ifndef CI_HS_TYPE_H_
#define CI_HS_TYPE_H_

#ifdef __cplusplus
 extern "C" {
#endif

// DCCPARAMS
enum {
  DCCPARAMS_DEN_MASK = 0x1Fu, ///< DEN bit 4:0
};

// USBCMD
enum {
  USBCMD_RUN_STOP         = TU_BIT(0),
  USBCMD_RESET            = TU_BIT(1),
  USBCMD_SETUP_TRIPWIRE   = TU_BIT(13),
  USBCMD_ADD_QTD_TRIPWIRE = TU_BIT(14), // This bit is used as a semaphore to ensure the to proper addition of a
                                        // new dTD to an active (primed) endpoint’s linked list. This bit is set and
                                        // cleared by software during the process of adding a new dTD

  USBCMD_INTR_THRESHOLD_MASK = 0x00FF0000u, // Interrupt Threshold bit 23:16
};

// PORTSC1
#define PORTSC1_PORT_SPEED_POS    26

enum {
  PORTSC1_CURRENT_CONNECT_STATUS = TU_BIT(0),
  PORTSC1_FORCE_PORT_RESUME      = TU_BIT(6),
  PORTSC1_SUSPEND                = TU_BIT(7),
  PORTSC1_FORCE_FULL_SPEED       = TU_BIT(24),
  PORTSC1_PORT_SPEED             = TU_BIT(26) | TU_BIT(27)
};

// OTGSC
enum {
  OTGSC_VBUS_DISCHARGE          = TU_BIT(0),
  OTGSC_VBUS_CHARGE             = TU_BIT(1),
//  OTGSC_HWASSIST_AUTORESET    = TU_BIT(2),
  OTGSC_OTG_TERMINATION         = TU_BIT(3), ///< Must set to 1 when OTG go to device mode
  OTGSC_DATA_PULSING            = TU_BIT(4),
  OTGSC_ID_PULLUP               = TU_BIT(5),
//  OTGSC_HWASSIT_DATA_PULSE    = TU_BIT(6),
//  OTGSC_HWASSIT_BDIS_ACONN    = TU_BIT(7),
  OTGSC_ID                      = TU_BIT(8), ///< 0 = A device, 1 = B Device
  OTGSC_A_VBUS_VALID            = TU_BIT(9),
  OTGSC_A_SESSION_VALID         = TU_BIT(10),
  OTGSC_B_SESSION_VALID         = TU_BIT(11),
  OTGSC_B_SESSION_END           = TU_BIT(12),
  OTGSC_1MS_TOGGLE              = TU_BIT(13),
  OTGSC_DATA_BUS_PULSING_STATUS = TU_BIT(14),
};

// USBMode
enum {
  USBMOD_CM_MASK    = TU_BIT(0) | TU_BIT(1),
  USBMODE_CM_DEVICE = 2,
  USBMODE_CM_HOST   = 3,

  USBMODE_SLOM = TU_BIT(3),
  USBMODE_SDIS = TU_BIT(4),

  USBMODE_VBUS_POWER_SELECT = TU_BIT(5), // Need to be enabled for LPC18XX/43XX in host mode
};

// Device Registers
typedef struct
{
  //------------- ID + HW Parameter Registers-------------//
  volatile uint32_t TU_RESERVED[64]; ///< For iMX RT10xx, but not used by LPC18XX/LPC43XX

  //------------- Capability Registers-------------//
  volatile uint8_t  CAPLENGTH;       ///< Capability Registers Length
  volatile uint8_t  TU_RESERVED[1];
  volatile uint16_t HCIVERSION;      ///< Host Controller Interface Version

  volatile uint32_t HCSPARAMS;       ///< Host Controller Structural Parameters
  volatile uint32_t HCCPARAMS;       ///< Host Controller Capability Parameters
  volatile uint32_t TU_RESERVED[5];

  volatile uint16_t DCIVERSION;      ///< Device Controller Interface Version
  volatile uint8_t  TU_RESERVED[2];

  volatile uint32_t DCCPARAMS;       ///< Device Controller Capability Parameters
  volatile uint32_t TU_RESERVED[6];

  //------------- Operational Registers -------------//
  volatile uint32_t USBCMD;          ///< USB Command Register
  volatile uint32_t USBSTS;          ///< USB Status Register
  volatile uint32_t USBINTR;         ///< Interrupt Enable Register
  volatile uint32_t FRINDEX;         ///< USB Frame Index
  volatile uint32_t TU_RESERVED;
  volatile uint32_t DEVICEADDR;      ///< Device Address
  volatile uint32_t ENDPTLISTADDR;   ///< Endpoint List Address
  volatile uint32_t TU_RESERVED;
  volatile uint32_t BURSTSIZE;       ///< Programmable Burst Size
  volatile uint32_t TXFILLTUNING;    ///< TX FIFO Fill Tuning
           uint32_t TU_RESERVED[4];
  volatile uint32_t ENDPTNAK;        ///< Endpoint NAK
  volatile uint32_t ENDPTNAKEN;      ///< Endpoint NAK Enable
  volatile uint32_t TU_RESERVED;
  volatile uint32_t PORTSC1;         ///< Port Status & Control
  volatile uint32_t TU_RESERVED[7];
  volatile uint32_t OTGSC;           ///< On-The-Go Status & control
  volatile uint32_t USBMODE;         ///< USB Device Mode
  volatile uint32_t ENDPTSETUPSTAT;  ///< Endpoint Setup Status
  volatile uint32_t ENDPTPRIME;      ///< Endpoint Prime
  volatile uint32_t ENDPTFLUSH;      ///< Endpoint Flush
  volatile uint32_t ENDPTSTAT;       ///< Endpoint Status
  volatile uint32_t ENDPTCOMPLETE;   ///< Endpoint Complete
  volatile uint32_t ENDPTCTRL[8];    ///< Endpoint Control 0 - 7
} ci_hs_regs_t;


typedef struct
{
  uint32_t reg_base;
  uint32_t irqnum;
}ci_hs_controller_t;

#ifdef __cplusplus
 }
#endif

#endif /* CI_HS_TYPE_H_ */
