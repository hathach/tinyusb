/**
  * @author  MCD Application Team
  *          Ha Thach (tinyusb.org)
  *
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  */

#ifndef _TUSB_DWC2_TYPES_H_
#define _TUSB_DWC2_TYPES_H_

#include "stdint.h"

#ifdef __cplusplus
 extern "C" {
#endif

#if 0
// HS PHY
typedef struct
{
  volatile uint32_t HS_PHYC_PLL;         // This register is used to control the PLL of the HS PHY.                       000h */
  volatile uint32_t Reserved04;          // Reserved                                                                      004h */
  volatile uint32_t Reserved08;          // Reserved                                                                      008h */
  volatile uint32_t HS_PHYC_TUNE;        // This register is used to control the tuning interface of the High Speed PHY.  00Ch */
  volatile uint32_t Reserved10;          // Reserved                                                                      010h */
  volatile uint32_t Reserved14;          // Reserved                                                                      014h */
  volatile uint32_t HS_PHYC_LDO;         // This register is used to control the regulator (LDO).                         018h */
} HS_PHYC_GlobalTypeDef;
#endif

// Host Channel
typedef struct
{
  volatile uint32_t hcchar;           // 500 + 20n Host Channel Characteristics Register
  volatile uint32_t hcsplt;           // 504 + 20n Host Channel Split Control Register
  volatile uint32_t hcint;            // 508 + 20n Host Channel Interrupt Register
  volatile uint32_t hcintmsk;         // 50C + 20n Host Channel Interrupt Mask Register
  volatile uint32_t hctsiz;           // 510 + 20n Host Channel Transfer Size Register
  volatile uint32_t hcdma;            // 514 + 20n Host Channel DMA Address Register
           uint32_t reserved518;      // 518 + 20n
  volatile uint32_t hcdmab;           // 51C + 20n Host Channel DMA Address Register
} dwc2_channel_t;

typedef struct
{
  //------------- Core Global -------------//
  volatile uint32_t gotgctl;              // 000 OTG Control and Status Register
  volatile uint32_t gotgint;              // 004 OTG Interrupt Register
  volatile uint32_t gahbcfg;              // 008 AHB Configuration Register
  volatile uint32_t gusbcfg;              // 00c USB Configuration Register
  volatile uint32_t grstctl;              // 010 Reset Register
  volatile uint32_t gintsts;              // 014 Interrupt Register
  volatile uint32_t gintmsk;              // 018 Interrupt Mask Register
  volatile uint32_t grxstsr;              // 01c Receive Status Debug Read Register
  volatile uint32_t grxstsp;              // 020 Receive Status Read/Pop Register
  volatile uint32_t grxfsiz;              // 024 Receive FIFO Size Register
union {
  volatile uint32_t dieptxf0;             // 028 EP0 Tx FIFO Size Register
  volatile uint32_t gnptxfsiz;            // 028 Non-periodic Transmit FIFO Size Register
};
  volatile uint32_t gnptxsts;             // 02c Non-periodic Transmit FIFO/Queue Status Register
  volatile uint32_t gi2cctl;              // 030 I2C Address Register
  volatile uint32_t gpvndctl;             // 034 PHY Vendor Control Register
union {
  volatile uint32_t ggpio;                // 038 General Purpose IO Register
  volatile uint32_t gccfg;                // 038 STM32 General Core Configuration
};
  volatile uint32_t guid;                 // 03C User ID Register
  volatile uint32_t gsnpsid;              // 040 Synopsys ID Register
  volatile uint32_t ghwcfg1;              // 044 User Hardware Configuration 1 Register
  volatile uint32_t ghwcfg2;              // 048 User Hardware Configuration 2 Register
  volatile uint32_t ghwcfg3;              // 04C User Hardware Configuration 3 Register
  volatile uint32_t ghwcfg4;              // 050 User Hardware Configuration 4 Register
  volatile uint32_t glpmcfg;              // 054 Core LPM Configuration Register
  volatile uint32_t gpwrdn;               // 058 Power Down Register
  volatile uint32_t gdfifocfg;            // 05C DFIFO Software Configuration Register
  volatile uint32_t gadpctl;              // 060 ADP Timer, Control and Status Register

           uint32_t reserved64[39];       // 064..0FF
  volatile uint32_t hptxfsiz;             // 100 Host Periodic Tx FIFO Size Register
  volatile uint32_t dieptxf[15];          // 104..13C Device Periodic Transmit FIFO Size Register
           uint32_t reserved140[176];     // 140..3FC

  //------------- Host -------------//
  volatile uint32_t hcfg;             // 400 Host Configuration Register
  volatile uint32_t hfir;             // 404 Host Frame Interval Register
  volatile uint32_t hfnum;            // 408 Host Frame Number / Frame Remaining
           uint32_t reserved40c;      // 40C
  volatile uint32_t hptxsts;          // 410 Host Periodic TX FIFO / Queue Status
  volatile uint32_t haint;            // 414 Host All Channels Interrupt Register
  volatile uint32_t haintmsk;         // 418 Host All Channels Interrupt Mask
  volatile uint32_t hflbaddr;         // 41C Host Frame List Base Address Register
           uint32_t reserved420[8];   // 420..43C
  volatile uint32_t hprt;             // 440 Host Port Control and Status
           uint32_t reserved444[47];  // 444..4FC

  //------------- Host Channel -------------//
  dwc2_channel_t channel[16];         // 500..6FC Host Channels 0-15

           uint32_t reserved700[64];  // 700..7FC

  //------------- Device -------------//
  volatile uint32_t dcfg;            // 800 Device Configuration Register
  volatile uint32_t dctl;            // 804 Device Control Register
  volatile uint32_t dsts;            // 808 Device Status Register (RO)
           uint32_t reserved80c;     // 80C
  volatile uint32_t diepmsk;         // 810 Device IN Endpoint Interrupt Mask
  volatile uint32_t doepmsk;         // 814 Device OUT Endpoint Interrupt Mask
  volatile uint32_t daint;           // 818 Device All Endpoints Interrupt
  volatile uint32_t daintmsk;        // 81C Device All Endpoints Interrupt Mask
  volatile uint32_t dtknqr1;         // 820 Device IN token sequence learning queue read register1
  volatile uint32_t dtknqr2;         // 824 Device IN token sequence learning queue read register2
  volatile uint32_t dvbusdis;        // 828 Device VBUS Discharge Time Register
  volatile uint32_t dvbuspulse;      // 82C Device VBUS Pulsing Time Register
  volatile uint32_t dthrctl;         // 830 Device threshold Control
  volatile uint32_t diepempmsk;      // 834 Device IN Endpoint FIFO Empty Interrupt Mask

  volatile uint32_t deachint;        // 838 Device Each Endpoint Interrupt
  volatile uint32_t deachmsk;        // 83C Device Each Endpoint Interrupt msk
  volatile uint32_t diepeachmsk[16]; // 840..87C Device Each IN Endpoint mask
  volatile uint32_t doepeachmsk[16]; // 880..8BC Device Each OUT Endpoint mask
           uint32_t reserved8c0[16]; // 8C0..8FC

  //------------- Device IN Endpoint -------------//


  //------------- Device OUT Endpoint -------------//
} dwc2_regs_t;

TU_VERIFY_STATIC(offsetof(dwc2_regs_t, hcfg   ) == 0x0400, "incorrect size");
TU_VERIFY_STATIC(offsetof(dwc2_regs_t, channel) == 0x0500, "incorrect size");
TU_VERIFY_STATIC(offsetof(dwc2_regs_t, dcfg   ) == 0x0800, "incorrect size");

// Endpoint IN
typedef struct
{
  volatile uint32_t DIEPCTL;       // dev IN Endpoint Control Reg    900h + (ep_num * 20h) + 00h */
           uint32_t Reserved04;    // Reserved                       900h + (ep_num * 20h) + 04h */
  volatile uint32_t DIEPINT;       // dev IN Endpoint Itr Reg        900h + (ep_num * 20h) + 08h */
           uint32_t Reserved0C;    // Reserved                       900h + (ep_num * 20h) + 0Ch */
  volatile uint32_t DIEPTSIZ;      // IN Endpoint Txfer Size         900h + (ep_num * 20h) + 10h */
  volatile uint32_t DIEPDMA;       // IN Endpoint DMA Address Reg    900h + (ep_num * 20h) + 14h */
  volatile uint32_t DTXFSTS;       // IN Endpoint Tx FIFO Status Reg 900h + (ep_num * 20h) + 18h */
           uint32_t Reserved18;    // Reserved  900h+(ep_num*20h)+1Ch-900h+ (ep_num * 20h) + 1Ch */
} dwc2_epin_t;

// Endpoint OUT
typedef struct
{
  volatile uint32_t DOEPCTL;       // dev OUT Endpoint Control Reg           B00h + (ep_num * 20h) + 00h */
           uint32_t Reserved04;    // Reserved                               B00h + (ep_num * 20h) + 04h */
  volatile uint32_t DOEPINT;       // dev OUT Endpoint Itr Reg               B00h + (ep_num * 20h) + 08h */
           uint32_t Reserved0C;    // Reserved                               B00h + (ep_num * 20h) + 0Ch */
  volatile uint32_t DOEPTSIZ;      // dev OUT Endpoint Txfer Size            B00h + (ep_num * 20h) + 10h */
  volatile uint32_t DOEPDMA;       // dev OUT Endpoint DMA Address           B00h + (ep_num * 20h) + 14h */
           uint32_t Reserved18[2]; // Reserved B00h + (ep_num * 20h) + 18h - B00h + (ep_num * 20h) + 1Ch */
} dwc2_epout_t;


//--------------------------------------------------------------------+
// Register Base Address
//--------------------------------------------------------------------+

#define DWC2_GLOBAL_BASE                  0x00000000UL
#define DWC2_DEVICE_BASE                  0x00000800UL
#define DWC2_IN_ENDPOINT_BASE             0x00000900UL
#define DWC2_OUT_ENDPOINT_BASE            0x00000B00UL
#define DWC2_EP_REG_SIZE                  0x00000020UL
#define DWC2_HOST_BASE                    0x00000400UL
#define DWC2_HOST_PORT_BASE               0x00000440UL
#define DWC2_HOST_CHANNEL_BASE            0x00000500UL
#define DWC2_HOST_CHANNEL_SIZE            0x00000020UL
#define DWC2_PCGCCTL_BASE                 0x00000E00UL
#define DWC2_FIFO_BASE                    0x00001000UL
#define DWC2_FIFO_SIZE                    0x00001000UL

//--------------------------------------------------------------------+
// Register Bit Definitions
//--------------------------------------------------------------------+

/********************  Bit definition for GOTGCTL register  ********************/
#define GOTGCTL_SRQSCS_Pos               (0U)
#define GOTGCTL_SRQSCS_Msk               (0x1UL << GOTGCTL_SRQSCS_Pos)            // 0x00000001 */
#define GOTGCTL_SRQSCS                   GOTGCTL_SRQSCS_Msk                       // Session request success */
#define GOTGCTL_SRQ_Pos                  (1U)
#define GOTGCTL_SRQ_Msk                  (0x1UL << GOTGCTL_SRQ_Pos)               // 0x00000002 */
#define GOTGCTL_SRQ                      GOTGCTL_SRQ_Msk                          // Session request */
#define GOTGCTL_VBVALOEN_Pos             (2U)
#define GOTGCTL_VBVALOEN_Msk             (0x1UL << GOTGCTL_VBVALOEN_Pos)          // 0x00000004 */
#define GOTGCTL_VBVALOEN                 GOTGCTL_VBVALOEN_Msk                     // VBUS valid override enable */
#define GOTGCTL_VBVALOVAL_Pos            (3U)
#define GOTGCTL_VBVALOVAL_Msk            (0x1UL << GOTGCTL_VBVALOVAL_Pos)         // 0x00000008 */
#define GOTGCTL_VBVALOVAL                GOTGCTL_VBVALOVAL_Msk                    // VBUS valid override value */
#define GOTGCTL_AVALOEN_Pos              (4U)
#define GOTGCTL_AVALOEN_Msk              (0x1UL << GOTGCTL_AVALOEN_Pos)           // 0x00000010 */
#define GOTGCTL_AVALOEN                  GOTGCTL_AVALOEN_Msk                      // A-peripheral session valid override enable */
#define GOTGCTL_AVALOVAL_Pos             (5U)
#define GOTGCTL_AVALOVAL_Msk             (0x1UL << GOTGCTL_AVALOVAL_Pos)          // 0x00000020 */
#define GOTGCTL_AVALOVAL                 GOTGCTL_AVALOVAL_Msk                     // A-peripheral session valid override value */
#define GOTGCTL_BVALOEN_Pos              (6U)
#define GOTGCTL_BVALOEN_Msk              (0x1UL << GOTGCTL_BVALOEN_Pos)           // 0x00000040 */
#define GOTGCTL_BVALOEN                  GOTGCTL_BVALOEN_Msk                      // B-peripheral session valid override enable */
#define GOTGCTL_BVALOVAL_Pos             (7U)
#define GOTGCTL_BVALOVAL_Msk             (0x1UL << GOTGCTL_BVALOVAL_Pos)          // 0x00000080 */
#define GOTGCTL_BVALOVAL                 GOTGCTL_BVALOVAL_Msk                     // B-peripheral session valid override value  */
#define GOTGCTL_HNGSCS_Pos               (8U)
#define GOTGCTL_HNGSCS_Msk               (0x1UL << GOTGCTL_HNGSCS_Pos)            // 0x00000100 */
#define GOTGCTL_HNGSCS                   GOTGCTL_HNGSCS_Msk                       // Host set HNP enable */
#define GOTGCTL_HNPRQ_Pos                (9U)
#define GOTGCTL_HNPRQ_Msk                (0x1UL << GOTGCTL_HNPRQ_Pos)             // 0x00000200 */
#define GOTGCTL_HNPRQ                    GOTGCTL_HNPRQ_Msk                        // HNP request */
#define GOTGCTL_HSHNPEN_Pos              (10U)
#define GOTGCTL_HSHNPEN_Msk              (0x1UL << GOTGCTL_HSHNPEN_Pos)           // 0x00000400 */
#define GOTGCTL_HSHNPEN                  GOTGCTL_HSHNPEN_Msk                      // Host set HNP enable */
#define GOTGCTL_DHNPEN_Pos               (11U)
#define GOTGCTL_DHNPEN_Msk               (0x1UL << GOTGCTL_DHNPEN_Pos)            // 0x00000800 */
#define GOTGCTL_DHNPEN                   GOTGCTL_DHNPEN_Msk                       // Device HNP enabled */
#define GOTGCTL_EHEN_Pos                 (12U)
#define GOTGCTL_EHEN_Msk                 (0x1UL << GOTGCTL_EHEN_Pos)              // 0x00001000 */
#define GOTGCTL_EHEN                     GOTGCTL_EHEN_Msk                         // Embedded host enable */
#define GOTGCTL_CIDSTS_Pos               (16U)
#define GOTGCTL_CIDSTS_Msk               (0x1UL << GOTGCTL_CIDSTS_Pos)            // 0x00010000 */
#define GOTGCTL_CIDSTS                   GOTGCTL_CIDSTS_Msk                       // Connector ID status */
#define GOTGCTL_DBCT_Pos                 (17U)
#define GOTGCTL_DBCT_Msk                 (0x1UL << GOTGCTL_DBCT_Pos)              // 0x00020000 */
#define GOTGCTL_DBCT                     GOTGCTL_DBCT_Msk                         // Long/short debounce time */
#define GOTGCTL_ASVLD_Pos                (18U)
#define GOTGCTL_ASVLD_Msk                (0x1UL << GOTGCTL_ASVLD_Pos)             // 0x00040000 */
#define GOTGCTL_ASVLD                    GOTGCTL_ASVLD_Msk                        // A-session valid  */
#define GOTGCTL_BSESVLD_Pos              (19U)
#define GOTGCTL_BSESVLD_Msk              (0x1UL << GOTGCTL_BSESVLD_Pos)           // 0x00080000 */
#define GOTGCTL_BSESVLD                  GOTGCTL_BSESVLD_Msk                      // B-session valid */
#define GOTGCTL_OTGVER_Pos               (20U)
#define GOTGCTL_OTGVER_Msk               (0x1UL << GOTGCTL_OTGVER_Pos)            // 0x00100000 */
#define GOTGCTL_OTGVER                   GOTGCTL_OTGVER_Msk                       // OTG version  */

/********************  Bit definition for HCFG register  ********************/
#define HCFG_FSLSPCS_Pos                 (0U)
#define HCFG_FSLSPCS_Msk                 (0x3UL << HCFG_FSLSPCS_Pos)              // 0x00000003 */
#define HCFG_FSLSPCS                     HCFG_FSLSPCS_Msk                         // FS/LS PHY clock select  */
#define HCFG_FSLSPCS_0                   (0x1UL << HCFG_FSLSPCS_Pos)              // 0x00000001 */
#define HCFG_FSLSPCS_1                   (0x2UL << HCFG_FSLSPCS_Pos)              // 0x00000002 */
#define HCFG_FSLSS_Pos                   (2U)
#define HCFG_FSLSS_Msk                   (0x1UL << HCFG_FSLSS_Pos)                // 0x00000004 */
#define HCFG_FSLSS                       HCFG_FSLSS_Msk                           // FS- and LS-only support */

/********************  Bit definition for DCFG register  ********************/
#define DCFG_DSPD_Pos                    (0U)
#define DCFG_DSPD_Msk                    (0x3UL << DCFG_DSPD_Pos)                 // 0x00000003 */
#define DCFG_DSPD                        DCFG_DSPD_Msk                            // Device speed */
#define DCFG_DSPD_0                      (0x1UL << DCFG_DSPD_Pos)                 // 0x00000001 */
#define DCFG_DSPD_1                      (0x2UL << DCFG_DSPD_Pos)                 // 0x00000002 */
#define DCFG_NZLSOHSK_Pos                (2U)
#define DCFG_NZLSOHSK_Msk                (0x1UL << DCFG_NZLSOHSK_Pos)             // 0x00000004 */
#define DCFG_NZLSOHSK                    DCFG_NZLSOHSK_Msk                        // Nonzero-length status OUT handshake */

#define DCFG_DAD_Pos                     (4U)
#define DCFG_DAD_Msk                     (0x7FUL << DCFG_DAD_Pos)                 // 0x000007F0 */
#define DCFG_DAD                         DCFG_DAD_Msk                             // Device address */
#define DCFG_DAD_0                       (0x01UL << DCFG_DAD_Pos)                 // 0x00000010 */
#define DCFG_DAD_1                       (0x02UL << DCFG_DAD_Pos)                 // 0x00000020 */
#define DCFG_DAD_2                       (0x04UL << DCFG_DAD_Pos)                 // 0x00000040 */
#define DCFG_DAD_3                       (0x08UL << DCFG_DAD_Pos)                 // 0x00000080 */
#define DCFG_DAD_4                       (0x10UL << DCFG_DAD_Pos)                 // 0x00000100 */
#define DCFG_DAD_5                       (0x20UL << DCFG_DAD_Pos)                 // 0x00000200 */
#define DCFG_DAD_6                       (0x40UL << DCFG_DAD_Pos)                 // 0x00000400 */

#define DCFG_PFIVL_Pos                   (11U)
#define DCFG_PFIVL_Msk                   (0x3UL << DCFG_PFIVL_Pos)                // 0x00001800 */
#define DCFG_PFIVL                       DCFG_PFIVL_Msk                           // Periodic (micro)frame interval */
#define DCFG_PFIVL_0                     (0x1UL << DCFG_PFIVL_Pos)                // 0x00000800 */
#define DCFG_PFIVL_1                     (0x2UL << DCFG_PFIVL_Pos)                // 0x00001000 */

#define DCFG_PERSCHIVL_Pos               (24U)
#define DCFG_PERSCHIVL_Msk               (0x3UL << DCFG_PERSCHIVL_Pos)            // 0x03000000 */
#define DCFG_PERSCHIVL                   DCFG_PERSCHIVL_Msk                       // Periodic scheduling interval */
#define DCFG_PERSCHIVL_0                 (0x1UL << DCFG_PERSCHIVL_Pos)            // 0x01000000 */
#define DCFG_PERSCHIVL_1                 (0x2UL << DCFG_PERSCHIVL_Pos)            // 0x02000000 */

/********************  Bit definition for PCGCR register  ********************/
#define PCGCR_STPPCLK_Pos                (0U)
#define PCGCR_STPPCLK_Msk                (0x1UL << PCGCR_STPPCLK_Pos)             // 0x00000001 */
#define PCGCR_STPPCLK                    PCGCR_STPPCLK_Msk                        // Stop PHY clock */
#define PCGCR_GATEHCLK_Pos               (1U)
#define PCGCR_GATEHCLK_Msk               (0x1UL << PCGCR_GATEHCLK_Pos)            // 0x00000002 */
#define PCGCR_GATEHCLK                   PCGCR_GATEHCLK_Msk                       // Gate HCLK */
#define PCGCR_PHYSUSP_Pos                (4U)
#define PCGCR_PHYSUSP_Msk                (0x1UL << PCGCR_PHYSUSP_Pos)             // 0x00000010 */
#define PCGCR_PHYSUSP                    PCGCR_PHYSUSP_Msk                        // PHY suspended */

/********************  Bit definition for GOTGINT register  ********************/
#define GOTGINT_SEDET_Pos                (2U)
#define GOTGINT_SEDET_Msk                (0x1UL << GOTGINT_SEDET_Pos)             // 0x00000004 */
#define GOTGINT_SEDET                    GOTGINT_SEDET_Msk                        // Session end detected                   */
#define GOTGINT_SRSSCHG_Pos              (8U)
#define GOTGINT_SRSSCHG_Msk              (0x1UL << GOTGINT_SRSSCHG_Pos)           // 0x00000100 */
#define GOTGINT_SRSSCHG                  GOTGINT_SRSSCHG_Msk                      // Session request success status change  */
#define GOTGINT_HNSSCHG_Pos              (9U)
#define GOTGINT_HNSSCHG_Msk              (0x1UL << GOTGINT_HNSSCHG_Pos)           // 0x00000200 */
#define GOTGINT_HNSSCHG                  GOTGINT_HNSSCHG_Msk                      // Host negotiation success status change */
#define GOTGINT_HNGDET_Pos               (17U)
#define GOTGINT_HNGDET_Msk               (0x1UL << GOTGINT_HNGDET_Pos)            // 0x00020000 */
#define GOTGINT_HNGDET                   GOTGINT_HNGDET_Msk                       // Host negotiation detected              */
#define GOTGINT_ADTOCHG_Pos              (18U)
#define GOTGINT_ADTOCHG_Msk              (0x1UL << GOTGINT_ADTOCHG_Pos)           // 0x00040000 */
#define GOTGINT_ADTOCHG                  GOTGINT_ADTOCHG_Msk                      // A-device timeout change                */
#define GOTGINT_DBCDNE_Pos               (19U)
#define GOTGINT_DBCDNE_Msk               (0x1UL << GOTGINT_DBCDNE_Pos)            // 0x00080000 */
#define GOTGINT_DBCDNE                   GOTGINT_DBCDNE_Msk                       // Debounce done                          */
#define GOTGINT_IDCHNG_Pos               (20U)
#define GOTGINT_IDCHNG_Msk               (0x1UL << GOTGINT_IDCHNG_Pos)            // 0x00100000 */
#define GOTGINT_IDCHNG                   GOTGINT_IDCHNG_Msk                       // Change in ID pin input value           */

/********************  Bit definition for DCTL register  ********************/
#define DCTL_RWUSIG_Pos                  (0U)
#define DCTL_RWUSIG_Msk                  (0x1UL << DCTL_RWUSIG_Pos)               // 0x00000001 */
#define DCTL_RWUSIG                      DCTL_RWUSIG_Msk                          // Remote wakeup signaling */
#define DCTL_SDIS_Pos                    (1U)
#define DCTL_SDIS_Msk                    (0x1UL << DCTL_SDIS_Pos)                 // 0x00000002 */
#define DCTL_SDIS                        DCTL_SDIS_Msk                            // Soft disconnect         */
#define DCTL_GINSTS_Pos                  (2U)
#define DCTL_GINSTS_Msk                  (0x1UL << DCTL_GINSTS_Pos)               // 0x00000004 */
#define DCTL_GINSTS                      DCTL_GINSTS_Msk                          // Global IN NAK status    */
#define DCTL_GONSTS_Pos                  (3U)
#define DCTL_GONSTS_Msk                  (0x1UL << DCTL_GONSTS_Pos)               // 0x00000008 */
#define DCTL_GONSTS                      DCTL_GONSTS_Msk                          // Global OUT NAK status   */

#define DCTL_TCTL_Pos                    (4U)
#define DCTL_TCTL_Msk                    (0x7UL << DCTL_TCTL_Pos)                 // 0x00000070 */
#define DCTL_TCTL                        DCTL_TCTL_Msk                            // Test control */
#define DCTL_TCTL_0                      (0x1UL << DCTL_TCTL_Pos)                 // 0x00000010 */
#define DCTL_TCTL_1                      (0x2UL << DCTL_TCTL_Pos)                 // 0x00000020 */
#define DCTL_TCTL_2                      (0x4UL << DCTL_TCTL_Pos)                 // 0x00000040 */
#define DCTL_SGINAK_Pos                  (7U)
#define DCTL_SGINAK_Msk                  (0x1UL << DCTL_SGINAK_Pos)               // 0x00000080 */
#define DCTL_SGINAK                      DCTL_SGINAK_Msk                          // Set global IN NAK         */
#define DCTL_CGINAK_Pos                  (8U)
#define DCTL_CGINAK_Msk                  (0x1UL << DCTL_CGINAK_Pos)               // 0x00000100 */
#define DCTL_CGINAK                      DCTL_CGINAK_Msk                          // Clear global IN NAK       */
#define DCTL_SGONAK_Pos                  (9U)
#define DCTL_SGONAK_Msk                  (0x1UL << DCTL_SGONAK_Pos)               // 0x00000200 */
#define DCTL_SGONAK                      DCTL_SGONAK_Msk                          // Set global OUT NAK        */
#define DCTL_CGONAK_Pos                  (10U)
#define DCTL_CGONAK_Msk                  (0x1UL << DCTL_CGONAK_Pos)               // 0x00000400 */
#define DCTL_CGONAK                      DCTL_CGONAK_Msk                          // Clear global OUT NAK      */
#define DCTL_POPRGDNE_Pos                (11U)
#define DCTL_POPRGDNE_Msk                (0x1UL << DCTL_POPRGDNE_Pos)             // 0x00000800 */
#define DCTL_POPRGDNE                    DCTL_POPRGDNE_Msk                        // Power-on programming done */

/********************  Bit definition for HFIR register  ********************/
#define HFIR_FRIVL_Pos                   (0U)
#define HFIR_FRIVL_Msk                   (0xFFFFUL << HFIR_FRIVL_Pos)             // 0x0000FFFF */
#define HFIR_FRIVL                       HFIR_FRIVL_Msk                           // Frame interval */

/********************  Bit definition for HFNUM register  ********************/
#define HFNUM_FRNUM_Pos                  (0U)
#define HFNUM_FRNUM_Msk                  (0xFFFFUL << HFNUM_FRNUM_Pos)            // 0x0000FFFF */
#define HFNUM_FRNUM                      HFNUM_FRNUM_Msk                          // Frame number         */
#define HFNUM_FTREM_Pos                  (16U)
#define HFNUM_FTREM_Msk                  (0xFFFFUL << HFNUM_FTREM_Pos)            // 0xFFFF0000 */
#define HFNUM_FTREM                      HFNUM_FTREM_Msk                          // Frame time remaining */

/********************  Bit definition for DSTS register  ********************/
#define DSTS_SUSPSTS_Pos                 (0U)
#define DSTS_SUSPSTS_Msk                 (0x1UL << DSTS_SUSPSTS_Pos)              // 0x00000001 */
#define DSTS_SUSPSTS                     DSTS_SUSPSTS_Msk                         // Suspend status   */

#define DSTS_ENUMSPD_Pos                 (1U)
#define DSTS_ENUMSPD_Msk                 (0x3UL << DSTS_ENUMSPD_Pos)              // 0x00000006 */
#define DSTS_ENUMSPD                     DSTS_ENUMSPD_Msk                         // Enumerated speed */
#define DSTS_ENUMSPD_0                   (0x1UL << DSTS_ENUMSPD_Pos)              // 0x00000002 */
#define DSTS_ENUMSPD_1                   (0x2UL << DSTS_ENUMSPD_Pos)              // 0x00000004 */
#define DSTS_EERR_Pos                    (3U)
#define DSTS_EERR_Msk                    (0x1UL << DSTS_EERR_Pos)                 // 0x00000008 */
#define DSTS_EERR                        DSTS_EERR_Msk                            // Erratic error     */
#define DSTS_FNSOF_Pos                   (8U)
#define DSTS_FNSOF_Msk                   (0x3FFFUL << DSTS_FNSOF_Pos)             // 0x003FFF00 */
#define DSTS_FNSOF                       DSTS_FNSOF_Msk                           // Frame number of the received SOF */

/********************  Bit definition for GAHBCFG register  ********************/
#define GAHBCFG_GINT_Pos                 (0U)
#define GAHBCFG_GINT_Msk                 (0x1UL << GAHBCFG_GINT_Pos)              // 0x00000001 */
#define GAHBCFG_GINT                     GAHBCFG_GINT_Msk                         // Global interrupt mask */
#define GAHBCFG_HBSTLEN_Pos              (1U)
#define GAHBCFG_HBSTLEN_Msk              (0xFUL << GAHBCFG_HBSTLEN_Pos)           // 0x0000001E */
#define GAHBCFG_HBSTLEN                  GAHBCFG_HBSTLEN_Msk                      // Burst length/type */
#define GAHBCFG_HBSTLEN_0                (0x0UL << GAHBCFG_HBSTLEN_Pos)           // Single */
#define GAHBCFG_HBSTLEN_1                (0x1UL << GAHBCFG_HBSTLEN_Pos)           // INCR */
#define GAHBCFG_HBSTLEN_2                (0x3UL << GAHBCFG_HBSTLEN_Pos)           // INCR4 */
#define GAHBCFG_HBSTLEN_3                (0x5UL << GAHBCFG_HBSTLEN_Pos)           // INCR8 */
#define GAHBCFG_HBSTLEN_4                (0x7UL << GAHBCFG_HBSTLEN_Pos)           // INCR16 */
#define GAHBCFG_DMAEN_Pos                (5U)
#define GAHBCFG_DMAEN_Msk                (0x1UL << GAHBCFG_DMAEN_Pos)             // 0x00000020 */
#define GAHBCFG_DMAEN                    GAHBCFG_DMAEN_Msk                        // DMA enable */
#define GAHBCFG_TXFELVL_Pos              (7U)
#define GAHBCFG_TXFELVL_Msk              (0x1UL << GAHBCFG_TXFELVL_Pos)           // 0x00000080 */
#define GAHBCFG_TXFELVL                  GAHBCFG_TXFELVL_Msk                      // TxFIFO empty level */
#define GAHBCFG_PTXFELVL_Pos             (8U)
#define GAHBCFG_PTXFELVL_Msk             (0x1UL << GAHBCFG_PTXFELVL_Pos)          // 0x00000100 */
#define GAHBCFG_PTXFELVL                 GAHBCFG_PTXFELVL_Msk                     // Periodic TxFIFO empty level */

/********************  Bit definition for GUSBCFG register  ********************/
#define GUSBCFG_TOCAL_Pos                (0U)
#define GUSBCFG_TOCAL_Msk                (0x7UL << GUSBCFG_TOCAL_Pos)             // 0x00000007 */
#define GUSBCFG_TOCAL                    GUSBCFG_TOCAL_Msk                        // FS timeout calibration */
#define GUSBCFG_TOCAL_0                  (0x1UL << GUSBCFG_TOCAL_Pos)             // 0x00000001 */
#define GUSBCFG_TOCAL_1                  (0x2UL << GUSBCFG_TOCAL_Pos)             // 0x00000002 */
#define GUSBCFG_TOCAL_2                  (0x4UL << GUSBCFG_TOCAL_Pos)             // 0x00000004 */
#define GUSBCFG_PHYIF_Pos                (3U)
#define GUSBCFG_PHYIF_Msk                (0x1UL << GUSBCFG_PHYIF_Pos)             // 0x00000008 */
#define GUSBCFG_PHYIF                    GUSBCFG_PHYIF_Msk                        // PHY Interface (PHYIf) */
#define GUSBCFG_ULPI_UTMI_SEL_Pos        (4U)
#define GUSBCFG_ULPI_UTMI_SEL_Msk        (0x1UL << GUSBCFG_ULPI_UTMI_SEL_Pos)     // 0x00000010 */
#define GUSBCFG_ULPI_UTMI_SEL            GUSBCFG_ULPI_UTMI_SEL_Msk                // ULPI or UTMI+ Select (ULPI_UTMI_Sel) */
#define GUSBCFG_PHYSEL_Pos               (6U)
#define GUSBCFG_PHYSEL_Msk               (0x1UL << GUSBCFG_PHYSEL_Pos)            // 0x00000040 */
#define GUSBCFG_PHYSEL                   GUSBCFG_PHYSEL_Msk                       // USB 2.0 high-speed ULPI PHY or USB 1.1 full-speed serial transceiver select */
#define GUSBCFG_SRPCAP_Pos               (8U)
#define GUSBCFG_SRPCAP_Msk               (0x1UL << GUSBCFG_SRPCAP_Pos)            // 0x00000100 */
#define GUSBCFG_SRPCAP                   GUSBCFG_SRPCAP_Msk                       // SRP-capable */
#define GUSBCFG_HNPCAP_Pos               (9U)
#define GUSBCFG_HNPCAP_Msk               (0x1UL << GUSBCFG_HNPCAP_Pos)            // 0x00000200 */
#define GUSBCFG_HNPCAP                   GUSBCFG_HNPCAP_Msk                       // HNP-capable */
#define GUSBCFG_TRDT_Pos                 (10U)
#define GUSBCFG_TRDT_Msk                 (0xFUL << GUSBCFG_TRDT_Pos)              // 0x00003C00 */
#define GUSBCFG_TRDT                     GUSBCFG_TRDT_Msk                         // USB turnaround time */
#define GUSBCFG_TRDT_0                   (0x1UL << GUSBCFG_TRDT_Pos)              // 0x00000400 */
#define GUSBCFG_TRDT_1                   (0x2UL << GUSBCFG_TRDT_Pos)              // 0x00000800 */
#define GUSBCFG_TRDT_2                   (0x4UL << GUSBCFG_TRDT_Pos)              // 0x00001000 */
#define GUSBCFG_TRDT_3                   (0x8UL << GUSBCFG_TRDT_Pos)              // 0x00002000 */
#define GUSBCFG_PHYLPCS_Pos              (15U)
#define GUSBCFG_PHYLPCS_Msk              (0x1UL << GUSBCFG_PHYLPCS_Pos)           // 0x00008000 */
#define GUSBCFG_PHYLPCS                  GUSBCFG_PHYLPCS_Msk                      // PHY Low-power clock select */
#define GUSBCFG_ULPIFSLS_Pos             (17U)
#define GUSBCFG_ULPIFSLS_Msk             (0x1UL << GUSBCFG_ULPIFSLS_Pos)          // 0x00020000 */
#define GUSBCFG_ULPIFSLS                 GUSBCFG_ULPIFSLS_Msk                     // ULPI FS/LS select               */
#define GUSBCFG_ULPIAR_Pos               (18U)
#define GUSBCFG_ULPIAR_Msk               (0x1UL << GUSBCFG_ULPIAR_Pos)            // 0x00040000 */
#define GUSBCFG_ULPIAR                   GUSBCFG_ULPIAR_Msk                       // ULPI Auto-resume                */
#define GUSBCFG_ULPICSM_Pos              (19U)
#define GUSBCFG_ULPICSM_Msk              (0x1UL << GUSBCFG_ULPICSM_Pos)           // 0x00080000 */
#define GUSBCFG_ULPICSM                  GUSBCFG_ULPICSM_Msk                      // ULPI Clock SuspendM             */
#define GUSBCFG_ULPIEVBUSD_Pos           (20U)
#define GUSBCFG_ULPIEVBUSD_Msk           (0x1UL << GUSBCFG_ULPIEVBUSD_Pos)        // 0x00100000 */
#define GUSBCFG_ULPIEVBUSD               GUSBCFG_ULPIEVBUSD_Msk                   // ULPI External VBUS Drive        */
#define GUSBCFG_ULPIEVBUSI_Pos           (21U)
#define GUSBCFG_ULPIEVBUSI_Msk           (0x1UL << GUSBCFG_ULPIEVBUSI_Pos)        // 0x00200000 */
#define GUSBCFG_ULPIEVBUSI               GUSBCFG_ULPIEVBUSI_Msk                   // ULPI external VBUS indicator    */
#define GUSBCFG_TSDPS_Pos                (22U)
#define GUSBCFG_TSDPS_Msk                (0x1UL << GUSBCFG_TSDPS_Pos)             // 0x00400000 */
#define GUSBCFG_TSDPS                    GUSBCFG_TSDPS_Msk                        // TermSel DLine pulsing selection */
#define GUSBCFG_PCCI_Pos                 (23U)
#define GUSBCFG_PCCI_Msk                 (0x1UL << GUSBCFG_PCCI_Pos)              // 0x00800000 */
#define GUSBCFG_PCCI                     GUSBCFG_PCCI_Msk                         // Indicator complement            */
#define GUSBCFG_PTCI_Pos                 (24U)
#define GUSBCFG_PTCI_Msk                 (0x1UL << GUSBCFG_PTCI_Pos)              // 0x01000000 */
#define GUSBCFG_PTCI                     GUSBCFG_PTCI_Msk                         // Indicator pass through          */
#define GUSBCFG_ULPIIPD_Pos              (25U)
#define GUSBCFG_ULPIIPD_Msk              (0x1UL << GUSBCFG_ULPIIPD_Pos)           // 0x02000000 */
#define GUSBCFG_ULPIIPD                  GUSBCFG_ULPIIPD_Msk                      // ULPI interface protect disable  */
#define GUSBCFG_FHMOD_Pos                (29U)
#define GUSBCFG_FHMOD_Msk                (0x1UL << GUSBCFG_FHMOD_Pos)             // 0x20000000 */
#define GUSBCFG_FHMOD                    GUSBCFG_FHMOD_Msk                        // Forced host mode                */
#define GUSBCFG_FDMOD_Pos                (30U)
#define GUSBCFG_FDMOD_Msk                (0x1UL << GUSBCFG_FDMOD_Pos)             // 0x40000000 */
#define GUSBCFG_FDMOD                    GUSBCFG_FDMOD_Msk                        // Forced peripheral mode          */
#define GUSBCFG_CTXPKT_Pos               (31U)
#define GUSBCFG_CTXPKT_Msk               (0x1UL << GUSBCFG_CTXPKT_Pos)            // 0x80000000 */
#define GUSBCFG_CTXPKT                   GUSBCFG_CTXPKT_Msk                       // Corrupt Tx packet               */

/********************  Bit definition for GRSTCTL register  ********************/
#define GRSTCTL_CSRST_Pos                (0U)
#define GRSTCTL_CSRST_Msk                (0x1UL << GRSTCTL_CSRST_Pos)             // 0x00000001 */
#define GRSTCTL_CSRST                    GRSTCTL_CSRST_Msk                        // Core soft reset          */
#define GRSTCTL_HSRST_Pos                (1U)
#define GRSTCTL_HSRST_Msk                (0x1UL << GRSTCTL_HSRST_Pos)             // 0x00000002 */
#define GRSTCTL_HSRST                    GRSTCTL_HSRST_Msk                        // HCLK soft reset          */
#define GRSTCTL_FCRST_Pos                (2U)
#define GRSTCTL_FCRST_Msk                (0x1UL << GRSTCTL_FCRST_Pos)             // 0x00000004 */
#define GRSTCTL_FCRST                    GRSTCTL_FCRST_Msk                        // Host frame counter reset */
#define GRSTCTL_RXFFLSH_Pos              (4U)
#define GRSTCTL_RXFFLSH_Msk              (0x1UL << GRSTCTL_RXFFLSH_Pos)           // 0x00000010 */
#define GRSTCTL_RXFFLSH                  GRSTCTL_RXFFLSH_Msk                      // RxFIFO flush             */
#define GRSTCTL_TXFFLSH_Pos              (5U)
#define GRSTCTL_TXFFLSH_Msk              (0x1UL << GRSTCTL_TXFFLSH_Pos)           // 0x00000020 */
#define GRSTCTL_TXFFLSH                  GRSTCTL_TXFFLSH_Msk                      // TxFIFO flush             */
#define GRSTCTL_TXFNUM_Pos               (6U)
#define GRSTCTL_TXFNUM_Msk               (0x1FUL << GRSTCTL_TXFNUM_Pos)           // 0x000007C0 */
#define GRSTCTL_TXFNUM                   GRSTCTL_TXFNUM_Msk                       // TxFIFO number */
#define GRSTCTL_TXFNUM_0                 (0x01UL << GRSTCTL_TXFNUM_Pos)           // 0x00000040 */
#define GRSTCTL_TXFNUM_1                 (0x02UL << GRSTCTL_TXFNUM_Pos)           // 0x00000080 */
#define GRSTCTL_TXFNUM_2                 (0x04UL << GRSTCTL_TXFNUM_Pos)           // 0x00000100 */
#define GRSTCTL_TXFNUM_3                 (0x08UL << GRSTCTL_TXFNUM_Pos)           // 0x00000200 */
#define GRSTCTL_TXFNUM_4                 (0x10UL << GRSTCTL_TXFNUM_Pos)           // 0x00000400 */
#define GRSTCTL_DMAREQ_Pos               (30U)
#define GRSTCTL_DMAREQ_Msk               (0x1UL << GRSTCTL_DMAREQ_Pos)            // 0x40000000 */
#define GRSTCTL_DMAREQ                   GRSTCTL_DMAREQ_Msk                       // DMA request signal */
#define GRSTCTL_AHBIDL_Pos               (31U)
#define GRSTCTL_AHBIDL_Msk               (0x1UL << GRSTCTL_AHBIDL_Pos)            // 0x80000000 */
#define GRSTCTL_AHBIDL                   GRSTCTL_AHBIDL_Msk                       // AHB master idle */

/********************  Bit definition for DIEPMSK register  ********************/
#define DIEPMSK_XFRCM_Pos                (0U)
#define DIEPMSK_XFRCM_Msk                (0x1UL << DIEPMSK_XFRCM_Pos)             // 0x00000001 */
#define DIEPMSK_XFRCM                    DIEPMSK_XFRCM_Msk                        // Transfer completed interrupt mask                 */
#define DIEPMSK_EPDM_Pos                 (1U)
#define DIEPMSK_EPDM_Msk                 (0x1UL << DIEPMSK_EPDM_Pos)              // 0x00000002 */
#define DIEPMSK_EPDM                     DIEPMSK_EPDM_Msk                         // Endpoint disabled interrupt mask                  */
#define DIEPMSK_TOM_Pos                  (3U)
#define DIEPMSK_TOM_Msk                  (0x1UL << DIEPMSK_TOM_Pos)               // 0x00000008 */
#define DIEPMSK_TOM                      DIEPMSK_TOM_Msk                          // Timeout condition mask (nonisochronous endpoints) */
#define DIEPMSK_ITTXFEMSK_Pos            (4U)
#define DIEPMSK_ITTXFEMSK_Msk            (0x1UL << DIEPMSK_ITTXFEMSK_Pos)         // 0x00000010 */
#define DIEPMSK_ITTXFEMSK                DIEPMSK_ITTXFEMSK_Msk                    // IN token received when TxFIFO empty mask          */
#define DIEPMSK_INEPNMM_Pos              (5U)
#define DIEPMSK_INEPNMM_Msk              (0x1UL << DIEPMSK_INEPNMM_Pos)           // 0x00000020 */
#define DIEPMSK_INEPNMM                  DIEPMSK_INEPNMM_Msk                      // IN token received with EP mismatch mask           */
#define DIEPMSK_INEPNEM_Pos              (6U)
#define DIEPMSK_INEPNEM_Msk              (0x1UL << DIEPMSK_INEPNEM_Pos)           // 0x00000040 */
#define DIEPMSK_INEPNEM                  DIEPMSK_INEPNEM_Msk                      // IN endpoint NAK effective mask                    */
#define DIEPMSK_TXFURM_Pos               (8U)
#define DIEPMSK_TXFURM_Msk               (0x1UL << DIEPMSK_TXFURM_Pos)            // 0x00000100 */
#define DIEPMSK_TXFURM                   DIEPMSK_TXFURM_Msk                       // FIFO underrun mask                                */
#define DIEPMSK_BIM_Pos                  (9U)
#define DIEPMSK_BIM_Msk                  (0x1UL << DIEPMSK_BIM_Pos)               // 0x00000200 */
#define DIEPMSK_BIM                      DIEPMSK_BIM_Msk                          // BNA interrupt mask                                */

/********************  Bit definition for HPTXSTS register  ********************/
#define HPTXSTS_PTXFSAVL_Pos             (0U)
#define HPTXSTS_PTXFSAVL_Msk             (0xFFFFUL << HPTXSTS_PTXFSAVL_Pos)       // 0x0000FFFF */
#define HPTXSTS_PTXFSAVL                 HPTXSTS_PTXFSAVL_Msk                     // Periodic transmit data FIFO space available     */
#define HPTXSTS_PTXQSAV_Pos              (16U)
#define HPTXSTS_PTXQSAV_Msk              (0xFFUL << HPTXSTS_PTXQSAV_Pos)          // 0x00FF0000 */
#define HPTXSTS_PTXQSAV                  HPTXSTS_PTXQSAV_Msk                      // Periodic transmit request queue space available */
#define HPTXSTS_PTXQSAV_0                (0x01UL << HPTXSTS_PTXQSAV_Pos)          // 0x00010000 */
#define HPTXSTS_PTXQSAV_1                (0x02UL << HPTXSTS_PTXQSAV_Pos)          // 0x00020000 */
#define HPTXSTS_PTXQSAV_2                (0x04UL << HPTXSTS_PTXQSAV_Pos)          // 0x00040000 */
#define HPTXSTS_PTXQSAV_3                (0x08UL << HPTXSTS_PTXQSAV_Pos)          // 0x00080000 */
#define HPTXSTS_PTXQSAV_4                (0x10UL << HPTXSTS_PTXQSAV_Pos)          // 0x00100000 */
#define HPTXSTS_PTXQSAV_5                (0x20UL << HPTXSTS_PTXQSAV_Pos)          // 0x00200000 */
#define HPTXSTS_PTXQSAV_6                (0x40UL << HPTXSTS_PTXQSAV_Pos)          // 0x00400000 */
#define HPTXSTS_PTXQSAV_7                (0x80UL << HPTXSTS_PTXQSAV_Pos)          // 0x00800000 */

#define HPTXSTS_PTXQTOP_Pos              (24U)
#define HPTXSTS_PTXQTOP_Msk              (0xFFUL << HPTXSTS_PTXQTOP_Pos)          // 0xFF000000 */
#define HPTXSTS_PTXQTOP                  HPTXSTS_PTXQTOP_Msk                      // Top of the periodic transmit request queue */
#define HPTXSTS_PTXQTOP_0                (0x01UL << HPTXSTS_PTXQTOP_Pos)          // 0x01000000 */
#define HPTXSTS_PTXQTOP_1                (0x02UL << HPTXSTS_PTXQTOP_Pos)          // 0x02000000 */
#define HPTXSTS_PTXQTOP_2                (0x04UL << HPTXSTS_PTXQTOP_Pos)          // 0x04000000 */
#define HPTXSTS_PTXQTOP_3                (0x08UL << HPTXSTS_PTXQTOP_Pos)          // 0x08000000 */
#define HPTXSTS_PTXQTOP_4                (0x10UL << HPTXSTS_PTXQTOP_Pos)          // 0x10000000 */
#define HPTXSTS_PTXQTOP_5                (0x20UL << HPTXSTS_PTXQTOP_Pos)          // 0x20000000 */
#define HPTXSTS_PTXQTOP_6                (0x40UL << HPTXSTS_PTXQTOP_Pos)          // 0x40000000 */
#define HPTXSTS_PTXQTOP_7                (0x80UL << HPTXSTS_PTXQTOP_Pos)          // 0x80000000 */

/********************  Bit definition for HAINT register  ********************/
#define HAINT_HAINT_Pos                  (0U)
#define HAINT_HAINT_Msk                  (0xFFFFUL << HAINT_HAINT_Pos)            // 0x0000FFFF */
#define HAINT_HAINT                      HAINT_HAINT_Msk                          // Channel interrupts */

/********************  Bit definition for DOEPMSK register  ********************/
#define DOEPMSK_XFRCM_Pos                (0U)
#define DOEPMSK_XFRCM_Msk                (0x1UL << DOEPMSK_XFRCM_Pos)             // 0x00000001 */
#define DOEPMSK_XFRCM                    DOEPMSK_XFRCM_Msk                        // Transfer completed interrupt mask */
#define DOEPMSK_EPDM_Pos                 (1U)
#define DOEPMSK_EPDM_Msk                 (0x1UL << DOEPMSK_EPDM_Pos)              // 0x00000002 */
#define DOEPMSK_EPDM                     DOEPMSK_EPDM_Msk                         // Endpoint disabled interrupt mask               */
#define DOEPMSK_AHBERRM_Pos              (2U)
#define DOEPMSK_AHBERRM_Msk              (0x1UL << DOEPMSK_AHBERRM_Pos)           // 0x00000004 */
#define DOEPMSK_AHBERRM                  DOEPMSK_AHBERRM_Msk                      // OUT transaction AHB Error interrupt mask    */
#define DOEPMSK_STUPM_Pos                (3U)
#define DOEPMSK_STUPM_Msk                (0x1UL << DOEPMSK_STUPM_Pos)             // 0x00000008 */
#define DOEPMSK_STUPM                    DOEPMSK_STUPM_Msk                        // SETUP phase done mask                          */
#define DOEPMSK_OTEPDM_Pos               (4U)
#define DOEPMSK_OTEPDM_Msk               (0x1UL << DOEPMSK_OTEPDM_Pos)            // 0x00000010 */
#define DOEPMSK_OTEPDM                   DOEPMSK_OTEPDM_Msk                       // OUT token received when endpoint disabled mask */
#define DOEPMSK_OTEPSPRM_Pos             (5U)
#define DOEPMSK_OTEPSPRM_Msk             (0x1UL << DOEPMSK_OTEPSPRM_Pos)          // 0x00000020 */
#define DOEPMSK_OTEPSPRM                 DOEPMSK_OTEPSPRM_Msk                     // Status Phase Received mask                     */
#define DOEPMSK_B2BSTUP_Pos              (6U)
#define DOEPMSK_B2BSTUP_Msk              (0x1UL << DOEPMSK_B2BSTUP_Pos)           // 0x00000040 */
#define DOEPMSK_B2BSTUP                  DOEPMSK_B2BSTUP_Msk                      // Back-to-back SETUP packets received mask       */
#define DOEPMSK_OPEM_Pos                 (8U)
#define DOEPMSK_OPEM_Msk                 (0x1UL << DOEPMSK_OPEM_Pos)              // 0x00000100 */
#define DOEPMSK_OPEM                     DOEPMSK_OPEM_Msk                         // OUT packet error mask                          */
#define DOEPMSK_BOIM_Pos                 (9U)
#define DOEPMSK_BOIM_Msk                 (0x1UL << DOEPMSK_BOIM_Pos)              // 0x00000200 */
#define DOEPMSK_BOIM                     DOEPMSK_BOIM_Msk                         // BNA interrupt mask                             */
#define DOEPMSK_BERRM_Pos                (12U)
#define DOEPMSK_BERRM_Msk                (0x1UL << DOEPMSK_BERRM_Pos)             // 0x00001000 */
#define DOEPMSK_BERRM                    DOEPMSK_BERRM_Msk                        // Babble error interrupt mask                   */
#define DOEPMSK_NAKM_Pos                 (13U)
#define DOEPMSK_NAKM_Msk                 (0x1UL << DOEPMSK_NAKM_Pos)              // 0x00002000 */
#define DOEPMSK_NAKM                     DOEPMSK_NAKM_Msk                         // OUT Packet NAK interrupt mask                  */
#define DOEPMSK_NYETM_Pos                (14U)
#define DOEPMSK_NYETM_Msk                (0x1UL << DOEPMSK_NYETM_Pos)             // 0x00004000 */
#define DOEPMSK_NYETM                    DOEPMSK_NYETM_Msk                        // NYET interrupt mask                           */

/********************  Bit definition for GINTSTS register  ********************/
#define GINTSTS_CMOD_Pos                 (0U)
#define GINTSTS_CMOD_Msk                 (0x1UL << GINTSTS_CMOD_Pos)              // 0x00000001 */
#define GINTSTS_CMOD                     GINTSTS_CMOD_Msk                         // Current mode of operation                      */
#define GINTSTS_MMIS_Pos                 (1U)
#define GINTSTS_MMIS_Msk                 (0x1UL << GINTSTS_MMIS_Pos)              // 0x00000002 */
#define GINTSTS_MMIS                     GINTSTS_MMIS_Msk                         // Mode mismatch interrupt                        */
#define GINTSTS_OTGINT_Pos               (2U)
#define GINTSTS_OTGINT_Msk               (0x1UL << GINTSTS_OTGINT_Pos)            // 0x00000004 */
#define GINTSTS_OTGINT                   GINTSTS_OTGINT_Msk                       // OTG interrupt                                  */
#define GINTSTS_SOF_Pos                  (3U)
#define GINTSTS_SOF_Msk                  (0x1UL << GINTSTS_SOF_Pos)               // 0x00000008 */
#define GINTSTS_SOF                      GINTSTS_SOF_Msk                          // Start of frame                                 */
#define GINTSTS_RXFLVL_Pos               (4U)
#define GINTSTS_RXFLVL_Msk               (0x1UL << GINTSTS_RXFLVL_Pos)            // 0x00000010 */
#define GINTSTS_RXFLVL                   GINTSTS_RXFLVL_Msk                       // RxFIFO nonempty                                */
#define GINTSTS_NPTXFE_Pos               (5U)
#define GINTSTS_NPTXFE_Msk               (0x1UL << GINTSTS_NPTXFE_Pos)            // 0x00000020 */
#define GINTSTS_NPTXFE                   GINTSTS_NPTXFE_Msk                       // Nonperiodic TxFIFO empty                       */
#define GINTSTS_GINAKEFF_Pos             (6U)
#define GINTSTS_GINAKEFF_Msk             (0x1UL << GINTSTS_GINAKEFF_Pos)          // 0x00000040 */
#define GINTSTS_GINAKEFF                 GINTSTS_GINAKEFF_Msk                     // Global IN nonperiodic NAK effective            */
#define GINTSTS_BOUTNAKEFF_Pos           (7U)
#define GINTSTS_BOUTNAKEFF_Msk           (0x1UL << GINTSTS_BOUTNAKEFF_Pos)        // 0x00000080 */
#define GINTSTS_BOUTNAKEFF               GINTSTS_BOUTNAKEFF_Msk                   // Global OUT NAK effective                       */
#define GINTSTS_ESUSP_Pos                (10U)
#define GINTSTS_ESUSP_Msk                (0x1UL << GINTSTS_ESUSP_Pos)             // 0x00000400 */
#define GINTSTS_ESUSP                    GINTSTS_ESUSP_Msk                        // Early suspend                                  */
#define GINTSTS_USBSUSP_Pos              (11U)
#define GINTSTS_USBSUSP_Msk              (0x1UL << GINTSTS_USBSUSP_Pos)           // 0x00000800 */
#define GINTSTS_USBSUSP                  GINTSTS_USBSUSP_Msk                      // USB suspend                                    */
#define GINTSTS_USBRST_Pos               (12U)
#define GINTSTS_USBRST_Msk               (0x1UL << GINTSTS_USBRST_Pos)            // 0x00001000 */
#define GINTSTS_USBRST                   GINTSTS_USBRST_Msk                       // USB reset                                      */
#define GINTSTS_ENUMDNE_Pos              (13U)
#define GINTSTS_ENUMDNE_Msk              (0x1UL << GINTSTS_ENUMDNE_Pos)           // 0x00002000 */
#define GINTSTS_ENUMDNE                  GINTSTS_ENUMDNE_Msk                      // Enumeration done                               */
#define GINTSTS_ISOODRP_Pos              (14U)
#define GINTSTS_ISOODRP_Msk              (0x1UL << GINTSTS_ISOODRP_Pos)           // 0x00004000 */
#define GINTSTS_ISOODRP                  GINTSTS_ISOODRP_Msk                      // Isochronous OUT packet dropped interrupt       */
#define GINTSTS_EOPF_Pos                 (15U)
#define GINTSTS_EOPF_Msk                 (0x1UL << GINTSTS_EOPF_Pos)              // 0x00008000 */
#define GINTSTS_EOPF                     GINTSTS_EOPF_Msk                         // End of periodic frame interrupt                */
#define GINTSTS_IEPINT_Pos               (18U)
#define GINTSTS_IEPINT_Msk               (0x1UL << GINTSTS_IEPINT_Pos)            // 0x00040000 */
#define GINTSTS_IEPINT                   GINTSTS_IEPINT_Msk                       // IN endpoint interrupt                          */
#define GINTSTS_OEPINT_Pos               (19U)
#define GINTSTS_OEPINT_Msk               (0x1UL << GINTSTS_OEPINT_Pos)            // 0x00080000 */
#define GINTSTS_OEPINT                   GINTSTS_OEPINT_Msk                       // OUT endpoint interrupt                         */
#define GINTSTS_IISOIXFR_Pos             (20U)
#define GINTSTS_IISOIXFR_Msk             (0x1UL << GINTSTS_IISOIXFR_Pos)          // 0x00100000 */
#define GINTSTS_IISOIXFR                 GINTSTS_IISOIXFR_Msk                     // Incomplete isochronous IN transfer             */
#define GINTSTS_PXFR_INCOMPISOOUT_Pos    (21U)
#define GINTSTS_PXFR_INCOMPISOOUT_Msk    (0x1UL << GINTSTS_PXFR_INCOMPISOOUT_Pos) // 0x00200000 */
#define GINTSTS_PXFR_INCOMPISOOUT        GINTSTS_PXFR_INCOMPISOOUT_Msk            // Incomplete periodic transfer                   */
#define GINTSTS_DATAFSUSP_Pos            (22U)
#define GINTSTS_DATAFSUSP_Msk            (0x1UL << GINTSTS_DATAFSUSP_Pos)         // 0x00400000 */
#define GINTSTS_DATAFSUSP                GINTSTS_DATAFSUSP_Msk                    // Data fetch suspended                           */
#define GINTSTS_RSTDET_Pos               (23U)
#define GINTSTS_RSTDET_Msk               (0x1UL << GINTSTS_RSTDET_Pos)            // 0x00800000 */
#define GINTSTS_RSTDET                   GINTSTS_RSTDET_Msk                       // Reset detected interrupt                       */
#define GINTSTS_HPRTINT_Pos              (24U)
#define GINTSTS_HPRTINT_Msk              (0x1UL << GINTSTS_HPRTINT_Pos)           // 0x01000000 */
#define GINTSTS_HPRTINT                  GINTSTS_HPRTINT_Msk                      // Host port interrupt                            */
#define GINTSTS_HCINT_Pos                (25U)
#define GINTSTS_HCINT_Msk                (0x1UL << GINTSTS_HCINT_Pos)             // 0x02000000 */
#define GINTSTS_HCINT                    GINTSTS_HCINT_Msk                        // Host channels interrupt                        */
#define GINTSTS_PTXFE_Pos                (26U)
#define GINTSTS_PTXFE_Msk                (0x1UL << GINTSTS_PTXFE_Pos)             // 0x04000000 */
#define GINTSTS_PTXFE                    GINTSTS_PTXFE_Msk                        // Periodic TxFIFO empty                          */
#define GINTSTS_LPMINT_Pos               (27U)
#define GINTSTS_LPMINT_Msk               (0x1UL << GINTSTS_LPMINT_Pos)            // 0x08000000 */
#define GINTSTS_LPMINT                   GINTSTS_LPMINT_Msk                       // LPM interrupt                                  */
#define GINTSTS_CIDSCHG_Pos              (28U)
#define GINTSTS_CIDSCHG_Msk              (0x1UL << GINTSTS_CIDSCHG_Pos)           // 0x10000000 */
#define GINTSTS_CIDSCHG                  GINTSTS_CIDSCHG_Msk                      // Connector ID status change                     */
#define GINTSTS_DISCINT_Pos              (29U)
#define GINTSTS_DISCINT_Msk              (0x1UL << GINTSTS_DISCINT_Pos)           // 0x20000000 */
#define GINTSTS_DISCINT                  GINTSTS_DISCINT_Msk                      // Disconnect detected interrupt                  */
#define GINTSTS_SRQINT_Pos               (30U)
#define GINTSTS_SRQINT_Msk               (0x1UL << GINTSTS_SRQINT_Pos)            // 0x40000000 */
#define GINTSTS_SRQINT                   GINTSTS_SRQINT_Msk                       // Session request/new session detected interrupt */
#define GINTSTS_WKUINT_Pos               (31U)
#define GINTSTS_WKUINT_Msk               (0x1UL << GINTSTS_WKUINT_Pos)            // 0x80000000 */
#define GINTSTS_WKUINT                   GINTSTS_WKUINT_Msk                       // Resume/remote wakeup detected interrupt        */

/********************  Bit definition for GINTMSK register  ********************/
#define GINTMSK_MMISM_Pos                (1U)
#define GINTMSK_MMISM_Msk                (0x1UL << GINTMSK_MMISM_Pos)             // 0x00000002 */
#define GINTMSK_MMISM                    GINTMSK_MMISM_Msk                        // Mode mismatch interrupt mask                        */
#define GINTMSK_OTGINT_Pos               (2U)
#define GINTMSK_OTGINT_Msk               (0x1UL << GINTMSK_OTGINT_Pos)            // 0x00000004 */
#define GINTMSK_OTGINT                   GINTMSK_OTGINT_Msk                       // OTG interrupt mask                                  */
#define GINTMSK_SOFM_Pos                 (3U)
#define GINTMSK_SOFM_Msk                 (0x1UL << GINTMSK_SOFM_Pos)              // 0x00000008 */
#define GINTMSK_SOFM                     GINTMSK_SOFM_Msk                         // Start of frame mask                                 */
#define GINTMSK_RXFLVLM_Pos              (4U)
#define GINTMSK_RXFLVLM_Msk              (0x1UL << GINTMSK_RXFLVLM_Pos)           // 0x00000010 */
#define GINTMSK_RXFLVLM                  GINTMSK_RXFLVLM_Msk                      // Receive FIFO nonempty mask                          */
#define GINTMSK_NPTXFEM_Pos              (5U)
#define GINTMSK_NPTXFEM_Msk              (0x1UL << GINTMSK_NPTXFEM_Pos)           // 0x00000020 */
#define GINTMSK_NPTXFEM                  GINTMSK_NPTXFEM_Msk                      // Nonperiodic TxFIFO empty mask                       */
#define GINTMSK_GINAKEFFM_Pos            (6U)
#define GINTMSK_GINAKEFFM_Msk            (0x1UL << GINTMSK_GINAKEFFM_Pos)         // 0x00000040 */
#define GINTMSK_GINAKEFFM                GINTMSK_GINAKEFFM_Msk                    // Global nonperiodic IN NAK effective mask            */
#define GINTMSK_GONAKEFFM_Pos            (7U)
#define GINTMSK_GONAKEFFM_Msk            (0x1UL << GINTMSK_GONAKEFFM_Pos)         // 0x00000080 */
#define GINTMSK_GONAKEFFM                GINTMSK_GONAKEFFM_Msk                    // Global OUT NAK effective mask                       */
#define GINTMSK_ESUSPM_Pos               (10U)
#define GINTMSK_ESUSPM_Msk               (0x1UL << GINTMSK_ESUSPM_Pos)            // 0x00000400 */
#define GINTMSK_ESUSPM                   GINTMSK_ESUSPM_Msk                       // Early suspend mask                                  */
#define GINTMSK_USBSUSPM_Pos             (11U)
#define GINTMSK_USBSUSPM_Msk             (0x1UL << GINTMSK_USBSUSPM_Pos)          // 0x00000800 */
#define GINTMSK_USBSUSPM                 GINTMSK_USBSUSPM_Msk                     // USB suspend mask                                    */
#define GINTMSK_USBRST_Pos               (12U)
#define GINTMSK_USBRST_Msk               (0x1UL << GINTMSK_USBRST_Pos)            // 0x00001000 */
#define GINTMSK_USBRST                   GINTMSK_USBRST_Msk                       // USB reset mask                                      */
#define GINTMSK_ENUMDNEM_Pos             (13U)
#define GINTMSK_ENUMDNEM_Msk             (0x1UL << GINTMSK_ENUMDNEM_Pos)          // 0x00002000 */
#define GINTMSK_ENUMDNEM                 GINTMSK_ENUMDNEM_Msk                     // Enumeration done mask                               */
#define GINTMSK_ISOODRPM_Pos             (14U)
#define GINTMSK_ISOODRPM_Msk             (0x1UL << GINTMSK_ISOODRPM_Pos)          // 0x00004000 */
#define GINTMSK_ISOODRPM                 GINTMSK_ISOODRPM_Msk                     // Isochronous OUT packet dropped interrupt mask       */
#define GINTMSK_EOPFM_Pos                (15U)
#define GINTMSK_EOPFM_Msk                (0x1UL << GINTMSK_EOPFM_Pos)             // 0x00008000 */
#define GINTMSK_EOPFM                    GINTMSK_EOPFM_Msk                        // End of periodic frame interrupt mask                */
#define GINTMSK_EPMISM_Pos               (17U)
#define GINTMSK_EPMISM_Msk               (0x1UL << GINTMSK_EPMISM_Pos)            // 0x00020000 */
#define GINTMSK_EPMISM                   GINTMSK_EPMISM_Msk                       // Endpoint mismatch interrupt mask                    */
#define GINTMSK_IEPINT_Pos               (18U)
#define GINTMSK_IEPINT_Msk               (0x1UL << GINTMSK_IEPINT_Pos)            // 0x00040000 */
#define GINTMSK_IEPINT                   GINTMSK_IEPINT_Msk                       // IN endpoints interrupt mask                         */
#define GINTMSK_OEPINT_Pos               (19U)
#define GINTMSK_OEPINT_Msk               (0x1UL << GINTMSK_OEPINT_Pos)            // 0x00080000 */
#define GINTMSK_OEPINT                   GINTMSK_OEPINT_Msk                       // OUT endpoints interrupt mask                        */
#define GINTMSK_IISOIXFRM_Pos            (20U)
#define GINTMSK_IISOIXFRM_Msk            (0x1UL << GINTMSK_IISOIXFRM_Pos)         // 0x00100000 */
#define GINTMSK_IISOIXFRM                GINTMSK_IISOIXFRM_Msk                    // Incomplete isochronous IN transfer mask             */
#define GINTMSK_PXFRM_IISOOXFRM_Pos      (21U)
#define GINTMSK_PXFRM_IISOOXFRM_Msk      (0x1UL << GINTMSK_PXFRM_IISOOXFRM_Pos)   // 0x00200000 */
#define GINTMSK_PXFRM_IISOOXFRM          GINTMSK_PXFRM_IISOOXFRM_Msk              // Incomplete periodic transfer mask                   */
#define GINTMSK_FSUSPM_Pos               (22U)
#define GINTMSK_FSUSPM_Msk               (0x1UL << GINTMSK_FSUSPM_Pos)            // 0x00400000 */
#define GINTMSK_FSUSPM                   GINTMSK_FSUSPM_Msk                       // Data fetch suspended mask                           */
#define GINTMSK_RSTDEM_Pos               (23U)
#define GINTMSK_RSTDEM_Msk               (0x1UL << GINTMSK_RSTDEM_Pos)            // 0x00800000 */
#define GINTMSK_RSTDEM                   GINTMSK_RSTDEM_Msk                       // Reset detected interrupt mask                       */
#define GINTMSK_PRTIM_Pos                (24U)
#define GINTMSK_PRTIM_Msk                (0x1UL << GINTMSK_PRTIM_Pos)             // 0x01000000 */
#define GINTMSK_PRTIM                    GINTMSK_PRTIM_Msk                        // Host port interrupt mask                            */
#define GINTMSK_HCIM_Pos                 (25U)
#define GINTMSK_HCIM_Msk                 (0x1UL << GINTMSK_HCIM_Pos)              // 0x02000000 */
#define GINTMSK_HCIM                     GINTMSK_HCIM_Msk                         // Host channels interrupt mask                        */
#define GINTMSK_PTXFEM_Pos               (26U)
#define GINTMSK_PTXFEM_Msk               (0x1UL << GINTMSK_PTXFEM_Pos)            // 0x04000000 */
#define GINTMSK_PTXFEM                   GINTMSK_PTXFEM_Msk                       // Periodic TxFIFO empty mask                          */
#define GINTMSK_LPMINTM_Pos              (27U)
#define GINTMSK_LPMINTM_Msk              (0x1UL << GINTMSK_LPMINTM_Pos)           // 0x08000000 */
#define GINTMSK_LPMINTM                  GINTMSK_LPMINTM_Msk                      // LPM interrupt Mask                                  */
#define GINTMSK_CIDSCHGM_Pos             (28U)
#define GINTMSK_CIDSCHGM_Msk             (0x1UL << GINTMSK_CIDSCHGM_Pos)          // 0x10000000 */
#define GINTMSK_CIDSCHGM                 GINTMSK_CIDSCHGM_Msk                     // Connector ID status change mask                     */
#define GINTMSK_DISCINT_Pos              (29U)
#define GINTMSK_DISCINT_Msk              (0x1UL << GINTMSK_DISCINT_Pos)           // 0x20000000 */
#define GINTMSK_DISCINT                  GINTMSK_DISCINT_Msk                      // Disconnect detected interrupt mask                  */
#define GINTMSK_SRQIM_Pos                (30U)
#define GINTMSK_SRQIM_Msk                (0x1UL << GINTMSK_SRQIM_Pos)             // 0x40000000 */
#define GINTMSK_SRQIM                    GINTMSK_SRQIM_Msk                        // Session request/new session detected interrupt mask */
#define GINTMSK_WUIM_Pos                 (31U)
#define GINTMSK_WUIM_Msk                 (0x1UL << GINTMSK_WUIM_Pos)              // 0x80000000 */
#define GINTMSK_WUIM                     GINTMSK_WUIM_Msk                         // Resume/remote wakeup detected interrupt mask        */

/********************  Bit definition for DAINT register  ********************/
#define DAINT_IEPINT_Pos                 (0U)
#define DAINT_IEPINT_Msk                 (0xFFFFUL << DAINT_IEPINT_Pos)           // 0x0000FFFF */
#define DAINT_IEPINT                     DAINT_IEPINT_Msk                         // IN endpoint interrupt bits  */
#define DAINT_OEPINT_Pos                 (16U)
#define DAINT_OEPINT_Msk                 (0xFFFFUL << DAINT_OEPINT_Pos)           // 0xFFFF0000 */
#define DAINT_OEPINT                     DAINT_OEPINT_Msk                         // OUT endpoint interrupt bits */

/********************  Bit definition for HAINTMSK register  ********************/
#define HAINTMSK_HAINTM_Pos              (0U)
#define HAINTMSK_HAINTM_Msk              (0xFFFFUL << HAINTMSK_HAINTM_Pos)        // 0x0000FFFF */
#define HAINTMSK_HAINTM                  HAINTMSK_HAINTM_Msk                      // Channel interrupt mask */

/********************  Bit definition for GRXSTSP register  ********************/
#define GRXSTSP_EPNUM_Pos                (0U)
#define GRXSTSP_EPNUM_Msk                (0xFUL << GRXSTSP_EPNUM_Pos)             // 0x0000000F */
#define GRXSTSP_EPNUM                    GRXSTSP_EPNUM_Msk                        // IN EP interrupt mask bits  */
#define GRXSTSP_BCNT_Pos                 (4U)
#define GRXSTSP_BCNT_Msk                 (0x7FFUL << GRXSTSP_BCNT_Pos)            // 0x00007FF0 */
#define GRXSTSP_BCNT                     GRXSTSP_BCNT_Msk                         // OUT EP interrupt mask bits */
#define GRXSTSP_DPID_Pos                 (15U)
#define GRXSTSP_DPID_Msk                 (0x3UL << GRXSTSP_DPID_Pos)              // 0x00018000 */
#define GRXSTSP_DPID                     GRXSTSP_DPID_Msk                         // OUT EP interrupt mask bits */
#define GRXSTSP_PKTSTS_Pos               (17U)
#define GRXSTSP_PKTSTS_Msk               (0xFUL << GRXSTSP_PKTSTS_Pos)            // 0x001E0000 */
#define GRXSTSP_PKTSTS                   GRXSTSP_PKTSTS_Msk                       // OUT EP interrupt mask bits */

/********************  Bit definition for DAINTMSK register  ********************/
#define DAINTMSK_IEPM_Pos                (0U)
#define DAINTMSK_IEPM_Msk                (0xFFFFUL << DAINTMSK_IEPM_Pos)          // 0x0000FFFF */
#define DAINTMSK_IEPM                    DAINTMSK_IEPM_Msk                        // IN EP interrupt mask bits */
#define DAINTMSK_OEPM_Pos                (16U)
#define DAINTMSK_OEPM_Msk                (0xFFFFUL << DAINTMSK_OEPM_Pos)          // 0xFFFF0000 */
#define DAINTMSK_OEPM                    DAINTMSK_OEPM_Msk                        // OUT EP interrupt mask bits */

/********************  Bit definition for OTG register  ********************/
#define CHNUM_Pos                        (0U)
#define CHNUM_Msk                        (0xFUL << CHNUM_Pos)                     // 0x0000000F */
#define CHNUM                            CHNUM_Msk                                // Channel number */
#define CHNUM_0                          (0x1UL << CHNUM_Pos)                     // 0x00000001 */
#define CHNUM_1                          (0x2UL << CHNUM_Pos)                     // 0x00000002 */
#define CHNUM_2                          (0x4UL << CHNUM_Pos)                     // 0x00000004 */
#define CHNUM_3                          (0x8UL << CHNUM_Pos)                     // 0x00000008 */
#define BCNT_Pos                         (4U)
#define BCNT_Msk                         (0x7FFUL << BCNT_Pos)                    // 0x00007FF0 */
#define BCNT                             BCNT_Msk                                 // Byte count */

#define DPID_Pos                         (15U)
#define DPID_Msk                         (0x3UL << DPID_Pos)                      // 0x00018000 */
#define DPID                             DPID_Msk                                 // Data PID */
#define DPID_0                           (0x1UL << DPID_Pos)                      // 0x00008000 */
#define DPID_1                           (0x2UL << DPID_Pos)                      // 0x00010000 */

#define PKTSTS_Pos                       (17U)
#define PKTSTS_Msk                       (0xFUL << PKTSTS_Pos)                    // 0x001E0000 */
#define PKTSTS                           PKTSTS_Msk                               // Packet status */
#define PKTSTS_0                         (0x1UL << PKTSTS_Pos)                    // 0x00020000 */
#define PKTSTS_1                         (0x2UL << PKTSTS_Pos)                    // 0x00040000 */
#define PKTSTS_2                         (0x4UL << PKTSTS_Pos)                    // 0x00080000 */
#define PKTSTS_3                         (0x8UL << PKTSTS_Pos)                    // 0x00100000 */

#define EPNUM_Pos                        (0U)
#define EPNUM_Msk                        (0xFUL << EPNUM_Pos)                     // 0x0000000F */
#define EPNUM                            EPNUM_Msk                                // Endpoint number */
#define EPNUM_0                          (0x1UL << EPNUM_Pos)                     // 0x00000001 */
#define EPNUM_1                          (0x2UL << EPNUM_Pos)                     // 0x00000002 */
#define EPNUM_2                          (0x4UL << EPNUM_Pos)                     // 0x00000004 */
#define EPNUM_3                          (0x8UL << EPNUM_Pos)                     // 0x00000008 */

#define FRMNUM_Pos                       (21U)
#define FRMNUM_Msk                       (0xFUL << FRMNUM_Pos)                    // 0x01E00000 */
#define FRMNUM                           FRMNUM_Msk                               // Frame number */
#define FRMNUM_0                         (0x1UL << FRMNUM_Pos)                    // 0x00200000 */
#define FRMNUM_1                         (0x2UL << FRMNUM_Pos)                    // 0x00400000 */
#define FRMNUM_2                         (0x4UL << FRMNUM_Pos)                    // 0x00800000 */
#define FRMNUM_3                         (0x8UL << FRMNUM_Pos)                    // 0x01000000 */

/********************  Bit definition for GRXFSIZ register  ********************/
#define GRXFSIZ_RXFD_Pos                 (0U)
#define GRXFSIZ_RXFD_Msk                 (0xFFFFUL << GRXFSIZ_RXFD_Pos)           // 0x0000FFFF */
#define GRXFSIZ_RXFD                     GRXFSIZ_RXFD_Msk                         // RxFIFO depth */

/********************  Bit definition for DVBUSDIS register  ********************/
#define DVBUSDIS_VBUSDT_Pos              (0U)
#define DVBUSDIS_VBUSDT_Msk              (0xFFFFUL << DVBUSDIS_VBUSDT_Pos)        // 0x0000FFFF */
#define DVBUSDIS_VBUSDT                  DVBUSDIS_VBUSDT_Msk                      // Device VBUS discharge time */

/********************  Bit definition for OTG register  ********************/
#define NPTXFSA_Pos                      (0U)
#define NPTXFSA_Msk                      (0xFFFFUL << NPTXFSA_Pos)                // 0x0000FFFF */
#define NPTXFSA                          NPTXFSA_Msk                              // Nonperiodic transmit RAM start address */
#define NPTXFD_Pos                       (16U)
#define NPTXFD_Msk                       (0xFFFFUL << NPTXFD_Pos)                 // 0xFFFF0000 */
#define NPTXFD                           NPTXFD_Msk                               // Nonperiodic TxFIFO depth               */
#define TX0FSA_Pos                       (0U)
#define TX0FSA_Msk                       (0xFFFFUL << TX0FSA_Pos)                 // 0x0000FFFF */
#define TX0FSA                           TX0FSA_Msk                               // Endpoint 0 transmit RAM start address  */
#define TX0FD_Pos                        (16U)
#define TX0FD_Msk                        (0xFFFFUL << TX0FD_Pos)                  // 0xFFFF0000 */
#define TX0FD                            TX0FD_Msk                                // Endpoint 0 TxFIFO depth                */

/********************  Bit definition for DVBUSPULSE register  ********************/
#define DVBUSPULSE_DVBUSP_Pos            (0U)
#define DVBUSPULSE_DVBUSP_Msk            (0xFFFUL << DVBUSPULSE_DVBUSP_Pos)       // 0x00000FFF */
#define DVBUSPULSE_DVBUSP                DVBUSPULSE_DVBUSP_Msk                    // Device VBUS pulsing time */

/********************  Bit definition for GNPTXSTS register  ********************/
#define GNPTXSTS_NPTXFSAV_Pos            (0U)
#define GNPTXSTS_NPTXFSAV_Msk            (0xFFFFUL << GNPTXSTS_NPTXFSAV_Pos)      // 0x0000FFFF */
#define GNPTXSTS_NPTXFSAV                GNPTXSTS_NPTXFSAV_Msk                    // Nonperiodic TxFIFO space available */

#define GNPTXSTS_NPTQXSAV_Pos            (16U)
#define GNPTXSTS_NPTQXSAV_Msk            (0xFFUL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00FF0000 */
#define GNPTXSTS_NPTQXSAV                GNPTXSTS_NPTQXSAV_Msk                    // Nonperiodic transmit request queue space available */
#define GNPTXSTS_NPTQXSAV_0              (0x01UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00010000 */
#define GNPTXSTS_NPTQXSAV_1              (0x02UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00020000 */
#define GNPTXSTS_NPTQXSAV_2              (0x04UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00040000 */
#define GNPTXSTS_NPTQXSAV_3              (0x08UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00080000 */
#define GNPTXSTS_NPTQXSAV_4              (0x10UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00100000 */
#define GNPTXSTS_NPTQXSAV_5              (0x20UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00200000 */
#define GNPTXSTS_NPTQXSAV_6              (0x40UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00400000 */
#define GNPTXSTS_NPTQXSAV_7              (0x80UL << GNPTXSTS_NPTQXSAV_Pos)        // 0x00800000 */

#define GNPTXSTS_NPTXQTOP_Pos            (24U)
#define GNPTXSTS_NPTXQTOP_Msk            (0x7FUL << GNPTXSTS_NPTXQTOP_Pos)        // 0x7F000000 */
#define GNPTXSTS_NPTXQTOP                GNPTXSTS_NPTXQTOP_Msk                    // Top of the nonperiodic transmit request queue */
#define GNPTXSTS_NPTXQTOP_0              (0x01UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x01000000 */
#define GNPTXSTS_NPTXQTOP_1              (0x02UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x02000000 */
#define GNPTXSTS_NPTXQTOP_2              (0x04UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x04000000 */
#define GNPTXSTS_NPTXQTOP_3              (0x08UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x08000000 */
#define GNPTXSTS_NPTXQTOP_4              (0x10UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x10000000 */
#define GNPTXSTS_NPTXQTOP_5              (0x20UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x20000000 */
#define GNPTXSTS_NPTXQTOP_6              (0x40UL << GNPTXSTS_NPTXQTOP_Pos)        // 0x40000000 */

/********************  Bit definition for DTHRCTL register  ********************/
#define DTHRCTL_NONISOTHREN_Pos          (0U)
#define DTHRCTL_NONISOTHREN_Msk          (0x1UL << DTHRCTL_NONISOTHREN_Pos)       // 0x00000001 */
#define DTHRCTL_NONISOTHREN              DTHRCTL_NONISOTHREN_Msk                  // Nonisochronous IN endpoints threshold enable */
#define DTHRCTL_ISOTHREN_Pos             (1U)
#define DTHRCTL_ISOTHREN_Msk             (0x1UL << DTHRCTL_ISOTHREN_Pos)          // 0x00000002 */
#define DTHRCTL_ISOTHREN                 DTHRCTL_ISOTHREN_Msk                     // ISO IN endpoint threshold enable */

#define DTHRCTL_TXTHRLEN_Pos             (2U)
#define DTHRCTL_TXTHRLEN_Msk             (0x1FFUL << DTHRCTL_TXTHRLEN_Pos)        // 0x000007FC */
#define DTHRCTL_TXTHRLEN                 DTHRCTL_TXTHRLEN_Msk                     // Transmit threshold length */
#define DTHRCTL_TXTHRLEN_0               (0x001UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000004 */
#define DTHRCTL_TXTHRLEN_1               (0x002UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000008 */
#define DTHRCTL_TXTHRLEN_2               (0x004UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000010 */
#define DTHRCTL_TXTHRLEN_3               (0x008UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000020 */
#define DTHRCTL_TXTHRLEN_4               (0x010UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000040 */
#define DTHRCTL_TXTHRLEN_5               (0x020UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000080 */
#define DTHRCTL_TXTHRLEN_6               (0x040UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000100 */
#define DTHRCTL_TXTHRLEN_7               (0x080UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000200 */
#define DTHRCTL_TXTHRLEN_8               (0x100UL << DTHRCTL_TXTHRLEN_Pos)        // 0x00000400 */
#define DTHRCTL_RXTHREN_Pos              (16U)
#define DTHRCTL_RXTHREN_Msk              (0x1UL << DTHRCTL_RXTHREN_Pos)           // 0x00010000 */
#define DTHRCTL_RXTHREN                  DTHRCTL_RXTHREN_Msk                      // Receive threshold enable */

#define DTHRCTL_RXTHRLEN_Pos             (17U)
#define DTHRCTL_RXTHRLEN_Msk             (0x1FFUL << DTHRCTL_RXTHRLEN_Pos)        // 0x03FE0000 */
#define DTHRCTL_RXTHRLEN                 DTHRCTL_RXTHRLEN_Msk                     // Receive threshold length */
#define DTHRCTL_RXTHRLEN_0               (0x001UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00020000 */
#define DTHRCTL_RXTHRLEN_1               (0x002UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00040000 */
#define DTHRCTL_RXTHRLEN_2               (0x004UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00080000 */
#define DTHRCTL_RXTHRLEN_3               (0x008UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00100000 */
#define DTHRCTL_RXTHRLEN_4               (0x010UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00200000 */
#define DTHRCTL_RXTHRLEN_5               (0x020UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00400000 */
#define DTHRCTL_RXTHRLEN_6               (0x040UL << DTHRCTL_RXTHRLEN_Pos)        // 0x00800000 */
#define DTHRCTL_RXTHRLEN_7               (0x080UL << DTHRCTL_RXTHRLEN_Pos)        // 0x01000000 */
#define DTHRCTL_RXTHRLEN_8               (0x100UL << DTHRCTL_RXTHRLEN_Pos)        // 0x02000000 */
#define DTHRCTL_ARPEN_Pos                (27U)
#define DTHRCTL_ARPEN_Msk                (0x1UL << DTHRCTL_ARPEN_Pos)             // 0x08000000 */
#define DTHRCTL_ARPEN                    DTHRCTL_ARPEN_Msk                        // Arbiter parking enable */

/********************  Bit definition for DIEPEMPMSK register  ********************/
#define DIEPEMPMSK_INEPTXFEM_Pos         (0U)
#define DIEPEMPMSK_INEPTXFEM_Msk         (0xFFFFUL << DIEPEMPMSK_INEPTXFEM_Pos)   // 0x0000FFFF */
#define DIEPEMPMSK_INEPTXFEM             DIEPEMPMSK_INEPTXFEM_Msk                 // IN EP Tx FIFO empty interrupt mask bits */

/********************  Bit definition for DEACHINT register  ********************/
#define DEACHINT_IEP1INT_Pos             (1U)
#define DEACHINT_IEP1INT_Msk             (0x1UL << DEACHINT_IEP1INT_Pos)          // 0x00000002 */
#define DEACHINT_IEP1INT                 DEACHINT_IEP1INT_Msk                     // IN endpoint 1interrupt bit   */
#define DEACHINT_OEP1INT_Pos             (17U)
#define DEACHINT_OEP1INT_Msk             (0x1UL << DEACHINT_OEP1INT_Pos)          // 0x00020000 */
#define DEACHINT_OEP1INT                 DEACHINT_OEP1INT_Msk                     // OUT endpoint 1 interrupt bit */

/********************  Bit definition for GCCFG register  ********************/
#define GCCFG_DCDET_Pos                  (0U)
#define GCCFG_DCDET_Msk                  (0x1UL << GCCFG_DCDET_Pos)               // 0x00000001 */
#define GCCFG_DCDET                      GCCFG_DCDET_Msk                          // Data contact detection (DCD) status */
#define GCCFG_PDET_Pos                   (1U)
#define GCCFG_PDET_Msk                   (0x1UL << GCCFG_PDET_Pos)                // 0x00000002 */
#define GCCFG_PDET                       GCCFG_PDET_Msk                           // Primary detection (PD) status */
#define GCCFG_SDET_Pos                   (2U)
#define GCCFG_SDET_Msk                   (0x1UL << GCCFG_SDET_Pos)                // 0x00000004 */
#define GCCFG_SDET                       GCCFG_SDET_Msk                           // Secondary detection (SD) status */
#define GCCFG_PS2DET_Pos                 (3U)
#define GCCFG_PS2DET_Msk                 (0x1UL << GCCFG_PS2DET_Pos)              // 0x00000008 */
#define GCCFG_PS2DET                     GCCFG_PS2DET_Msk                         // DM pull-up detection status */
#define GCCFG_PWRDWN_Pos                 (16U)
#define GCCFG_PWRDWN_Msk                 (0x1UL << GCCFG_PWRDWN_Pos)              // 0x00010000 */
#define GCCFG_PWRDWN                     GCCFG_PWRDWN_Msk                         // Power down */
#define GCCFG_BCDEN_Pos                  (17U)
#define GCCFG_BCDEN_Msk                  (0x1UL << GCCFG_BCDEN_Pos)               // 0x00020000 */
#define GCCFG_BCDEN                      GCCFG_BCDEN_Msk                          // Battery charging detector (BCD) enable */
#define GCCFG_DCDEN_Pos                  (18U)
#define GCCFG_DCDEN_Msk                  (0x1UL << GCCFG_DCDEN_Pos)               // 0x00040000 */
#define GCCFG_DCDEN                      GCCFG_DCDEN_Msk                          // Data contact detection (DCD) mode enable*/
#define GCCFG_PDEN_Pos                   (19U)
#define GCCFG_PDEN_Msk                   (0x1UL << GCCFG_PDEN_Pos)                // 0x00080000 */
#define GCCFG_PDEN                       GCCFG_PDEN_Msk                           // Primary detection (PD) mode enable*/
#define GCCFG_SDEN_Pos                   (20U)
#define GCCFG_SDEN_Msk                   (0x1UL << GCCFG_SDEN_Pos)                // 0x00100000 */
#define GCCFG_SDEN                       GCCFG_SDEN_Msk                           // Secondary detection (SD) mode enable */
#define GCCFG_VBDEN_Pos                  (21U)
#define GCCFG_VBDEN_Msk                  (0x1UL << GCCFG_VBDEN_Pos)               // 0x00200000 */
#define GCCFG_VBDEN                      GCCFG_VBDEN_Msk                          // VBUS mode enable */
#define GCCFG_OTGIDEN_Pos                (22U)
#define GCCFG_OTGIDEN_Msk                (0x1UL << GCCFG_OTGIDEN_Pos)             // 0x00400000 */
#define GCCFG_OTGIDEN                    GCCFG_OTGIDEN_Msk                        // OTG Id enable */
#define GCCFG_PHYHSEN_Pos                (23U)
#define GCCFG_PHYHSEN_Msk                (0x1UL << GCCFG_PHYHSEN_Pos)             // 0x00800000 */
#define GCCFG_PHYHSEN                    GCCFG_PHYHSEN_Msk                        // HS PHY enable */

/********************  Bit definition for DEACHINTMSK register  ********************/
#define DEACHINTMSK_IEP1INTM_Pos         (1U)
#define DEACHINTMSK_IEP1INTM_Msk         (0x1UL << DEACHINTMSK_IEP1INTM_Pos)      // 0x00000002 */
#define DEACHINTMSK_IEP1INTM             DEACHINTMSK_IEP1INTM_Msk                 // IN Endpoint 1 interrupt mask bit  */
#define DEACHINTMSK_OEP1INTM_Pos         (17U)
#define DEACHINTMSK_OEP1INTM_Msk         (0x1UL << DEACHINTMSK_OEP1INTM_Pos)      // 0x00020000 */
#define DEACHINTMSK_OEP1INTM             DEACHINTMSK_OEP1INTM_Msk                 // OUT Endpoint 1 interrupt mask bit */

/********************  Bit definition for CID register  ********************/
#define CID_PRODUCT_ID_Pos               (0U)
#define CID_PRODUCT_ID_Msk               (0xFFFFFFFFUL << CID_PRODUCT_ID_Pos)     // 0xFFFFFFFF */
#define CID_PRODUCT_ID                   CID_PRODUCT_ID_Msk                       // Product ID field */

/********************  Bit definition for GLPMCFG register  ********************/
#define GLPMCFG_LPMEN_Pos                (0U)
#define GLPMCFG_LPMEN_Msk                (0x1UL << GLPMCFG_LPMEN_Pos)             // 0x00000001 */
#define GLPMCFG_LPMEN                    GLPMCFG_LPMEN_Msk                        // LPM support enable                                     */
#define GLPMCFG_LPMACK_Pos               (1U)
#define GLPMCFG_LPMACK_Msk               (0x1UL << GLPMCFG_LPMACK_Pos)            // 0x00000002 */
#define GLPMCFG_LPMACK                   GLPMCFG_LPMACK_Msk                       // LPM Token acknowledge enable                           */
#define GLPMCFG_BESL_Pos                 (2U)
#define GLPMCFG_BESL_Msk                 (0xFUL << GLPMCFG_BESL_Pos)              // 0x0000003C */
#define GLPMCFG_BESL                     GLPMCFG_BESL_Msk                         // BESL value received with last ACKed LPM Token          */
#define GLPMCFG_REMWAKE_Pos              (6U)
#define GLPMCFG_REMWAKE_Msk              (0x1UL << GLPMCFG_REMWAKE_Pos)           // 0x00000040 */
#define GLPMCFG_REMWAKE                  GLPMCFG_REMWAKE_Msk                      // bRemoteWake value received with last ACKed LPM Token   */
#define GLPMCFG_L1SSEN_Pos               (7U)
#define GLPMCFG_L1SSEN_Msk               (0x1UL << GLPMCFG_L1SSEN_Pos)            // 0x00000080 */
#define GLPMCFG_L1SSEN                   GLPMCFG_L1SSEN_Msk                       // L1 shallow sleep enable                                */
#define GLPMCFG_BESLTHRS_Pos             (8U)
#define GLPMCFG_BESLTHRS_Msk             (0xFUL << GLPMCFG_BESLTHRS_Pos)          // 0x00000F00 */
#define GLPMCFG_BESLTHRS                 GLPMCFG_BESLTHRS_Msk                     // BESL threshold                                         */
#define GLPMCFG_L1DSEN_Pos               (12U)
#define GLPMCFG_L1DSEN_Msk               (0x1UL << GLPMCFG_L1DSEN_Pos)            // 0x00001000 */
#define GLPMCFG_L1DSEN                   GLPMCFG_L1DSEN_Msk                       // L1 deep sleep enable                                   */
#define GLPMCFG_LPMRSP_Pos               (13U)
#define GLPMCFG_LPMRSP_Msk               (0x3UL << GLPMCFG_LPMRSP_Pos)            // 0x00006000 */
#define GLPMCFG_LPMRSP                   GLPMCFG_LPMRSP_Msk                       // LPM response                                           */
#define GLPMCFG_SLPSTS_Pos               (15U)
#define GLPMCFG_SLPSTS_Msk               (0x1UL << GLPMCFG_SLPSTS_Pos)            // 0x00008000 */
#define GLPMCFG_SLPSTS                   GLPMCFG_SLPSTS_Msk                       // Port sleep status                                      */
#define GLPMCFG_L1RSMOK_Pos              (16U)
#define GLPMCFG_L1RSMOK_Msk              (0x1UL << GLPMCFG_L1RSMOK_Pos)           // 0x00010000 */
#define GLPMCFG_L1RSMOK                  GLPMCFG_L1RSMOK_Msk                      // Sleep State Resume OK                                  */
#define GLPMCFG_LPMCHIDX_Pos             (17U)
#define GLPMCFG_LPMCHIDX_Msk             (0xFUL << GLPMCFG_LPMCHIDX_Pos)          // 0x001E0000 */
#define GLPMCFG_LPMCHIDX                 GLPMCFG_LPMCHIDX_Msk                     // LPM Channel Index                                      */
#define GLPMCFG_LPMRCNT_Pos              (21U)
#define GLPMCFG_LPMRCNT_Msk              (0x7UL << GLPMCFG_LPMRCNT_Pos)           // 0x00E00000 */
#define GLPMCFG_LPMRCNT                  GLPMCFG_LPMRCNT_Msk                      // LPM retry count                                        */
#define GLPMCFG_SNDLPM_Pos               (24U)
#define GLPMCFG_SNDLPM_Msk               (0x1UL << GLPMCFG_SNDLPM_Pos)            // 0x01000000 */
#define GLPMCFG_SNDLPM                   GLPMCFG_SNDLPM_Msk                       // Send LPM transaction                                   */
#define GLPMCFG_LPMRCNTSTS_Pos           (25U)
#define GLPMCFG_LPMRCNTSTS_Msk           (0x7UL << GLPMCFG_LPMRCNTSTS_Pos)        // 0x0E000000 */
#define GLPMCFG_LPMRCNTSTS               GLPMCFG_LPMRCNTSTS_Msk                   // LPM retry count status                                 */
#define GLPMCFG_ENBESL_Pos               (28U)
#define GLPMCFG_ENBESL_Msk               (0x1UL << GLPMCFG_ENBESL_Pos)            // 0x10000000 */
#define GLPMCFG_ENBESL                   GLPMCFG_ENBESL_Msk                       // Enable best effort service latency                     */

/********************  Bit definition for DIEPEACHMSK1 register  ********************/
#define DIEPEACHMSK1_XFRCM_Pos           (0U)
#define DIEPEACHMSK1_XFRCM_Msk           (0x1UL << DIEPEACHMSK1_XFRCM_Pos)        // 0x00000001 */
#define DIEPEACHMSK1_XFRCM               DIEPEACHMSK1_XFRCM_Msk                   // Transfer completed interrupt mask                 */
#define DIEPEACHMSK1_EPDM_Pos            (1U)
#define DIEPEACHMSK1_EPDM_Msk            (0x1UL << DIEPEACHMSK1_EPDM_Pos)         // 0x00000002 */
#define DIEPEACHMSK1_EPDM                DIEPEACHMSK1_EPDM_Msk                    // Endpoint disabled interrupt mask                  */
#define DIEPEACHMSK1_TOM_Pos             (3U)
#define DIEPEACHMSK1_TOM_Msk             (0x1UL << DIEPEACHMSK1_TOM_Pos)          // 0x00000008 */
#define DIEPEACHMSK1_TOM                 DIEPEACHMSK1_TOM_Msk                     // Timeout condition mask (nonisochronous endpoints) */
#define DIEPEACHMSK1_ITTXFEMSK_Pos       (4U)
#define DIEPEACHMSK1_ITTXFEMSK_Msk       (0x1UL << DIEPEACHMSK1_ITTXFEMSK_Pos)    // 0x00000010 */
#define DIEPEACHMSK1_ITTXFEMSK           DIEPEACHMSK1_ITTXFEMSK_Msk               // IN token received when TxFIFO empty mask          */
#define DIEPEACHMSK1_INEPNMM_Pos         (5U)
#define DIEPEACHMSK1_INEPNMM_Msk         (0x1UL << DIEPEACHMSK1_INEPNMM_Pos)      // 0x00000020 */
#define DIEPEACHMSK1_INEPNMM             DIEPEACHMSK1_INEPNMM_Msk                 // IN token received with EP mismatch mask           */
#define DIEPEACHMSK1_INEPNEM_Pos         (6U)
#define DIEPEACHMSK1_INEPNEM_Msk         (0x1UL << DIEPEACHMSK1_INEPNEM_Pos)      // 0x00000040 */
#define DIEPEACHMSK1_INEPNEM             DIEPEACHMSK1_INEPNEM_Msk                 // IN endpoint NAK effective mask                    */
#define DIEPEACHMSK1_TXFURM_Pos          (8U)
#define DIEPEACHMSK1_TXFURM_Msk          (0x1UL << DIEPEACHMSK1_TXFURM_Pos)       // 0x00000100 */
#define DIEPEACHMSK1_TXFURM              DIEPEACHMSK1_TXFURM_Msk                  // FIFO underrun mask                                */
#define DIEPEACHMSK1_BIM_Pos             (9U)
#define DIEPEACHMSK1_BIM_Msk             (0x1UL << DIEPEACHMSK1_BIM_Pos)          // 0x00000200 */
#define DIEPEACHMSK1_BIM                 DIEPEACHMSK1_BIM_Msk                     // BNA interrupt mask                                */
#define DIEPEACHMSK1_NAKM_Pos            (13U)
#define DIEPEACHMSK1_NAKM_Msk            (0x1UL << DIEPEACHMSK1_NAKM_Pos)         // 0x00002000 */
#define DIEPEACHMSK1_NAKM                DIEPEACHMSK1_NAKM_Msk                    // NAK interrupt mask                                */

/********************  Bit definition for HPRT register  ********************/
#define HPRT_PCSTS_Pos                   (0U)
#define HPRT_PCSTS_Msk                   (0x1UL << HPRT_PCSTS_Pos)                // 0x00000001 */
#define HPRT_PCSTS                       HPRT_PCSTS_Msk                           // Port connect status        */
#define HPRT_PCDET_Pos                   (1U)
#define HPRT_PCDET_Msk                   (0x1UL << HPRT_PCDET_Pos)                // 0x00000002 */
#define HPRT_PCDET                       HPRT_PCDET_Msk                           // Port connect detected      */
#define HPRT_PENA_Pos                    (2U)
#define HPRT_PENA_Msk                    (0x1UL << HPRT_PENA_Pos)                 // 0x00000004 */
#define HPRT_PENA                        HPRT_PENA_Msk                            // Port enable                */
#define HPRT_PENCHNG_Pos                 (3U)
#define HPRT_PENCHNG_Msk                 (0x1UL << HPRT_PENCHNG_Pos)              // 0x00000008 */
#define HPRT_PENCHNG                     HPRT_PENCHNG_Msk                         // Port enable/disable change */
#define HPRT_POCA_Pos                    (4U)
#define HPRT_POCA_Msk                    (0x1UL << HPRT_POCA_Pos)                 // 0x00000010 */
#define HPRT_POCA                        HPRT_POCA_Msk                            // Port overcurrent active    */
#define HPRT_POCCHNG_Pos                 (5U)
#define HPRT_POCCHNG_Msk                 (0x1UL << HPRT_POCCHNG_Pos)              // 0x00000020 */
#define HPRT_POCCHNG                     HPRT_POCCHNG_Msk                         // Port overcurrent change    */
#define HPRT_PRES_Pos                    (6U)
#define HPRT_PRES_Msk                    (0x1UL << HPRT_PRES_Pos)                 // 0x00000040 */
#define HPRT_PRES                        HPRT_PRES_Msk                            // Port resume                */
#define HPRT_PSUSP_Pos                   (7U)
#define HPRT_PSUSP_Msk                   (0x1UL << HPRT_PSUSP_Pos)                // 0x00000080 */
#define HPRT_PSUSP                       HPRT_PSUSP_Msk                           // Port suspend               */
#define HPRT_PRST_Pos                    (8U)
#define HPRT_PRST_Msk                    (0x1UL << HPRT_PRST_Pos)                 // 0x00000100 */
#define HPRT_PRST                        HPRT_PRST_Msk                            // Port reset                 */

#define HPRT_PLSTS_Pos                   (10U)
#define HPRT_PLSTS_Msk                   (0x3UL << HPRT_PLSTS_Pos)                // 0x00000C00 */
#define HPRT_PLSTS                       HPRT_PLSTS_Msk                           // Port line status           */
#define HPRT_PLSTS_0                     (0x1UL << HPRT_PLSTS_Pos)                // 0x00000400 */
#define HPRT_PLSTS_1                     (0x2UL << HPRT_PLSTS_Pos)                // 0x00000800 */
#define HPRT_PPWR_Pos                    (12U)
#define HPRT_PPWR_Msk                    (0x1UL << HPRT_PPWR_Pos)                 // 0x00001000 */
#define HPRT_PPWR                        HPRT_PPWR_Msk                            // Port power                 */

#define HPRT_PTCTL_Pos                   (13U)
#define HPRT_PTCTL_Msk                   (0xFUL << HPRT_PTCTL_Pos)                // 0x0001E000 */
#define HPRT_PTCTL                       HPRT_PTCTL_Msk                           // Port test control          */
#define HPRT_PTCTL_0                     (0x1UL << HPRT_PTCTL_Pos)                // 0x00002000 */
#define HPRT_PTCTL_1                     (0x2UL << HPRT_PTCTL_Pos)                // 0x00004000 */
#define HPRT_PTCTL_2                     (0x4UL << HPRT_PTCTL_Pos)                // 0x00008000 */
#define HPRT_PTCTL_3                     (0x8UL << HPRT_PTCTL_Pos)                // 0x00010000 */

#define HPRT_PSPD_Pos                    (17U)
#define HPRT_PSPD_Msk                    (0x3UL << HPRT_PSPD_Pos)                 // 0x00060000 */
#define HPRT_PSPD                        HPRT_PSPD_Msk                            // Port speed                 */
#define HPRT_PSPD_0                      (0x1UL << HPRT_PSPD_Pos)                 // 0x00020000 */
#define HPRT_PSPD_1                      (0x2UL << HPRT_PSPD_Pos)                 // 0x00040000 */

/********************  Bit definition for DOEPEACHMSK1 register  ********************/
#define DOEPEACHMSK1_XFRCM_Pos           (0U)
#define DOEPEACHMSK1_XFRCM_Msk           (0x1UL << DOEPEACHMSK1_XFRCM_Pos)        // 0x00000001 */
#define DOEPEACHMSK1_XFRCM               DOEPEACHMSK1_XFRCM_Msk                   // Transfer completed interrupt mask         */
#define DOEPEACHMSK1_EPDM_Pos            (1U)
#define DOEPEACHMSK1_EPDM_Msk            (0x1UL << DOEPEACHMSK1_EPDM_Pos)         // 0x00000002 */
#define DOEPEACHMSK1_EPDM                DOEPEACHMSK1_EPDM_Msk                    // Endpoint disabled interrupt mask          */
#define DOEPEACHMSK1_TOM_Pos             (3U)
#define DOEPEACHMSK1_TOM_Msk             (0x1UL << DOEPEACHMSK1_TOM_Pos)          // 0x00000008 */
#define DOEPEACHMSK1_TOM                 DOEPEACHMSK1_TOM_Msk                     // Timeout condition mask                    */
#define DOEPEACHMSK1_ITTXFEMSK_Pos       (4U)
#define DOEPEACHMSK1_ITTXFEMSK_Msk       (0x1UL << DOEPEACHMSK1_ITTXFEMSK_Pos)    // 0x00000010 */
#define DOEPEACHMSK1_ITTXFEMSK           DOEPEACHMSK1_ITTXFEMSK_Msk               // IN token received when TxFIFO empty mask  */
#define DOEPEACHMSK1_INEPNMM_Pos         (5U)
#define DOEPEACHMSK1_INEPNMM_Msk         (0x1UL << DOEPEACHMSK1_INEPNMM_Pos)      // 0x00000020 */
#define DOEPEACHMSK1_INEPNMM             DOEPEACHMSK1_INEPNMM_Msk                 // IN token received with EP mismatch mask   */
#define DOEPEACHMSK1_INEPNEM_Pos         (6U)
#define DOEPEACHMSK1_INEPNEM_Msk         (0x1UL << DOEPEACHMSK1_INEPNEM_Pos)      // 0x00000040 */
#define DOEPEACHMSK1_INEPNEM             DOEPEACHMSK1_INEPNEM_Msk                 // IN endpoint NAK effective mask            */
#define DOEPEACHMSK1_TXFURM_Pos          (8U)
#define DOEPEACHMSK1_TXFURM_Msk          (0x1UL << DOEPEACHMSK1_TXFURM_Pos)       // 0x00000100 */
#define DOEPEACHMSK1_TXFURM              DOEPEACHMSK1_TXFURM_Msk                  // OUT packet error mask                     */
#define DOEPEACHMSK1_BIM_Pos             (9U)
#define DOEPEACHMSK1_BIM_Msk             (0x1UL << DOEPEACHMSK1_BIM_Pos)          // 0x00000200 */
#define DOEPEACHMSK1_BIM                 DOEPEACHMSK1_BIM_Msk                     // BNA interrupt mask                        */
#define DOEPEACHMSK1_BERRM_Pos           (12U)
#define DOEPEACHMSK1_BERRM_Msk           (0x1UL << DOEPEACHMSK1_BERRM_Pos)        // 0x00001000 */
#define DOEPEACHMSK1_BERRM               DOEPEACHMSK1_BERRM_Msk                   // Bubble error interrupt mask               */
#define DOEPEACHMSK1_NAKM_Pos            (13U)
#define DOEPEACHMSK1_NAKM_Msk            (0x1UL << DOEPEACHMSK1_NAKM_Pos)         // 0x00002000 */
#define DOEPEACHMSK1_NAKM                DOEPEACHMSK1_NAKM_Msk                    // NAK interrupt mask                        */
#define DOEPEACHMSK1_NYETM_Pos           (14U)
#define DOEPEACHMSK1_NYETM_Msk           (0x1UL << DOEPEACHMSK1_NYETM_Pos)        // 0x00004000 */
#define DOEPEACHMSK1_NYETM               DOEPEACHMSK1_NYETM_Msk                   // NYET interrupt mask                       */

/********************  Bit definition for HPTXFSIZ register  ********************/
#define HPTXFSIZ_PTXSA_Pos               (0U)
#define HPTXFSIZ_PTXSA_Msk               (0xFFFFUL << HPTXFSIZ_PTXSA_Pos)         // 0x0000FFFF */
#define HPTXFSIZ_PTXSA                   HPTXFSIZ_PTXSA_Msk                       // Host periodic TxFIFO start address            */
#define HPTXFSIZ_PTXFD_Pos               (16U)
#define HPTXFSIZ_PTXFD_Msk               (0xFFFFUL << HPTXFSIZ_PTXFD_Pos)         // 0xFFFF0000 */
#define HPTXFSIZ_PTXFD                   HPTXFSIZ_PTXFD_Msk                       // Host periodic TxFIFO depth                    */

/********************  Bit definition for DIEPCTL register  ********************/
#define DIEPCTL_MPSIZ_Pos                (0U)
#define DIEPCTL_MPSIZ_Msk                (0x7FFUL << DIEPCTL_MPSIZ_Pos)           // 0x000007FF */
#define DIEPCTL_MPSIZ                    DIEPCTL_MPSIZ_Msk                        // Maximum packet size              */
#define DIEPCTL_USBAEP_Pos               (15U)
#define DIEPCTL_USBAEP_Msk               (0x1UL << DIEPCTL_USBAEP_Pos)            // 0x00008000 */
#define DIEPCTL_USBAEP                   DIEPCTL_USBAEP_Msk                       // USB active endpoint              */
#define DIEPCTL_EONUM_DPID_Pos           (16U)
#define DIEPCTL_EONUM_DPID_Msk           (0x1UL << DIEPCTL_EONUM_DPID_Pos)        // 0x00010000 */
#define DIEPCTL_EONUM_DPID               DIEPCTL_EONUM_DPID_Msk                   // Even/odd frame                   */
#define DIEPCTL_NAKSTS_Pos               (17U)
#define DIEPCTL_NAKSTS_Msk               (0x1UL << DIEPCTL_NAKSTS_Pos)            // 0x00020000 */
#define DIEPCTL_NAKSTS                   DIEPCTL_NAKSTS_Msk                       // NAK status                       */

#define DIEPCTL_EPTYP_Pos                (18U)
#define DIEPCTL_EPTYP_Msk                (0x3UL << DIEPCTL_EPTYP_Pos)             // 0x000C0000 */
#define DIEPCTL_EPTYP                    DIEPCTL_EPTYP_Msk                        // Endpoint type                    */
#define DIEPCTL_EPTYP_0                  (0x1UL << DIEPCTL_EPTYP_Pos)             // 0x00040000 */
#define DIEPCTL_EPTYP_1                  (0x2UL << DIEPCTL_EPTYP_Pos)             // 0x00080000 */
#define DIEPCTL_STALL_Pos                (21U)
#define DIEPCTL_STALL_Msk                (0x1UL << DIEPCTL_STALL_Pos)             // 0x00200000 */
#define DIEPCTL_STALL                    DIEPCTL_STALL_Msk                        // STALL handshake                  */

#define DIEPCTL_TXFNUM_Pos               (22U)
#define DIEPCTL_TXFNUM_Msk               (0xFUL << DIEPCTL_TXFNUM_Pos)            // 0x03C00000 */
#define DIEPCTL_TXFNUM                   DIEPCTL_TXFNUM_Msk                       // TxFIFO number                    */
#define DIEPCTL_TXFNUM_0                 (0x1UL << DIEPCTL_TXFNUM_Pos)            // 0x00400000 */
#define DIEPCTL_TXFNUM_1                 (0x2UL << DIEPCTL_TXFNUM_Pos)            // 0x00800000 */
#define DIEPCTL_TXFNUM_2                 (0x4UL << DIEPCTL_TXFNUM_Pos)            // 0x01000000 */
#define DIEPCTL_TXFNUM_3                 (0x8UL << DIEPCTL_TXFNUM_Pos)            // 0x02000000 */
#define DIEPCTL_CNAK_Pos                 (26U)
#define DIEPCTL_CNAK_Msk                 (0x1UL << DIEPCTL_CNAK_Pos)              // 0x04000000 */
#define DIEPCTL_CNAK                     DIEPCTL_CNAK_Msk                         // Clear NAK                        */
#define DIEPCTL_SNAK_Pos                 (27U)
#define DIEPCTL_SNAK_Msk                 (0x1UL << DIEPCTL_SNAK_Pos)              // 0x08000000 */
#define DIEPCTL_SNAK                     DIEPCTL_SNAK_Msk                         // Set NAK */
#define DIEPCTL_SD0PID_SEVNFRM_Pos       (28U)
#define DIEPCTL_SD0PID_SEVNFRM_Msk       (0x1UL << DIEPCTL_SD0PID_SEVNFRM_Pos)    // 0x10000000 */
#define DIEPCTL_SD0PID_SEVNFRM           DIEPCTL_SD0PID_SEVNFRM_Msk               // Set DATA0 PID                    */
#define DIEPCTL_SODDFRM_Pos              (29U)
#define DIEPCTL_SODDFRM_Msk              (0x1UL << DIEPCTL_SODDFRM_Pos)           // 0x20000000 */
#define DIEPCTL_SODDFRM                  DIEPCTL_SODDFRM_Msk                      // Set odd frame                    */
#define DIEPCTL_EPDIS_Pos                (30U)
#define DIEPCTL_EPDIS_Msk                (0x1UL << DIEPCTL_EPDIS_Pos)             // 0x40000000 */
#define DIEPCTL_EPDIS                    DIEPCTL_EPDIS_Msk                        // Endpoint disable                 */
#define DIEPCTL_EPENA_Pos                (31U)
#define DIEPCTL_EPENA_Msk                (0x1UL << DIEPCTL_EPENA_Pos)             // 0x80000000 */
#define DIEPCTL_EPENA                    DIEPCTL_EPENA_Msk                        // Endpoint enable                  */

/********************  Bit definition for HCCHAR register  ********************/
#define HCCHAR_MPSIZ_Pos                 (0U)
#define HCCHAR_MPSIZ_Msk                 (0x7FFUL << HCCHAR_MPSIZ_Pos)            // 0x000007FF */
#define HCCHAR_MPSIZ                     HCCHAR_MPSIZ_Msk                         // Maximum packet size */

#define HCCHAR_EPNUM_Pos                 (11U)
#define HCCHAR_EPNUM_Msk                 (0xFUL << HCCHAR_EPNUM_Pos)              // 0x00007800 */
#define HCCHAR_EPNUM                     HCCHAR_EPNUM_Msk                         // Endpoint number */
#define HCCHAR_EPNUM_0                   (0x1UL << HCCHAR_EPNUM_Pos)              // 0x00000800 */
#define HCCHAR_EPNUM_1                   (0x2UL << HCCHAR_EPNUM_Pos)              // 0x00001000 */
#define HCCHAR_EPNUM_2                   (0x4UL << HCCHAR_EPNUM_Pos)              // 0x00002000 */
#define HCCHAR_EPNUM_3                   (0x8UL << HCCHAR_EPNUM_Pos)              // 0x00004000 */
#define HCCHAR_EPDIR_Pos                 (15U)
#define HCCHAR_EPDIR_Msk                 (0x1UL << HCCHAR_EPDIR_Pos)              // 0x00008000 */
#define HCCHAR_EPDIR                     HCCHAR_EPDIR_Msk                         // Endpoint direction */
#define HCCHAR_LSDEV_Pos                 (17U)
#define HCCHAR_LSDEV_Msk                 (0x1UL << HCCHAR_LSDEV_Pos)              // 0x00020000 */
#define HCCHAR_LSDEV                     HCCHAR_LSDEV_Msk                         // Low-speed device */

#define HCCHAR_EPTYP_Pos                 (18U)
#define HCCHAR_EPTYP_Msk                 (0x3UL << HCCHAR_EPTYP_Pos)              // 0x000C0000 */
#define HCCHAR_EPTYP                     HCCHAR_EPTYP_Msk                         // Endpoint type */
#define HCCHAR_EPTYP_0                   (0x1UL << HCCHAR_EPTYP_Pos)              // 0x00040000 */
#define HCCHAR_EPTYP_1                   (0x2UL << HCCHAR_EPTYP_Pos)              // 0x00080000 */

#define HCCHAR_MC_Pos                    (20U)
#define HCCHAR_MC_Msk                    (0x3UL << HCCHAR_MC_Pos)                 // 0x00300000 */
#define HCCHAR_MC                        HCCHAR_MC_Msk                            // Multi Count (MC) / Error Count (EC) */
#define HCCHAR_MC_0                      (0x1UL << HCCHAR_MC_Pos)                 // 0x00100000 */
#define HCCHAR_MC_1                      (0x2UL << HCCHAR_MC_Pos)                 // 0x00200000 */

#define HCCHAR_DAD_Pos                   (22U)
#define HCCHAR_DAD_Msk                   (0x7FUL << HCCHAR_DAD_Pos)               // 0x1FC00000 */
#define HCCHAR_DAD                       HCCHAR_DAD_Msk                           // Device address */
#define HCCHAR_DAD_0                     (0x01UL << HCCHAR_DAD_Pos)               // 0x00400000 */
#define HCCHAR_DAD_1                     (0x02UL << HCCHAR_DAD_Pos)               // 0x00800000 */
#define HCCHAR_DAD_2                     (0x04UL << HCCHAR_DAD_Pos)               // 0x01000000 */
#define HCCHAR_DAD_3                     (0x08UL << HCCHAR_DAD_Pos)               // 0x02000000 */
#define HCCHAR_DAD_4                     (0x10UL << HCCHAR_DAD_Pos)               // 0x04000000 */
#define HCCHAR_DAD_5                     (0x20UL << HCCHAR_DAD_Pos)               // 0x08000000 */
#define HCCHAR_DAD_6                     (0x40UL << HCCHAR_DAD_Pos)               // 0x10000000 */
#define HCCHAR_ODDFRM_Pos                (29U)
#define HCCHAR_ODDFRM_Msk                (0x1UL << HCCHAR_ODDFRM_Pos)             // 0x20000000 */
#define HCCHAR_ODDFRM                    HCCHAR_ODDFRM_Msk                        // Odd frame */
#define HCCHAR_CHDIS_Pos                 (30U)
#define HCCHAR_CHDIS_Msk                 (0x1UL << HCCHAR_CHDIS_Pos)              // 0x40000000 */
#define HCCHAR_CHDIS                     HCCHAR_CHDIS_Msk                         // Channel disable */
#define HCCHAR_CHENA_Pos                 (31U)
#define HCCHAR_CHENA_Msk                 (0x1UL << HCCHAR_CHENA_Pos)              // 0x80000000 */
#define HCCHAR_CHENA                     HCCHAR_CHENA_Msk                         // Channel enable */

/********************  Bit definition for HCSPLT register  ********************/

#define HCSPLT_PRTADDR_Pos               (0U)
#define HCSPLT_PRTADDR_Msk               (0x7FUL << HCSPLT_PRTADDR_Pos)           // 0x0000007F */
#define HCSPLT_PRTADDR                   HCSPLT_PRTADDR_Msk                       // Port address */
#define HCSPLT_PRTADDR_0                 (0x01UL << HCSPLT_PRTADDR_Pos)           // 0x00000001 */
#define HCSPLT_PRTADDR_1                 (0x02UL << HCSPLT_PRTADDR_Pos)           // 0x00000002 */
#define HCSPLT_PRTADDR_2                 (0x04UL << HCSPLT_PRTADDR_Pos)           // 0x00000004 */
#define HCSPLT_PRTADDR_3                 (0x08UL << HCSPLT_PRTADDR_Pos)           // 0x00000008 */
#define HCSPLT_PRTADDR_4                 (0x10UL << HCSPLT_PRTADDR_Pos)           // 0x00000010 */
#define HCSPLT_PRTADDR_5                 (0x20UL << HCSPLT_PRTADDR_Pos)           // 0x00000020 */
#define HCSPLT_PRTADDR_6                 (0x40UL << HCSPLT_PRTADDR_Pos)           // 0x00000040 */

#define HCSPLT_HUBADDR_Pos               (7U)
#define HCSPLT_HUBADDR_Msk               (0x7FUL << HCSPLT_HUBADDR_Pos)           // 0x00003F80 */
#define HCSPLT_HUBADDR                   HCSPLT_HUBADDR_Msk                       // Hub address */
#define HCSPLT_HUBADDR_0                 (0x01UL << HCSPLT_HUBADDR_Pos)           // 0x00000080 */
#define HCSPLT_HUBADDR_1                 (0x02UL << HCSPLT_HUBADDR_Pos)           // 0x00000100 */
#define HCSPLT_HUBADDR_2                 (0x04UL << HCSPLT_HUBADDR_Pos)           // 0x00000200 */
#define HCSPLT_HUBADDR_3                 (0x08UL << HCSPLT_HUBADDR_Pos)           // 0x00000400 */
#define HCSPLT_HUBADDR_4                 (0x10UL << HCSPLT_HUBADDR_Pos)           // 0x00000800 */
#define HCSPLT_HUBADDR_5                 (0x20UL << HCSPLT_HUBADDR_Pos)           // 0x00001000 */
#define HCSPLT_HUBADDR_6                 (0x40UL << HCSPLT_HUBADDR_Pos)           // 0x00002000 */

#define HCSPLT_XACTPOS_Pos               (14U)
#define HCSPLT_XACTPOS_Msk               (0x3UL << HCSPLT_XACTPOS_Pos)            // 0x0000C000 */
#define HCSPLT_XACTPOS                   HCSPLT_XACTPOS_Msk                       // XACTPOS */
#define HCSPLT_XACTPOS_0                 (0x1UL << HCSPLT_XACTPOS_Pos)            // 0x00004000 */
#define HCSPLT_XACTPOS_1                 (0x2UL << HCSPLT_XACTPOS_Pos)            // 0x00008000 */
#define HCSPLT_COMPLSPLT_Pos             (16U)
#define HCSPLT_COMPLSPLT_Msk             (0x1UL << HCSPLT_COMPLSPLT_Pos)          // 0x00010000 */
#define HCSPLT_COMPLSPLT                 HCSPLT_COMPLSPLT_Msk                     // Do complete split */
#define HCSPLT_SPLITEN_Pos               (31U)
#define HCSPLT_SPLITEN_Msk               (0x1UL << HCSPLT_SPLITEN_Pos)            // 0x80000000 */
#define HCSPLT_SPLITEN                   HCSPLT_SPLITEN_Msk                       // Split enable */

/********************  Bit definition for HCINT register  ********************/
#define HCINT_XFRC_Pos                   (0U)
#define HCINT_XFRC_Msk                   (0x1UL << HCINT_XFRC_Pos)                // 0x00000001 */
#define HCINT_XFRC                       HCINT_XFRC_Msk                           // Transfer completed */
#define HCINT_CHH_Pos                    (1U)
#define HCINT_CHH_Msk                    (0x1UL << HCINT_CHH_Pos)                 // 0x00000002 */
#define HCINT_CHH                        HCINT_CHH_Msk                            // Channel halted */
#define HCINT_AHBERR_Pos                 (2U)
#define HCINT_AHBERR_Msk                 (0x1UL << HCINT_AHBERR_Pos)              // 0x00000004 */
#define HCINT_AHBERR                     HCINT_AHBERR_Msk                         // AHB error */
#define HCINT_STALL_Pos                  (3U)
#define HCINT_STALL_Msk                  (0x1UL << HCINT_STALL_Pos)               // 0x00000008 */
#define HCINT_STALL                      HCINT_STALL_Msk                          // STALL response received interrupt */
#define HCINT_NAK_Pos                    (4U)
#define HCINT_NAK_Msk                    (0x1UL << HCINT_NAK_Pos)                 // 0x00000010 */
#define HCINT_NAK                        HCINT_NAK_Msk                            // NAK response received interrupt */
#define HCINT_ACK_Pos                    (5U)
#define HCINT_ACK_Msk                    (0x1UL << HCINT_ACK_Pos)                 // 0x00000020 */
#define HCINT_ACK                        HCINT_ACK_Msk                            // ACK response received/transmitted interrupt */
#define HCINT_NYET_Pos                   (6U)
#define HCINT_NYET_Msk                   (0x1UL << HCINT_NYET_Pos)                // 0x00000040 */
#define HCINT_NYET                       HCINT_NYET_Msk                           // Response received interrupt */
#define HCINT_TXERR_Pos                  (7U)
#define HCINT_TXERR_Msk                  (0x1UL << HCINT_TXERR_Pos)               // 0x00000080 */
#define HCINT_TXERR                      HCINT_TXERR_Msk                          // Transaction error */
#define HCINT_BBERR_Pos                  (8U)
#define HCINT_BBERR_Msk                  (0x1UL << HCINT_BBERR_Pos)               // 0x00000100 */
#define HCINT_BBERR                      HCINT_BBERR_Msk                          // Babble error */
#define HCINT_FRMOR_Pos                  (9U)
#define HCINT_FRMOR_Msk                  (0x1UL << HCINT_FRMOR_Pos)               // 0x00000200 */
#define HCINT_FRMOR                      HCINT_FRMOR_Msk                          // Frame overrun */
#define HCINT_DTERR_Pos                  (10U)
#define HCINT_DTERR_Msk                  (0x1UL << HCINT_DTERR_Pos)               // 0x00000400 */
#define HCINT_DTERR                      HCINT_DTERR_Msk                          // Data toggle error */

/********************  Bit definition for DIEPINT register  ********************/
#define DIEPINT_XFRC_Pos                 (0U)
#define DIEPINT_XFRC_Msk                 (0x1UL << DIEPINT_XFRC_Pos)              // 0x00000001 */
#define DIEPINT_XFRC                     DIEPINT_XFRC_Msk                         // Transfer completed interrupt */
#define DIEPINT_EPDISD_Pos               (1U)
#define DIEPINT_EPDISD_Msk               (0x1UL << DIEPINT_EPDISD_Pos)            // 0x00000002 */
#define DIEPINT_EPDISD                   DIEPINT_EPDISD_Msk                       // Endpoint disabled interrupt */
#define DIEPINT_AHBERR_Pos               (2U)
#define DIEPINT_AHBERR_Msk               (0x1UL << DIEPINT_AHBERR_Pos)            // 0x00000004 */
#define DIEPINT_AHBERR                   DIEPINT_AHBERR_Msk                       // AHB Error (AHBErr) during an IN transaction */
#define DIEPINT_TOC_Pos                  (3U)
#define DIEPINT_TOC_Msk                  (0x1UL << DIEPINT_TOC_Pos)               // 0x00000008 */
#define DIEPINT_TOC                      DIEPINT_TOC_Msk                          // Timeout condition */
#define DIEPINT_ITTXFE_Pos               (4U)
#define DIEPINT_ITTXFE_Msk               (0x1UL << DIEPINT_ITTXFE_Pos)            // 0x00000010 */
#define DIEPINT_ITTXFE                   DIEPINT_ITTXFE_Msk                       // IN token received when TxFIFO is empty */
#define DIEPINT_INEPNM_Pos               (5U)
#define DIEPINT_INEPNM_Msk               (0x1UL << DIEPINT_INEPNM_Pos)            // 0x00000020 */
#define DIEPINT_INEPNM                   DIEPINT_INEPNM_Msk                       // IN token received with EP mismatch */
#define DIEPINT_INEPNE_Pos               (6U)
#define DIEPINT_INEPNE_Msk               (0x1UL << DIEPINT_INEPNE_Pos)            // 0x00000040 */
#define DIEPINT_INEPNE                   DIEPINT_INEPNE_Msk                       // IN endpoint NAK effective */
#define DIEPINT_TXFE_Pos                 (7U)
#define DIEPINT_TXFE_Msk                 (0x1UL << DIEPINT_TXFE_Pos)              // 0x00000080 */
#define DIEPINT_TXFE                     DIEPINT_TXFE_Msk                         // Transmit FIFO empty */
#define DIEPINT_TXFIFOUDRN_Pos           (8U)
#define DIEPINT_TXFIFOUDRN_Msk           (0x1UL << DIEPINT_TXFIFOUDRN_Pos)        // 0x00000100 */
#define DIEPINT_TXFIFOUDRN               DIEPINT_TXFIFOUDRN_Msk                   // Transmit Fifo Underrun */
#define DIEPINT_BNA_Pos                  (9U)
#define DIEPINT_BNA_Msk                  (0x1UL << DIEPINT_BNA_Pos)               // 0x00000200 */
#define DIEPINT_BNA                      DIEPINT_BNA_Msk                          // Buffer not available interrupt */
#define DIEPINT_PKTDRPSTS_Pos            (11U)
#define DIEPINT_PKTDRPSTS_Msk            (0x1UL << DIEPINT_PKTDRPSTS_Pos)         // 0x00000800 */
#define DIEPINT_PKTDRPSTS                DIEPINT_PKTDRPSTS_Msk                    // Packet dropped status */
#define DIEPINT_BERR_Pos                 (12U)
#define DIEPINT_BERR_Msk                 (0x1UL << DIEPINT_BERR_Pos)              // 0x00001000 */
#define DIEPINT_BERR                     DIEPINT_BERR_Msk                         // Babble error interrupt */
#define DIEPINT_NAK_Pos                  (13U)
#define DIEPINT_NAK_Msk                  (0x1UL << DIEPINT_NAK_Pos)               // 0x00002000 */
#define DIEPINT_NAK                      DIEPINT_NAK_Msk                          // NAK interrupt */

/********************  Bit definition for HCINTMSK register  ********************/
#define HCINTMSK_XFRCM_Pos               (0U)
#define HCINTMSK_XFRCM_Msk               (0x1UL << HCINTMSK_XFRCM_Pos)            // 0x00000001 */
#define HCINTMSK_XFRCM                   HCINTMSK_XFRCM_Msk                       // Transfer completed mask */
#define HCINTMSK_CHHM_Pos                (1U)
#define HCINTMSK_CHHM_Msk                (0x1UL << HCINTMSK_CHHM_Pos)             // 0x00000002 */
#define HCINTMSK_CHHM                    HCINTMSK_CHHM_Msk                        // Channel halted mask */
#define HCINTMSK_AHBERR_Pos              (2U)
#define HCINTMSK_AHBERR_Msk              (0x1UL << HCINTMSK_AHBERR_Pos)           // 0x00000004 */
#define HCINTMSK_AHBERR                  HCINTMSK_AHBERR_Msk                      // AHB error */
#define HCINTMSK_STALLM_Pos              (3U)
#define HCINTMSK_STALLM_Msk              (0x1UL << HCINTMSK_STALLM_Pos)           // 0x00000008 */
#define HCINTMSK_STALLM                  HCINTMSK_STALLM_Msk                      // STALL response received interrupt mask */
#define HCINTMSK_NAKM_Pos                (4U)
#define HCINTMSK_NAKM_Msk                (0x1UL << HCINTMSK_NAKM_Pos)             // 0x00000010 */
#define HCINTMSK_NAKM                    HCINTMSK_NAKM_Msk                        // NAK response received interrupt mask */
#define HCINTMSK_ACKM_Pos                (5U)
#define HCINTMSK_ACKM_Msk                (0x1UL << HCINTMSK_ACKM_Pos)             // 0x00000020 */
#define HCINTMSK_ACKM                    HCINTMSK_ACKM_Msk                        // ACK response received/transmitted interrupt mask */
#define HCINTMSK_NYET_Pos                (6U)
#define HCINTMSK_NYET_Msk                (0x1UL << HCINTMSK_NYET_Pos)             // 0x00000040 */
#define HCINTMSK_NYET                    HCINTMSK_NYET_Msk                        // response received interrupt mask */
#define HCINTMSK_TXERRM_Pos              (7U)
#define HCINTMSK_TXERRM_Msk              (0x1UL << HCINTMSK_TXERRM_Pos)           // 0x00000080 */
#define HCINTMSK_TXERRM                  HCINTMSK_TXERRM_Msk                      // Transaction error mask */
#define HCINTMSK_BBERRM_Pos              (8U)
#define HCINTMSK_BBERRM_Msk              (0x1UL << HCINTMSK_BBERRM_Pos)           // 0x00000100 */
#define HCINTMSK_BBERRM                  HCINTMSK_BBERRM_Msk                      // Babble error mask */
#define HCINTMSK_FRMORM_Pos              (9U)
#define HCINTMSK_FRMORM_Msk              (0x1UL << HCINTMSK_FRMORM_Pos)           // 0x00000200 */
#define HCINTMSK_FRMORM                  HCINTMSK_FRMORM_Msk                      // Frame overrun mask */
#define HCINTMSK_DTERRM_Pos              (10U)
#define HCINTMSK_DTERRM_Msk              (0x1UL << HCINTMSK_DTERRM_Pos)           // 0x00000400 */
#define HCINTMSK_DTERRM                  HCINTMSK_DTERRM_Msk                      // Data toggle error mask */

/********************  Bit definition for DIEPTSIZ register  ********************/

#define DIEPTSIZ_XFRSIZ_Pos              (0U)
#define DIEPTSIZ_XFRSIZ_Msk              (0x7FFFFUL << DIEPTSIZ_XFRSIZ_Pos)       // 0x0007FFFF */
#define DIEPTSIZ_XFRSIZ                  DIEPTSIZ_XFRSIZ_Msk                      // Transfer size */
#define DIEPTSIZ_PKTCNT_Pos              (19U)
#define DIEPTSIZ_PKTCNT_Msk              (0x3FFUL << DIEPTSIZ_PKTCNT_Pos)         // 0x1FF80000 */
#define DIEPTSIZ_PKTCNT                  DIEPTSIZ_PKTCNT_Msk                      // Packet count */
#define DIEPTSIZ_MULCNT_Pos              (29U)
#define DIEPTSIZ_MULCNT_Msk              (0x3UL << DIEPTSIZ_MULCNT_Pos)           // 0x60000000 */
#define DIEPTSIZ_MULCNT                  DIEPTSIZ_MULCNT_Msk                      // Packet count */
                                                                                  /********************  Bit definition for HCTSIZ register  ********************/
#define HCTSIZ_XFRSIZ_Pos                (0U)
#define HCTSIZ_XFRSIZ_Msk                (0x7FFFFUL << HCTSIZ_XFRSIZ_Pos)         // 0x0007FFFF */
#define HCTSIZ_XFRSIZ                    HCTSIZ_XFRSIZ_Msk                        // Transfer size */
#define HCTSIZ_PKTCNT_Pos                (19U)
#define HCTSIZ_PKTCNT_Msk                (0x3FFUL << HCTSIZ_PKTCNT_Pos)           // 0x1FF80000 */
#define HCTSIZ_PKTCNT                    HCTSIZ_PKTCNT_Msk                        // Packet count */
#define HCTSIZ_DOPING_Pos                (31U)
#define HCTSIZ_DOPING_Msk                (0x1UL << HCTSIZ_DOPING_Pos)             // 0x80000000 */
#define HCTSIZ_DOPING                    HCTSIZ_DOPING_Msk                        // Do PING */
#define HCTSIZ_DPID_Pos                  (29U)
#define HCTSIZ_DPID_Msk                  (0x3UL << HCTSIZ_DPID_Pos)               // 0x60000000 */
#define HCTSIZ_DPID                      HCTSIZ_DPID_Msk                          // Data PID */
#define HCTSIZ_DPID_0                    (0x1UL << HCTSIZ_DPID_Pos)               // 0x20000000 */
#define HCTSIZ_DPID_1                    (0x2UL << HCTSIZ_DPID_Pos)               // 0x40000000 */

/********************  Bit definition for DIEPDMA register  ********************/
#define DIEPDMA_DMAADDR_Pos              (0U)
#define DIEPDMA_DMAADDR_Msk              (0xFFFFFFFFUL << DIEPDMA_DMAADDR_Pos)    // 0xFFFFFFFF */
#define DIEPDMA_DMAADDR                  DIEPDMA_DMAADDR_Msk                      // DMA address */

/********************  Bit definition for HCDMA register  ********************/
#define HCDMA_DMAADDR_Pos                (0U)
#define HCDMA_DMAADDR_Msk                (0xFFFFFFFFUL << HCDMA_DMAADDR_Pos)      // 0xFFFFFFFF */
#define HCDMA_DMAADDR                    HCDMA_DMAADDR_Msk                        // DMA address */

                                                                                  /********************  Bit definition for DTXFSTS register  ********************/
#define DTXFSTS_INEPTFSAV_Pos            (0U)
#define DTXFSTS_INEPTFSAV_Msk            (0xFFFFUL << DTXFSTS_INEPTFSAV_Pos)      // 0x0000FFFF */
#define DTXFSTS_INEPTFSAV                DTXFSTS_INEPTFSAV_Msk                    // IN endpoint TxFIFO space available */

                                                                                  /********************  Bit definition for DIEPTXF register  ********************/
#define DIEPTXF_INEPTXSA_Pos             (0U)
#define DIEPTXF_INEPTXSA_Msk             (0xFFFFUL << DIEPTXF_INEPTXSA_Pos)       // 0x0000FFFF */
#define DIEPTXF_INEPTXSA                 DIEPTXF_INEPTXSA_Msk                     // IN endpoint FIFOx transmit RAM start address */
#define DIEPTXF_INEPTXFD_Pos             (16U)
#define DIEPTXF_INEPTXFD_Msk             (0xFFFFUL << DIEPTXF_INEPTXFD_Pos)       // 0xFFFF0000 */
#define DIEPTXF_INEPTXFD                 DIEPTXF_INEPTXFD_Msk                     // IN endpoint TxFIFO depth */

/********************  Bit definition for DOEPCTL register  ********************/
#define DOEPCTL_MPSIZ_Pos                (0U)
#define DOEPCTL_MPSIZ_Msk                (0x7FFUL << DOEPCTL_MPSIZ_Pos)           // 0x000007FF */
#define DOEPCTL_MPSIZ                    DOEPCTL_MPSIZ_Msk                        // Maximum packet size */          //Bit 1 */
#define DOEPCTL_USBAEP_Pos               (15U)
#define DOEPCTL_USBAEP_Msk               (0x1UL << DOEPCTL_USBAEP_Pos)            // 0x00008000 */
#define DOEPCTL_USBAEP                   DOEPCTL_USBAEP_Msk                       // USB active endpoint */
#define DOEPCTL_NAKSTS_Pos               (17U)
#define DOEPCTL_NAKSTS_Msk               (0x1UL << DOEPCTL_NAKSTS_Pos)            // 0x00020000 */
#define DOEPCTL_NAKSTS                   DOEPCTL_NAKSTS_Msk                       // NAK status */
#define DOEPCTL_SD0PID_SEVNFRM_Pos       (28U)
#define DOEPCTL_SD0PID_SEVNFRM_Msk       (0x1UL << DOEPCTL_SD0PID_SEVNFRM_Pos)    // 0x10000000 */
#define DOEPCTL_SD0PID_SEVNFRM           DOEPCTL_SD0PID_SEVNFRM_Msk               // Set DATA0 PID */
#define DOEPCTL_SODDFRM_Pos              (29U)
#define DOEPCTL_SODDFRM_Msk              (0x1UL << DOEPCTL_SODDFRM_Pos)           // 0x20000000 */
#define DOEPCTL_SODDFRM                  DOEPCTL_SODDFRM_Msk                      // Set odd frame */
#define DOEPCTL_EPTYP_Pos                (18U)
#define DOEPCTL_EPTYP_Msk                (0x3UL << DOEPCTL_EPTYP_Pos)             // 0x000C0000 */
#define DOEPCTL_EPTYP                    DOEPCTL_EPTYP_Msk                        // Endpoint type */
#define DOEPCTL_EPTYP_0                  (0x1UL << DOEPCTL_EPTYP_Pos)             // 0x00040000 */
#define DOEPCTL_EPTYP_1                  (0x2UL << DOEPCTL_EPTYP_Pos)             // 0x00080000 */
#define DOEPCTL_SNPM_Pos                 (20U)
#define DOEPCTL_SNPM_Msk                 (0x1UL << DOEPCTL_SNPM_Pos)              // 0x00100000 */
#define DOEPCTL_SNPM                     DOEPCTL_SNPM_Msk                         // Snoop mode */
#define DOEPCTL_STALL_Pos                (21U)
#define DOEPCTL_STALL_Msk                (0x1UL << DOEPCTL_STALL_Pos)             // 0x00200000 */
#define DOEPCTL_STALL                    DOEPCTL_STALL_Msk                        // STALL handshake */
#define DOEPCTL_CNAK_Pos                 (26U)
#define DOEPCTL_CNAK_Msk                 (0x1UL << DOEPCTL_CNAK_Pos)              // 0x04000000 */
#define DOEPCTL_CNAK                     DOEPCTL_CNAK_Msk                         // Clear NAK */
#define DOEPCTL_SNAK_Pos                 (27U)
#define DOEPCTL_SNAK_Msk                 (0x1UL << DOEPCTL_SNAK_Pos)              // 0x08000000 */
#define DOEPCTL_SNAK                     DOEPCTL_SNAK_Msk                         // Set NAK */
#define DOEPCTL_EPDIS_Pos                (30U)
#define DOEPCTL_EPDIS_Msk                (0x1UL << DOEPCTL_EPDIS_Pos)             // 0x40000000 */
#define DOEPCTL_EPDIS                    DOEPCTL_EPDIS_Msk                        // Endpoint disable */
#define DOEPCTL_EPENA_Pos                (31U)
#define DOEPCTL_EPENA_Msk                (0x1UL << DOEPCTL_EPENA_Pos)             // 0x80000000 */
#define DOEPCTL_EPENA                    DOEPCTL_EPENA_Msk                        // Endpoint enable */

/********************  Bit definition for DOEPINT register  ********************/
#define DOEPINT_XFRC_Pos                 (0U)
#define DOEPINT_XFRC_Msk                 (0x1UL << DOEPINT_XFRC_Pos)              // 0x00000001 */
#define DOEPINT_XFRC                     DOEPINT_XFRC_Msk                         // Transfer completed interrupt */
#define DOEPINT_EPDISD_Pos               (1U)
#define DOEPINT_EPDISD_Msk               (0x1UL << DOEPINT_EPDISD_Pos)            // 0x00000002 */
#define DOEPINT_EPDISD                   DOEPINT_EPDISD_Msk                       // Endpoint disabled interrupt */
#define DOEPINT_AHBERR_Pos               (2U)
#define DOEPINT_AHBERR_Msk               (0x1UL << DOEPINT_AHBERR_Pos)            // 0x00000004 */
#define DOEPINT_AHBERR                   DOEPINT_AHBERR_Msk                       // AHB Error (AHBErr) during an OUT transaction */
#define DOEPINT_STUP_Pos                 (3U)
#define DOEPINT_STUP_Msk                 (0x1UL << DOEPINT_STUP_Pos)              // 0x00000008 */
#define DOEPINT_STUP                     DOEPINT_STUP_Msk                         // SETUP phase done */
#define DOEPINT_OTEPDIS_Pos              (4U)
#define DOEPINT_OTEPDIS_Msk              (0x1UL << DOEPINT_OTEPDIS_Pos)           // 0x00000010 */
#define DOEPINT_OTEPDIS                  DOEPINT_OTEPDIS_Msk                      // OUT token received when endpoint disabled */
#define DOEPINT_OTEPSPR_Pos              (5U)
#define DOEPINT_OTEPSPR_Msk              (0x1UL << DOEPINT_OTEPSPR_Pos)           // 0x00000020 */
#define DOEPINT_OTEPSPR                  DOEPINT_OTEPSPR_Msk                      // Status Phase Received For Control Write */
#define DOEPINT_B2BSTUP_Pos              (6U)
#define DOEPINT_B2BSTUP_Msk              (0x1UL << DOEPINT_B2BSTUP_Pos)           // 0x00000040 */
#define DOEPINT_B2BSTUP                  DOEPINT_B2BSTUP_Msk                      // Back-to-back SETUP packets received */
#define DOEPINT_OUTPKTERR_Pos            (8U)
#define DOEPINT_OUTPKTERR_Msk            (0x1UL << DOEPINT_OUTPKTERR_Pos)         // 0x00000100 */
#define DOEPINT_OUTPKTERR                DOEPINT_OUTPKTERR_Msk                    // OUT packet error */
#define DOEPINT_NAK_Pos                  (13U)
#define DOEPINT_NAK_Msk                  (0x1UL << DOEPINT_NAK_Pos)               // 0x00002000 */
#define DOEPINT_NAK                      DOEPINT_NAK_Msk                          // NAK Packet is transmitted by the device */
#define DOEPINT_NYET_Pos                 (14U)
#define DOEPINT_NYET_Msk                 (0x1UL << DOEPINT_NYET_Pos)              // 0x00004000 */
#define DOEPINT_NYET                     DOEPINT_NYET_Msk                         // NYET interrupt */
#define DOEPINT_STPKTRX_Pos              (15U)
#define DOEPINT_STPKTRX_Msk              (0x1UL << DOEPINT_STPKTRX_Pos)           // 0x00008000 */
#define DOEPINT_STPKTRX                  DOEPINT_STPKTRX_Msk                      // Setup Packet Received */

/********************  Bit definition for DOEPTSIZ register  ********************/
#define DOEPTSIZ_XFRSIZ_Pos              (0U)
#define DOEPTSIZ_XFRSIZ_Msk              (0x7FFFFUL << DOEPTSIZ_XFRSIZ_Pos)       // 0x0007FFFF */
#define DOEPTSIZ_XFRSIZ                  DOEPTSIZ_XFRSIZ_Msk                      // Transfer size */
#define DOEPTSIZ_PKTCNT_Pos              (19U)
#define DOEPTSIZ_PKTCNT_Msk              (0x3FFUL << DOEPTSIZ_PKTCNT_Pos)         // 0x1FF80000 */
#define DOEPTSIZ_PKTCNT                  DOEPTSIZ_PKTCNT_Msk                      // Packet count */

#define DOEPTSIZ_STUPCNT_Pos             (29U)
#define DOEPTSIZ_STUPCNT_Msk             (0x3UL << DOEPTSIZ_STUPCNT_Pos)          // 0x60000000 */
#define DOEPTSIZ_STUPCNT                 DOEPTSIZ_STUPCNT_Msk                     // SETUP packet count */
#define DOEPTSIZ_STUPCNT_0               (0x1UL << DOEPTSIZ_STUPCNT_Pos)          // 0x20000000 */
#define DOEPTSIZ_STUPCNT_1               (0x2UL << DOEPTSIZ_STUPCNT_Pos)          // 0x40000000 */

/********************  Bit definition for PCGCCTL register  ********************/
#define PCGCCTL_STOPCLK_Pos              (0U)
#define PCGCCTL_STOPCLK_Msk              (0x1UL << PCGCCTL_STOPCLK_Pos)           // 0x00000001 */
#define PCGCCTL_STOPCLK                  PCGCCTL_STOPCLK_Msk                      // SETUP packet count */
#define PCGCCTL_GATECLK_Pos              (1U)
#define PCGCCTL_GATECLK_Msk              (0x1UL << PCGCCTL_GATECLK_Pos)           // 0x00000002 */
#define PCGCCTL_GATECLK                  PCGCCTL_GATECLK_Msk                      //Bit 0 */
#define PCGCCTL_PHYSUSP_Pos              (4U)
#define PCGCCTL_PHYSUSP_Msk              (0x1UL << PCGCCTL_PHYSUSP_Pos)           // 0x00000010 */
#define PCGCCTL_PHYSUSP                  PCGCCTL_PHYSUSP_Msk                      //Bit 1 */

/********************  Bit definition for USBPHYC_PLL1 register  ********************/
#define HS_PHYC_PLL1_PLLEN_Pos                (0U)
#define HS_PHYC_PLL1_PLLEN_Msk                (0x1UL << HS_PHYC_PLL1_PLLEN_Pos)   // 0x00000001 */
#define HS_PHYC_PLL1_PLLEN                    HS_PHYC_PLL1_PLLEN_Msk              // Enable PLL */
#define HS_PHYC_PLL1_PLLSEL_Pos               (1U)
#define HS_PHYC_PLL1_PLLSEL_Msk               (0x7UL << HS_PHYC_PLL1_PLLSEL_Pos)  // 0x0000000E */
#define HS_PHYC_PLL1_PLLSEL                   HS_PHYC_PLL1_PLLSEL_Msk             // Controls PHY frequency operation selection */
#define HS_PHYC_PLL1_PLLSEL_1                 (0x1UL << HS_PHYC_PLL1_PLLSEL_Pos)  // 0x00000002 */
#define HS_PHYC_PLL1_PLLSEL_2                 (0x2UL << HS_PHYC_PLL1_PLLSEL_Pos)  // 0x00000004 */
#define HS_PHYC_PLL1_PLLSEL_3                 (0x4UL << HS_PHYC_PLL1_PLLSEL_Pos)  // 0x00000008 */

#define HS_PHYC_PLL1_PLLSEL_12MHZ             0x00000000U                                                       // PHY PLL1 input clock frequency 12 MHz   */
#define HS_PHYC_PLL1_PLLSEL_12_5MHZ           HS_PHYC_PLL1_PLLSEL_1                                         // PHY PLL1 input clock frequency 12.5 MHz */
#define HS_PHYC_PLL1_PLLSEL_16MHZ             (uint32_t)(HS_PHYC_PLL1_PLLSEL_1 | HS_PHYC_PLL1_PLLSEL_2) // PHY PLL1 input clock frequency 16 MHz   */
#define HS_PHYC_PLL1_PLLSEL_24MHZ             HS_PHYC_PLL1_PLLSEL_3                                         // PHY PLL1 input clock frequency 24 MHz   */
#define HS_PHYC_PLL1_PLLSEL_25MHZ             (uint32_t)(HS_PHYC_PLL1_PLLSEL_2 | HS_PHYC_PLL1_PLLSEL_3) // PHY PLL1 input clock frequency 25 MHz   */

/********************  Bit definition for USBPHYC_LDO register  ********************/
#define HS_PHYC_LDO_USED_Pos                 (0U)
#define HS_PHYC_LDO_USED_Msk                 (0x1UL << HS_PHYC_LDO_USED_Pos) // 0x00000001 */
#define HS_PHYC_LDO_USED                     HS_PHYC_LDO_USED_Msk      // Monitors the usage status of the PHY's LDO   */
#define HS_PHYC_LDO_STATUS_Pos               (1U)
#define HS_PHYC_LDO_STATUS_Msk               (0x1UL << HS_PHYC_LDO_STATUS_Pos) // 0x00000002 */
#define HS_PHYC_LDO_STATUS                   HS_PHYC_LDO_STATUS_Msk    // Monitors the status of the PHY's LDO.        */
#define HS_PHYC_LDO_DISABLE_Pos              (2U)
#define HS_PHYC_LDO_DISABLE_Msk              (0x1UL << HS_PHYC_LDO_DISABLE_Pos) // 0x00000004 */
#define HS_PHYC_LDO_DISABLE                  HS_PHYC_LDO_DISABLE_Msk    // Controls disable of the High Speed PHY's LDO */

#define HS_PHYC_LDO_ENABLE_Pos               HS_PHYC_LDO_DISABLE_Pos
#define HS_PHYC_LDO_ENABLE_Msk               HS_PHYC_LDO_DISABLE_Msk
#define HS_PHYC_LDO_ENABLE                   HS_PHYC_LDO_DISABLE

#ifdef __cplusplus
 }
#endif

#endif
