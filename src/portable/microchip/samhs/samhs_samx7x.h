 /*
* The MIT License (MIT)
*
* Copyright (c) 2019 Microchip Technology Inc.
* Copyright (c) 2018, hathach (tinyusb.org)
* Copyright (c) 2021, HiFiPhile
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

#ifndef _SAMHS_SAMX7X_H_
#define _SAMHS_SAMX7X_H_

/* -------- SAMHS_DEV_DMANXTDSC : (USBHS Offset: 0x00) (R/W 32) Device DMA Channel Next Descriptor Address Register -------- */

#define SAMHS_DEV_DMANXTDSC_OFFSET           (0x00)                                        /**<  (SAMHS_DEV_DMANXTDSC) Device DMA Channel Next Descriptor Address Register  Offset */

#define SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Pos  0                                              /**< (SAMHS_DEV_DMANXTDSC) Next Descriptor Address Position */
#define SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD      (_U_(0xFFFFFFFF) << SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Pos)  /**< (SAMHS_DEV_DMANXTDSC) Next Descriptor Address Mask */
#define SAMHS_DEV_DMANXTDSC_Msk              _U_(0xFFFFFFFF)                                /**< (SAMHS_DEV_DMANXTDSC) Register Mask  */


/* -------- SAMHS_DEV_DMAADDRESS : (USBHS Offset: 0x04) (R/W 32) Device DMA Channel Address Register -------- */

#define SAMHS_DEV_DMAADDRESS_OFFSET          (0x04)                                        /**<  (SAMHS_DEV_DMAADDRESS) Device DMA Channel Address Register  Offset */

#define SAMHS_DEV_DMAADDRESS_BUFF_ADD_Pos    0                                              /**< (SAMHS_DEV_DMAADDRESS) Buffer Address Position */
#define SAMHS_DEV_DMAADDRESS_BUFF_ADD        (_U_(0xFFFFFFFF) << SAMHS_DEV_DMAADDRESS_BUFF_ADD_Pos)  /**< (SAMHS_DEV_DMAADDRESS) Buffer Address Mask */
#define SAMHS_DEV_DMAADDRESS_Msk             _U_(0xFFFFFFFF)                                /**< (SAMHS_DEV_DMAADDRESS) Register Mask  */


/* -------- SAMHS_DEV_DMACONTROL : (USBHS Offset: 0x08) (R/W 32) Device DMA Channel Control Register -------- */

#define SAMHS_DEV_DMACONTROL_OFFSET          (0x08)                                        /**<  (SAMHS_DEV_DMACONTROL) Device DMA Channel Control Register  Offset */

#define SAMHS_DEV_DMACONTROL_CHANN_ENB_Pos   0                                              /**< (SAMHS_DEV_DMACONTROL) Channel Enable Command Position */
#define SAMHS_DEV_DMACONTROL_CHANN_ENB       (_U_(0x1) << SAMHS_DEV_DMACONTROL_CHANN_ENB_Pos)  /**< (SAMHS_DEV_DMACONTROL) Channel Enable Command Mask */
#define SAMHS_DEV_DMACONTROL_LDNXT_DSC_Pos   1                                              /**< (SAMHS_DEV_DMACONTROL) Load Next Channel Transfer Descriptor Enable Command Position */
#define SAMHS_DEV_DMACONTROL_LDNXT_DSC       (_U_(0x1) << SAMHS_DEV_DMACONTROL_LDNXT_DSC_Pos)  /**< (SAMHS_DEV_DMACONTROL) Load Next Channel Transfer Descriptor Enable Command Mask */
#define SAMHS_DEV_DMACONTROL_END_TR_EN_Pos   2                                              /**< (SAMHS_DEV_DMACONTROL) End of Transfer Enable Control (OUT transfers only) Position */
#define SAMHS_DEV_DMACONTROL_END_TR_EN       (_U_(0x1) << SAMHS_DEV_DMACONTROL_END_TR_EN_Pos)  /**< (SAMHS_DEV_DMACONTROL) End of Transfer Enable Control (OUT transfers only) Mask */
#define SAMHS_DEV_DMACONTROL_END_B_EN_Pos    3                                              /**< (SAMHS_DEV_DMACONTROL) End of Buffer Enable Control Position */
#define SAMHS_DEV_DMACONTROL_END_B_EN        (_U_(0x1) << SAMHS_DEV_DMACONTROL_END_B_EN_Pos)  /**< (SAMHS_DEV_DMACONTROL) End of Buffer Enable Control Mask */
#define SAMHS_DEV_DMACONTROL_END_TR_IT_Pos   4                                              /**< (SAMHS_DEV_DMACONTROL) End of Transfer Interrupt Enable Position */
#define SAMHS_DEV_DMACONTROL_END_TR_IT       (_U_(0x1) << SAMHS_DEV_DMACONTROL_END_TR_IT_Pos)  /**< (SAMHS_DEV_DMACONTROL) End of Transfer Interrupt Enable Mask */
#define SAMHS_DEV_DMACONTROL_END_BUFFIT_Pos  5                                              /**< (SAMHS_DEV_DMACONTROL) End of Buffer Interrupt Enable Position */
#define SAMHS_DEV_DMACONTROL_END_BUFFIT      (_U_(0x1) << SAMHS_DEV_DMACONTROL_END_BUFFIT_Pos)  /**< (SAMHS_DEV_DMACONTROL) End of Buffer Interrupt Enable Mask */
#define SAMHS_DEV_DMACONTROL_DESC_LD_IT_Pos  6                                              /**< (SAMHS_DEV_DMACONTROL) Descriptor Loaded Interrupt Enable Position */
#define SAMHS_DEV_DMACONTROL_DESC_LD_IT      (_U_(0x1) << SAMHS_DEV_DMACONTROL_DESC_LD_IT_Pos)  /**< (SAMHS_DEV_DMACONTROL) Descriptor Loaded Interrupt Enable Mask */
#define SAMHS_DEV_DMACONTROL_BURST_LCK_Pos   7                                              /**< (SAMHS_DEV_DMACONTROL) Burst Lock Enable Position */
#define SAMHS_DEV_DMACONTROL_BURST_LCK       (_U_(0x1) << SAMHS_DEV_DMACONTROL_BURST_LCK_Pos)  /**< (SAMHS_DEV_DMACONTROL) Burst Lock Enable Mask */
#define SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos 16                                             /**< (SAMHS_DEV_DMACONTROL) Buffer Byte Length (Write-only) Position */
#define SAMHS_DEV_DMACONTROL_BUFF_LENGTH     (_U_(0xFFFF) << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos)  /**< (SAMHS_DEV_DMACONTROL) Buffer Byte Length (Write-only) Mask */
#define SAMHS_DEV_DMACONTROL_Msk             _U_(0xFFFF00FF)                                /**< (SAMHS_DEV_DMACONTROL) Register Mask  */


/* -------- SAMHS_DEV_DMASTATUS : (USBHS Offset: 0x0c) (R/W 32) Device DMA Channel Status Register -------- */

#define SAMHS_DEV_DMASTATUS_OFFSET           (0x0C)                                        /**<  (SAMHS_DEV_DMASTATUS) Device DMA Channel Status Register  Offset */

#define SAMHS_DEV_DMASTATUS_CHANN_ENB_Pos    0                                              /**< (SAMHS_DEV_DMASTATUS) Channel Enable Status Position */
#define SAMHS_DEV_DMASTATUS_CHANN_ENB        (_U_(0x1) << SAMHS_DEV_DMASTATUS_CHANN_ENB_Pos)  /**< (SAMHS_DEV_DMASTATUS) Channel Enable Status Mask */
#define SAMHS_DEV_DMASTATUS_CHANN_ACT_Pos    1                                              /**< (SAMHS_DEV_DMASTATUS) Channel Active Status Position */
#define SAMHS_DEV_DMASTATUS_CHANN_ACT        (_U_(0x1) << SAMHS_DEV_DMASTATUS_CHANN_ACT_Pos)  /**< (SAMHS_DEV_DMASTATUS) Channel Active Status Mask */
#define SAMHS_DEV_DMASTATUS_END_TR_ST_Pos    4                                              /**< (SAMHS_DEV_DMASTATUS) End of Channel Transfer Status Position */
#define SAMHS_DEV_DMASTATUS_END_TR_ST        (_U_(0x1) << SAMHS_DEV_DMASTATUS_END_TR_ST_Pos)  /**< (SAMHS_DEV_DMASTATUS) End of Channel Transfer Status Mask */
#define SAMHS_DEV_DMASTATUS_END_BF_ST_Pos    5                                              /**< (SAMHS_DEV_DMASTATUS) End of Channel Buffer Status Position */
#define SAMHS_DEV_DMASTATUS_END_BF_ST        (_U_(0x1) << SAMHS_DEV_DMASTATUS_END_BF_ST_Pos)  /**< (SAMHS_DEV_DMASTATUS) End of Channel Buffer Status Mask */
#define SAMHS_DEV_DMASTATUS_DESC_LDST_Pos    6                                              /**< (SAMHS_DEV_DMASTATUS) Descriptor Loaded Status Position */
#define SAMHS_DEV_DMASTATUS_DESC_LDST        (_U_(0x1) << SAMHS_DEV_DMASTATUS_DESC_LDST_Pos)  /**< (SAMHS_DEV_DMASTATUS) Descriptor Loaded Status Mask */
#define SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos   16                                             /**< (SAMHS_DEV_DMASTATUS) Buffer Byte Count Position */
#define SAMHS_DEV_DMASTATUS_BUFF_COUNT       (_U_(0xFFFF) << SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos)  /**< (SAMHS_DEV_DMASTATUS) Buffer Byte Count Mask */
#define SAMHS_DEV_DMASTATUS_Msk              _U_(0xFFFF0073)                                /**< (SAMHS_DEV_DMASTATUS) Register Mask  */


/* -------- SAMHS_HST_DMANXTDSC : (USBHS Offset: 0x00) (R/W 32) Host DMA Channel Next Descriptor Address Register -------- */

#define SAMHS_HST_DMANXTDSC_OFFSET           (0x00)                                        /**<  (SAMHS_HST_DMANXTDSC) Host DMA Channel Next Descriptor Address Register  Offset */

#define SAMHS_HST_DMANXTDSC_NXT_DSC_ADD_Pos  0                                              /**< (SAMHS_HST_DMANXTDSC) Next Descriptor Address Position */
#define SAMHS_HST_DMANXTDSC_NXT_DSC_ADD      (_U_(0xFFFFFFFF) << SAMHS_HST_DMANXTDSC_NXT_DSC_ADD_Pos)  /**< (SAMHS_HST_DMANXTDSC) Next Descriptor Address Mask */
#define SAMHS_HST_DMANXTDSC_Msk              _U_(0xFFFFFFFF)                                /**< (SAMHS_HST_DMANXTDSC) Register Mask  */


/* -------- SAMHS_HST_DMAADDRESS : (USBHS Offset: 0x04) (R/W 32) Host DMA Channel Address Register -------- */

#define SAMHS_HST_DMAADDRESS_OFFSET          (0x04)                                        /**<  (SAMHS_HST_DMAADDRESS) Host DMA Channel Address Register  Offset */

#define SAMHS_HST_DMAADDRESS_BUFF_ADD_Pos    0                                              /**< (SAMHS_HST_DMAADDRESS) Buffer Address Position */
#define SAMHS_HST_DMAADDRESS_BUFF_ADD        (_U_(0xFFFFFFFF) << SAMHS_HST_DMAADDRESS_BUFF_ADD_Pos)  /**< (SAMHS_HST_DMAADDRESS) Buffer Address Mask */
#define SAMHS_HST_DMAADDRESS_Msk             _U_(0xFFFFFFFF)                                /**< (SAMHS_HST_DMAADDRESS) Register Mask  */


/* -------- SAMHS_HST_DMACONTROL : (USBHS Offset: 0x08) (R/W 32) Host DMA Channel Control Register -------- */

#define SAMHS_HST_DMACONTROL_OFFSET          (0x08)                                        /**<  (SAMHS_HST_DMACONTROL) Host DMA Channel Control Register  Offset */

#define SAMHS_HST_DMACONTROL_CHANN_ENB_Pos   0                                              /**< (SAMHS_HST_DMACONTROL) Channel Enable Command Position */
#define SAMHS_HST_DMACONTROL_CHANN_ENB       (_U_(0x1) << SAMHS_HST_DMACONTROL_CHANN_ENB_Pos)  /**< (SAMHS_HST_DMACONTROL) Channel Enable Command Mask */
#define SAMHS_HST_DMACONTROL_LDNXT_DSC_Pos   1                                              /**< (SAMHS_HST_DMACONTROL) Load Next Channel Transfer Descriptor Enable Command Position */
#define SAMHS_HST_DMACONTROL_LDNXT_DSC       (_U_(0x1) << SAMHS_HST_DMACONTROL_LDNXT_DSC_Pos)  /**< (SAMHS_HST_DMACONTROL) Load Next Channel Transfer Descriptor Enable Command Mask */
#define SAMHS_HST_DMACONTROL_END_TR_EN_Pos   2                                              /**< (SAMHS_HST_DMACONTROL) End of Transfer Enable Control (OUT transfers only) Position */
#define SAMHS_HST_DMACONTROL_END_TR_EN       (_U_(0x1) << SAMHS_HST_DMACONTROL_END_TR_EN_Pos)  /**< (SAMHS_HST_DMACONTROL) End of Transfer Enable Control (OUT transfers only) Mask */
#define SAMHS_HST_DMACONTROL_END_B_EN_Pos    3                                              /**< (SAMHS_HST_DMACONTROL) End of Buffer Enable Control Position */
#define SAMHS_HST_DMACONTROL_END_B_EN        (_U_(0x1) << SAMHS_HST_DMACONTROL_END_B_EN_Pos)  /**< (SAMHS_HST_DMACONTROL) End of Buffer Enable Control Mask */
#define SAMHS_HST_DMACONTROL_END_TR_IT_Pos   4                                              /**< (SAMHS_HST_DMACONTROL) End of Transfer Interrupt Enable Position */
#define SAMHS_HST_DMACONTROL_END_TR_IT       (_U_(0x1) << SAMHS_HST_DMACONTROL_END_TR_IT_Pos)  /**< (SAMHS_HST_DMACONTROL) End of Transfer Interrupt Enable Mask */
#define SAMHS_HST_DMACONTROL_END_BUFFIT_Pos  5                                              /**< (SAMHS_HST_DMACONTROL) End of Buffer Interrupt Enable Position */
#define SAMHS_HST_DMACONTROL_END_BUFFIT      (_U_(0x1) << SAMHS_HST_DMACONTROL_END_BUFFIT_Pos)  /**< (SAMHS_HST_DMACONTROL) End of Buffer Interrupt Enable Mask */
#define SAMHS_HST_DMACONTROL_DESC_LD_IT_Pos  6                                              /**< (SAMHS_HST_DMACONTROL) Descriptor Loaded Interrupt Enable Position */
#define SAMHS_HST_DMACONTROL_DESC_LD_IT      (_U_(0x1) << SAMHS_HST_DMACONTROL_DESC_LD_IT_Pos)  /**< (SAMHS_HST_DMACONTROL) Descriptor Loaded Interrupt Enable Mask */
#define SAMHS_HST_DMACONTROL_BURST_LCK_Pos   7                                              /**< (SAMHS_HST_DMACONTROL) Burst Lock Enable Position */
#define SAMHS_HST_DMACONTROL_BURST_LCK       (_U_(0x1) << SAMHS_HST_DMACONTROL_BURST_LCK_Pos)  /**< (SAMHS_HST_DMACONTROL) Burst Lock Enable Mask */
#define SAMHS_HST_DMACONTROL_BUFF_LENGTH_Pos 16                                             /**< (SAMHS_HST_DMACONTROL) Buffer Byte Length (Write-only) Position */
#define SAMHS_HST_DMACONTROL_BUFF_LENGTH     (_U_(0xFFFF) << SAMHS_HST_DMACONTROL_BUFF_LENGTH_Pos)  /**< (SAMHS_HST_DMACONTROL) Buffer Byte Length (Write-only) Mask */
#define SAMHS_HST_DMACONTROL_Msk             _U_(0xFFFF00FF)                                /**< (SAMHS_HST_DMACONTROL) Register Mask  */


/* -------- SAMHS_HST_DMASTATUS : (USBHS Offset: 0x0c) (R/W 32) Host DMA Channel Status Register -------- */

#define SAMHS_HST_DMASTATUS_OFFSET           (0x0C)                                        /**<  (SAMHS_HST_DMASTATUS) Host DMA Channel Status Register  Offset */

#define SAMHS_HST_DMASTATUS_CHANN_ENB_Pos    0                                              /**< (SAMHS_HST_DMASTATUS) Channel Enable Status Position */
#define SAMHS_HST_DMASTATUS_CHANN_ENB        (_U_(0x1) << SAMHS_HST_DMASTATUS_CHANN_ENB_Pos)  /**< (SAMHS_HST_DMASTATUS) Channel Enable Status Mask */
#define SAMHS_HST_DMASTATUS_CHANN_ACT_Pos    1                                              /**< (SAMHS_HST_DMASTATUS) Channel Active Status Position */
#define SAMHS_HST_DMASTATUS_CHANN_ACT        (_U_(0x1) << SAMHS_HST_DMASTATUS_CHANN_ACT_Pos)  /**< (SAMHS_HST_DMASTATUS) Channel Active Status Mask */
#define SAMHS_HST_DMASTATUS_END_TR_ST_Pos    4                                              /**< (SAMHS_HST_DMASTATUS) End of Channel Transfer Status Position */
#define SAMHS_HST_DMASTATUS_END_TR_ST        (_U_(0x1) << SAMHS_HST_DMASTATUS_END_TR_ST_Pos)  /**< (SAMHS_HST_DMASTATUS) End of Channel Transfer Status Mask */
#define SAMHS_HST_DMASTATUS_END_BF_ST_Pos    5                                              /**< (SAMHS_HST_DMASTATUS) End of Channel Buffer Status Position */
#define SAMHS_HST_DMASTATUS_END_BF_ST        (_U_(0x1) << SAMHS_HST_DMASTATUS_END_BF_ST_Pos)  /**< (SAMHS_HST_DMASTATUS) End of Channel Buffer Status Mask */
#define SAMHS_HST_DMASTATUS_DESC_LDST_Pos    6                                              /**< (SAMHS_HST_DMASTATUS) Descriptor Loaded Status Position */
#define SAMHS_HST_DMASTATUS_DESC_LDST        (_U_(0x1) << SAMHS_HST_DMASTATUS_DESC_LDST_Pos)  /**< (SAMHS_HST_DMASTATUS) Descriptor Loaded Status Mask */
#define SAMHS_HST_DMASTATUS_BUFF_COUNT_Pos   16                                             /**< (SAMHS_HST_DMASTATUS) Buffer Byte Count Position */
#define SAMHS_HST_DMASTATUS_BUFF_COUNT       (_U_(0xFFFF) << SAMHS_HST_DMASTATUS_BUFF_COUNT_Pos)  /**< (SAMHS_HST_DMASTATUS) Buffer Byte Count Mask */
#define SAMHS_HST_DMASTATUS_Msk              _U_(0xFFFF0073)                                /**< (SAMHS_HST_DMASTATUS) Register Mask  */


/* -------- SAMHS_DEV_CTRL : (USBHS Offset: 0x00) (R/W 32) Device General Control Register -------- */

#define SAMHS_DEV_CTRL_OFFSET                (0x00)                                        /**<  (SAMHS_DEV_CTRL) Device General Control Register  Offset */

#define SAMHS_DEV_CTRL_UADD_Pos              0                                              /**< (SAMHS_DEV_CTRL) USB Address Position */
#define SAMHS_DEV_CTRL_UADD                  (_U_(0x7F) << SAMHS_DEV_CTRL_UADD_Pos)          /**< (SAMHS_DEV_CTRL) USB Address Mask */
#define SAMHS_DEV_CTRL_ADDEN_Pos             7                                              /**< (SAMHS_DEV_CTRL) Address Enable Position */
#define SAMHS_DEV_CTRL_ADDEN                 (_U_(0x1) << SAMHS_DEV_CTRL_ADDEN_Pos)          /**< (SAMHS_DEV_CTRL) Address Enable Mask */
#define SAMHS_DEV_CTRL_DETACH_Pos            8                                              /**< (SAMHS_DEV_CTRL) Detach Position */
#define SAMHS_DEV_CTRL_DETACH                (_U_(0x1) << SAMHS_DEV_CTRL_DETACH_Pos)         /**< (SAMHS_DEV_CTRL) Detach Mask */
#define SAMHS_DEV_CTRL_RMWKUP_Pos            9                                              /**< (SAMHS_DEV_CTRL) Remote Wake-Up Position */
#define SAMHS_DEV_CTRL_RMWKUP                (_U_(0x1) << SAMHS_DEV_CTRL_RMWKUP_Pos)         /**< (SAMHS_DEV_CTRL) Remote Wake-Up Mask */
#define SAMHS_DEV_CTRL_SPDCONF_Pos           10                                             /**< (SAMHS_DEV_CTRL) Mode Configuration Position */
#define SAMHS_DEV_CTRL_SPDCONF               (_U_(0x3) << SAMHS_DEV_CTRL_SPDCONF_Pos)        /**< (SAMHS_DEV_CTRL) Mode Configuration Mask */
#define   SAMHS_DEV_CTRL_SPDCONF_NORMAL_Val  _U_(0x0)                                       /**< (SAMHS_DEV_CTRL) The peripheral starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the host is high-speed-capable.  */
#define   SAMHS_DEV_CTRL_SPDCONF_LOW_POWER_Val _U_(0x1)                                       /**< (SAMHS_DEV_CTRL) For a better consumption, if high speed is not needed.  */
#define   SAMHS_DEV_CTRL_SPDCONF_HIGH_SPEED_Val _U_(0x2)                                       /**< (SAMHS_DEV_CTRL) Forced high speed.  */
#define   SAMHS_DEV_CTRL_SPDCONF_FORCED_FS_Val _U_(0x3)                                       /**< (SAMHS_DEV_CTRL) The peripheral remains in Full-speed mode whatever the host speed capability.  */
#define SAMHS_DEV_CTRL_SPDCONF_NORMAL        (SAMHS_DEV_CTRL_SPDCONF_NORMAL_Val << SAMHS_DEV_CTRL_SPDCONF_Pos)  /**< (SAMHS_DEV_CTRL) The peripheral starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the host is high-speed-capable. Position  */
#define SAMHS_DEV_CTRL_SPDCONF_LOW_POWER     (SAMHS_DEV_CTRL_SPDCONF_LOW_POWER_Val << SAMHS_DEV_CTRL_SPDCONF_Pos)  /**< (SAMHS_DEV_CTRL) For a better consumption, if high speed is not needed. Position  */
#define SAMHS_DEV_CTRL_SPDCONF_HIGH_SPEED    (SAMHS_DEV_CTRL_SPDCONF_HIGH_SPEED_Val << SAMHS_DEV_CTRL_SPDCONF_Pos)  /**< (SAMHS_DEV_CTRL) Forced high speed. Position  */
#define SAMHS_DEV_CTRL_SPDCONF_FORCED_FS     (SAMHS_DEV_CTRL_SPDCONF_FORCED_FS_Val << SAMHS_DEV_CTRL_SPDCONF_Pos)  /**< (SAMHS_DEV_CTRL) The peripheral remains in Full-speed mode whatever the host speed capability. Position  */
#define SAMHS_DEV_CTRL_LS_Pos                12                                             /**< (SAMHS_DEV_CTRL) Low-Speed Mode Force Position */
#define SAMHS_DEV_CTRL_LS                    (_U_(0x1) << SAMHS_DEV_CTRL_LS_Pos)             /**< (SAMHS_DEV_CTRL) Low-Speed Mode Force Mask */
#define SAMHS_DEV_CTRL_TSTJ_Pos              13                                             /**< (SAMHS_DEV_CTRL) Test mode J Position */
#define SAMHS_DEV_CTRL_TSTJ                  (_U_(0x1) << SAMHS_DEV_CTRL_TSTJ_Pos)           /**< (SAMHS_DEV_CTRL) Test mode J Mask */
#define SAMHS_DEV_CTRL_TSTK_Pos              14                                             /**< (SAMHS_DEV_CTRL) Test mode K Position */
#define SAMHS_DEV_CTRL_TSTK                  (_U_(0x1) << SAMHS_DEV_CTRL_TSTK_Pos)           /**< (SAMHS_DEV_CTRL) Test mode K Mask */
#define SAMHS_DEV_CTRL_TSTPCKT_Pos           15                                             /**< (SAMHS_DEV_CTRL) Test packet mode Position */
#define SAMHS_DEV_CTRL_TSTPCKT               (_U_(0x1) << SAMHS_DEV_CTRL_TSTPCKT_Pos)        /**< (SAMHS_DEV_CTRL) Test packet mode Mask */
#define SAMHS_DEV_CTRL_OPMODE2_Pos           16                                             /**< (SAMHS_DEV_CTRL) Specific Operational mode Position */
#define SAMHS_DEV_CTRL_OPMODE2               (_U_(0x1) << SAMHS_DEV_CTRL_OPMODE2_Pos)        /**< (SAMHS_DEV_CTRL) Specific Operational mode Mask */
#define SAMHS_DEV_CTRL_Msk                   _U_(0x1FFFF)                                   /**< (SAMHS_DEV_CTRL) Register Mask  */

#define SAMHS_DEV_CTRL_OPMODE_Pos            16                                             /**< (SAMHS_DEV_CTRL Position) Specific Operational mode */
#define SAMHS_DEV_CTRL_OPMODE                (_U_(0x1) << SAMHS_DEV_CTRL_OPMODE_Pos)         /**< (SAMHS_DEV_CTRL Mask) OPMODE */

/* -------- SAMHS_DEV_ISR : (USBHS Offset: 0x04) (R/ 32) Device Global Interrupt Status Register -------- */

#define SAMHS_DEV_ISR_OFFSET                 (0x04)                                        /**<  (SAMHS_DEV_ISR) Device Global Interrupt Status Register  Offset */

#define SAMHS_DEV_ISR_SUSP_Pos               0                                              /**< (SAMHS_DEV_ISR) Suspend Interrupt Position */
#define SAMHS_DEV_ISR_SUSP                   (_U_(0x1) << SAMHS_DEV_ISR_SUSP_Pos)            /**< (SAMHS_DEV_ISR) Suspend Interrupt Mask */
#define SAMHS_DEV_ISR_MSOF_Pos               1                                              /**< (SAMHS_DEV_ISR) Micro Start of Frame Interrupt Position */
#define SAMHS_DEV_ISR_MSOF                   (_U_(0x1) << SAMHS_DEV_ISR_MSOF_Pos)            /**< (SAMHS_DEV_ISR) Micro Start of Frame Interrupt Mask */
#define SAMHS_DEV_ISR_SOF_Pos                2                                              /**< (SAMHS_DEV_ISR) Start of Frame Interrupt Position */
#define SAMHS_DEV_ISR_SOF                    (_U_(0x1) << SAMHS_DEV_ISR_SOF_Pos)             /**< (SAMHS_DEV_ISR) Start of Frame Interrupt Mask */
#define SAMHS_DEV_ISR_EORST_Pos              3                                              /**< (SAMHS_DEV_ISR) End of Reset Interrupt Position */
#define SAMHS_DEV_ISR_EORST                  (_U_(0x1) << SAMHS_DEV_ISR_EORST_Pos)           /**< (SAMHS_DEV_ISR) End of Reset Interrupt Mask */
#define SAMHS_DEV_ISR_WAKEUP_Pos             4                                              /**< (SAMHS_DEV_ISR) Wake-Up Interrupt Position */
#define SAMHS_DEV_ISR_WAKEUP                 (_U_(0x1) << SAMHS_DEV_ISR_WAKEUP_Pos)          /**< (SAMHS_DEV_ISR) Wake-Up Interrupt Mask */
#define SAMHS_DEV_ISR_EORSM_Pos              5                                              /**< (SAMHS_DEV_ISR) End of Resume Interrupt Position */
#define SAMHS_DEV_ISR_EORSM                  (_U_(0x1) << SAMHS_DEV_ISR_EORSM_Pos)           /**< (SAMHS_DEV_ISR) End of Resume Interrupt Mask */
#define SAMHS_DEV_ISR_UPRSM_Pos              6                                              /**< (SAMHS_DEV_ISR) Upstream Resume Interrupt Position */
#define SAMHS_DEV_ISR_UPRSM                  (_U_(0x1) << SAMHS_DEV_ISR_UPRSM_Pos)           /**< (SAMHS_DEV_ISR) Upstream Resume Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_0_Pos              12                                             /**< (SAMHS_DEV_ISR) Endpoint 0 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_0                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_0_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 0 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_1_Pos              13                                             /**< (SAMHS_DEV_ISR) Endpoint 1 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_1                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_1_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 1 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_2_Pos              14                                             /**< (SAMHS_DEV_ISR) Endpoint 2 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_2                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_2_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 2 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_3_Pos              15                                             /**< (SAMHS_DEV_ISR) Endpoint 3 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_3                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_3_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 3 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_4_Pos              16                                             /**< (SAMHS_DEV_ISR) Endpoint 4 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_4                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_4_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 4 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_5_Pos              17                                             /**< (SAMHS_DEV_ISR) Endpoint 5 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_5                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_5_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 5 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_6_Pos              18                                             /**< (SAMHS_DEV_ISR) Endpoint 6 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_6                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_6_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 6 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_7_Pos              19                                             /**< (SAMHS_DEV_ISR) Endpoint 7 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_7                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_7_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 7 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_8_Pos              20                                             /**< (SAMHS_DEV_ISR) Endpoint 8 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_8                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_8_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 8 Interrupt Mask */
#define SAMHS_DEV_ISR_PEP_9_Pos              21                                             /**< (SAMHS_DEV_ISR) Endpoint 9 Interrupt Position */
#define SAMHS_DEV_ISR_PEP_9                  (_U_(0x1) << SAMHS_DEV_ISR_PEP_9_Pos)           /**< (SAMHS_DEV_ISR) Endpoint 9 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_1_Pos              25                                             /**< (SAMHS_DEV_ISR) DMA Channel 1 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_1                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_1_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 1 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_2_Pos              26                                             /**< (SAMHS_DEV_ISR) DMA Channel 2 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_2                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_2_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 2 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_3_Pos              27                                             /**< (SAMHS_DEV_ISR) DMA Channel 3 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_3                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_3_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 3 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_4_Pos              28                                             /**< (SAMHS_DEV_ISR) DMA Channel 4 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_4                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_4_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 4 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_5_Pos              29                                             /**< (SAMHS_DEV_ISR) DMA Channel 5 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_5                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_5_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 5 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_6_Pos              30                                             /**< (SAMHS_DEV_ISR) DMA Channel 6 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_6                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_6_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 6 Interrupt Mask */
#define SAMHS_DEV_ISR_DMA_7_Pos              31                                             /**< (SAMHS_DEV_ISR) DMA Channel 7 Interrupt Position */
#define SAMHS_DEV_ISR_DMA_7                  (_U_(0x1) << SAMHS_DEV_ISR_DMA_7_Pos)           /**< (SAMHS_DEV_ISR) DMA Channel 7 Interrupt Mask */
#define SAMHS_DEV_ISR_Msk                    _U_(0xFE3FF07F)                                /**< (SAMHS_DEV_ISR) Register Mask  */

#define SAMHS_DEV_ISR_PEP__Pos               12                                             /**< (SAMHS_DEV_ISR Position) Endpoint x Interrupt */
#define SAMHS_DEV_ISR_PEP_                   (_U_(0x3FF) << SAMHS_DEV_ISR_PEP__Pos)          /**< (SAMHS_DEV_ISR Mask) PEP_ */
#define SAMHS_DEV_ISR_DMA__Pos               25                                             /**< (SAMHS_DEV_ISR Position) DMA Channel 7 Interrupt */
#define SAMHS_DEV_ISR_DMA_                   (_U_(0x7F) << SAMHS_DEV_ISR_DMA__Pos)           /**< (SAMHS_DEV_ISR Mask) DMA_ */

/* -------- SAMHS_DEV_ICR : (USBHS Offset: 0x08) (/W 32) Device Global Interrupt Clear Register -------- */

#define SAMHS_DEV_ICR_OFFSET                 (0x08)                                        /**<  (SAMHS_DEV_ICR) Device Global Interrupt Clear Register  Offset */

#define SAMHS_DEV_ICR_SUSPC_Pos              0                                              /**< (SAMHS_DEV_ICR) Suspend Interrupt Clear Position */
#define SAMHS_DEV_ICR_SUSPC                  (_U_(0x1) << SAMHS_DEV_ICR_SUSPC_Pos)           /**< (SAMHS_DEV_ICR) Suspend Interrupt Clear Mask */
#define SAMHS_DEV_ICR_MSOFC_Pos              1                                              /**< (SAMHS_DEV_ICR) Micro Start of Frame Interrupt Clear Position */
#define SAMHS_DEV_ICR_MSOFC                  (_U_(0x1) << SAMHS_DEV_ICR_MSOFC_Pos)           /**< (SAMHS_DEV_ICR) Micro Start of Frame Interrupt Clear Mask */
#define SAMHS_DEV_ICR_SOFC_Pos               2                                              /**< (SAMHS_DEV_ICR) Start of Frame Interrupt Clear Position */
#define SAMHS_DEV_ICR_SOFC                   (_U_(0x1) << SAMHS_DEV_ICR_SOFC_Pos)            /**< (SAMHS_DEV_ICR) Start of Frame Interrupt Clear Mask */
#define SAMHS_DEV_ICR_EORSTC_Pos             3                                              /**< (SAMHS_DEV_ICR) End of Reset Interrupt Clear Position */
#define SAMHS_DEV_ICR_EORSTC                 (_U_(0x1) << SAMHS_DEV_ICR_EORSTC_Pos)          /**< (SAMHS_DEV_ICR) End of Reset Interrupt Clear Mask */
#define SAMHS_DEV_ICR_WAKEUPC_Pos            4                                              /**< (SAMHS_DEV_ICR) Wake-Up Interrupt Clear Position */
#define SAMHS_DEV_ICR_WAKEUPC                (_U_(0x1) << SAMHS_DEV_ICR_WAKEUPC_Pos)         /**< (SAMHS_DEV_ICR) Wake-Up Interrupt Clear Mask */
#define SAMHS_DEV_ICR_EORSMC_Pos             5                                              /**< (SAMHS_DEV_ICR) End of Resume Interrupt Clear Position */
#define SAMHS_DEV_ICR_EORSMC                 (_U_(0x1) << SAMHS_DEV_ICR_EORSMC_Pos)          /**< (SAMHS_DEV_ICR) End of Resume Interrupt Clear Mask */
#define SAMHS_DEV_ICR_UPRSMC_Pos             6                                              /**< (SAMHS_DEV_ICR) Upstream Resume Interrupt Clear Position */
#define SAMHS_DEV_ICR_UPRSMC                 (_U_(0x1) << SAMHS_DEV_ICR_UPRSMC_Pos)          /**< (SAMHS_DEV_ICR) Upstream Resume Interrupt Clear Mask */
#define SAMHS_DEV_ICR_Msk                    _U_(0x7F)                                      /**< (SAMHS_DEV_ICR) Register Mask  */


/* -------- SAMHS_DEV_IFR : (USBHS Offset: 0x0c) (/W 32) Device Global Interrupt Set Register -------- */

#define SAMHS_DEV_IFR_OFFSET                 (0x0C)                                        /**<  (SAMHS_DEV_IFR) Device Global Interrupt Set Register  Offset */

#define SAMHS_DEV_IFR_SUSPS_Pos              0                                              /**< (SAMHS_DEV_IFR) Suspend Interrupt Set Position */
#define SAMHS_DEV_IFR_SUSPS                  (_U_(0x1) << SAMHS_DEV_IFR_SUSPS_Pos)           /**< (SAMHS_DEV_IFR) Suspend Interrupt Set Mask */
#define SAMHS_DEV_IFR_MSOFS_Pos              1                                              /**< (SAMHS_DEV_IFR) Micro Start of Frame Interrupt Set Position */
#define SAMHS_DEV_IFR_MSOFS                  (_U_(0x1) << SAMHS_DEV_IFR_MSOFS_Pos)           /**< (SAMHS_DEV_IFR) Micro Start of Frame Interrupt Set Mask */
#define SAMHS_DEV_IFR_SOFS_Pos               2                                              /**< (SAMHS_DEV_IFR) Start of Frame Interrupt Set Position */
#define SAMHS_DEV_IFR_SOFS                   (_U_(0x1) << SAMHS_DEV_IFR_SOFS_Pos)            /**< (SAMHS_DEV_IFR) Start of Frame Interrupt Set Mask */
#define SAMHS_DEV_IFR_EORSTS_Pos             3                                              /**< (SAMHS_DEV_IFR) End of Reset Interrupt Set Position */
#define SAMHS_DEV_IFR_EORSTS                 (_U_(0x1) << SAMHS_DEV_IFR_EORSTS_Pos)          /**< (SAMHS_DEV_IFR) End of Reset Interrupt Set Mask */
#define SAMHS_DEV_IFR_WAKEUPS_Pos            4                                              /**< (SAMHS_DEV_IFR) Wake-Up Interrupt Set Position */
#define SAMHS_DEV_IFR_WAKEUPS                (_U_(0x1) << SAMHS_DEV_IFR_WAKEUPS_Pos)         /**< (SAMHS_DEV_IFR) Wake-Up Interrupt Set Mask */
#define SAMHS_DEV_IFR_EORSMS_Pos             5                                              /**< (SAMHS_DEV_IFR) End of Resume Interrupt Set Position */
#define SAMHS_DEV_IFR_EORSMS                 (_U_(0x1) << SAMHS_DEV_IFR_EORSMS_Pos)          /**< (SAMHS_DEV_IFR) End of Resume Interrupt Set Mask */
#define SAMHS_DEV_IFR_UPRSMS_Pos             6                                              /**< (SAMHS_DEV_IFR) Upstream Resume Interrupt Set Position */
#define SAMHS_DEV_IFR_UPRSMS                 (_U_(0x1) << SAMHS_DEV_IFR_UPRSMS_Pos)          /**< (SAMHS_DEV_IFR) Upstream Resume Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_1_Pos              25                                             /**< (SAMHS_DEV_IFR) DMA Channel 1 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_1                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_1_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 1 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_2_Pos              26                                             /**< (SAMHS_DEV_IFR) DMA Channel 2 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_2                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_2_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 2 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_3_Pos              27                                             /**< (SAMHS_DEV_IFR) DMA Channel 3 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_3                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_3_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 3 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_4_Pos              28                                             /**< (SAMHS_DEV_IFR) DMA Channel 4 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_4                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_4_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 4 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_5_Pos              29                                             /**< (SAMHS_DEV_IFR) DMA Channel 5 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_5                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_5_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 5 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_6_Pos              30                                             /**< (SAMHS_DEV_IFR) DMA Channel 6 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_6                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_6_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 6 Interrupt Set Mask */
#define SAMHS_DEV_IFR_DMA_7_Pos              31                                             /**< (SAMHS_DEV_IFR) DMA Channel 7 Interrupt Set Position */
#define SAMHS_DEV_IFR_DMA_7                  (_U_(0x1) << SAMHS_DEV_IFR_DMA_7_Pos)           /**< (SAMHS_DEV_IFR) DMA Channel 7 Interrupt Set Mask */
#define SAMHS_DEV_IFR_Msk                    _U_(0xFE00007F)                                /**< (SAMHS_DEV_IFR) Register Mask  */

#define SAMHS_DEV_IFR_DMA__Pos               25                                             /**< (SAMHS_DEV_IFR Position) DMA Channel 7 Interrupt Set */
#define SAMHS_DEV_IFR_DMA_                   (_U_(0x7F) << SAMHS_DEV_IFR_DMA__Pos)           /**< (SAMHS_DEV_IFR Mask) DMA_ */

/* -------- SAMHS_DEV_IMR : (USBHS Offset: 0x10) (R/ 32) Device Global Interrupt Mask Register -------- */

#define SAMHS_DEV_IMR_OFFSET                 (0x10)                                        /**<  (SAMHS_DEV_IMR) Device Global Interrupt Mask Register  Offset */

#define SAMHS_DEV_IMR_SUSPE_Pos              0                                              /**< (SAMHS_DEV_IMR) Suspend Interrupt Mask Position */
#define SAMHS_DEV_IMR_SUSPE                  (_U_(0x1) << SAMHS_DEV_IMR_SUSPE_Pos)           /**< (SAMHS_DEV_IMR) Suspend Interrupt Mask Mask */
#define SAMHS_DEV_IMR_MSOFE_Pos              1                                              /**< (SAMHS_DEV_IMR) Micro Start of Frame Interrupt Mask Position */
#define SAMHS_DEV_IMR_MSOFE                  (_U_(0x1) << SAMHS_DEV_IMR_MSOFE_Pos)           /**< (SAMHS_DEV_IMR) Micro Start of Frame Interrupt Mask Mask */
#define SAMHS_DEV_IMR_SOFE_Pos               2                                              /**< (SAMHS_DEV_IMR) Start of Frame Interrupt Mask Position */
#define SAMHS_DEV_IMR_SOFE                   (_U_(0x1) << SAMHS_DEV_IMR_SOFE_Pos)            /**< (SAMHS_DEV_IMR) Start of Frame Interrupt Mask Mask */
#define SAMHS_DEV_IMR_EORSTE_Pos             3                                              /**< (SAMHS_DEV_IMR) End of Reset Interrupt Mask Position */
#define SAMHS_DEV_IMR_EORSTE                 (_U_(0x1) << SAMHS_DEV_IMR_EORSTE_Pos)          /**< (SAMHS_DEV_IMR) End of Reset Interrupt Mask Mask */
#define SAMHS_DEV_IMR_WAKEUPE_Pos            4                                              /**< (SAMHS_DEV_IMR) Wake-Up Interrupt Mask Position */
#define SAMHS_DEV_IMR_WAKEUPE                (_U_(0x1) << SAMHS_DEV_IMR_WAKEUPE_Pos)         /**< (SAMHS_DEV_IMR) Wake-Up Interrupt Mask Mask */
#define SAMHS_DEV_IMR_EORSME_Pos             5                                              /**< (SAMHS_DEV_IMR) End of Resume Interrupt Mask Position */
#define SAMHS_DEV_IMR_EORSME                 (_U_(0x1) << SAMHS_DEV_IMR_EORSME_Pos)          /**< (SAMHS_DEV_IMR) End of Resume Interrupt Mask Mask */
#define SAMHS_DEV_IMR_UPRSME_Pos             6                                              /**< (SAMHS_DEV_IMR) Upstream Resume Interrupt Mask Position */
#define SAMHS_DEV_IMR_UPRSME                 (_U_(0x1) << SAMHS_DEV_IMR_UPRSME_Pos)          /**< (SAMHS_DEV_IMR) Upstream Resume Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_0_Pos              12                                             /**< (SAMHS_DEV_IMR) Endpoint 0 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_0                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_0_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 0 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_1_Pos              13                                             /**< (SAMHS_DEV_IMR) Endpoint 1 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_1                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_1_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 1 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_2_Pos              14                                             /**< (SAMHS_DEV_IMR) Endpoint 2 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_2                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_2_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 2 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_3_Pos              15                                             /**< (SAMHS_DEV_IMR) Endpoint 3 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_3                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_3_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 3 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_4_Pos              16                                             /**< (SAMHS_DEV_IMR) Endpoint 4 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_4                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_4_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 4 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_5_Pos              17                                             /**< (SAMHS_DEV_IMR) Endpoint 5 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_5                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_5_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 5 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_6_Pos              18                                             /**< (SAMHS_DEV_IMR) Endpoint 6 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_6                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_6_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 6 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_7_Pos              19                                             /**< (SAMHS_DEV_IMR) Endpoint 7 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_7                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_7_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 7 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_8_Pos              20                                             /**< (SAMHS_DEV_IMR) Endpoint 8 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_8                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_8_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 8 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_PEP_9_Pos              21                                             /**< (SAMHS_DEV_IMR) Endpoint 9 Interrupt Mask Position */
#define SAMHS_DEV_IMR_PEP_9                  (_U_(0x1) << SAMHS_DEV_IMR_PEP_9_Pos)           /**< (SAMHS_DEV_IMR) Endpoint 9 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_1_Pos              25                                             /**< (SAMHS_DEV_IMR) DMA Channel 1 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_1                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_1_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 1 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_2_Pos              26                                             /**< (SAMHS_DEV_IMR) DMA Channel 2 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_2                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_2_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 2 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_3_Pos              27                                             /**< (SAMHS_DEV_IMR) DMA Channel 3 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_3                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_3_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 3 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_4_Pos              28                                             /**< (SAMHS_DEV_IMR) DMA Channel 4 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_4                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_4_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 4 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_5_Pos              29                                             /**< (SAMHS_DEV_IMR) DMA Channel 5 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_5                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_5_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 5 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_6_Pos              30                                             /**< (SAMHS_DEV_IMR) DMA Channel 6 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_6                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_6_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 6 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_DMA_7_Pos              31                                             /**< (SAMHS_DEV_IMR) DMA Channel 7 Interrupt Mask Position */
#define SAMHS_DEV_IMR_DMA_7                  (_U_(0x1) << SAMHS_DEV_IMR_DMA_7_Pos)           /**< (SAMHS_DEV_IMR) DMA Channel 7 Interrupt Mask Mask */
#define SAMHS_DEV_IMR_Msk                    _U_(0xFE3FF07F)                                /**< (SAMHS_DEV_IMR) Register Mask  */

#define SAMHS_DEV_IMR_PEP__Pos               12                                             /**< (SAMHS_DEV_IMR Position) Endpoint x Interrupt Mask */
#define SAMHS_DEV_IMR_PEP_                   (_U_(0x3FF) << SAMHS_DEV_IMR_PEP__Pos)          /**< (SAMHS_DEV_IMR Mask) PEP_ */
#define SAMHS_DEV_IMR_DMA__Pos               25                                             /**< (SAMHS_DEV_IMR Position) DMA Channel 7 Interrupt Mask */
#define SAMHS_DEV_IMR_DMA_                   (_U_(0x7F) << SAMHS_DEV_IMR_DMA__Pos)           /**< (SAMHS_DEV_IMR Mask) DMA_ */

/* -------- SAMHS_DEV_IDR : (USBHS Offset: 0x14) (/W 32) Device Global Interrupt Disable Register -------- */

#define SAMHS_DEV_IDR_OFFSET                 (0x14)                                        /**<  (SAMHS_DEV_IDR) Device Global Interrupt Disable Register  Offset */

#define SAMHS_DEV_IDR_SUSPEC_Pos             0                                              /**< (SAMHS_DEV_IDR) Suspend Interrupt Disable Position */
#define SAMHS_DEV_IDR_SUSPEC                 (_U_(0x1) << SAMHS_DEV_IDR_SUSPEC_Pos)          /**< (SAMHS_DEV_IDR) Suspend Interrupt Disable Mask */
#define SAMHS_DEV_IDR_MSOFEC_Pos             1                                              /**< (SAMHS_DEV_IDR) Micro Start of Frame Interrupt Disable Position */
#define SAMHS_DEV_IDR_MSOFEC                 (_U_(0x1) << SAMHS_DEV_IDR_MSOFEC_Pos)          /**< (SAMHS_DEV_IDR) Micro Start of Frame Interrupt Disable Mask */
#define SAMHS_DEV_IDR_SOFEC_Pos              2                                              /**< (SAMHS_DEV_IDR) Start of Frame Interrupt Disable Position */
#define SAMHS_DEV_IDR_SOFEC                  (_U_(0x1) << SAMHS_DEV_IDR_SOFEC_Pos)           /**< (SAMHS_DEV_IDR) Start of Frame Interrupt Disable Mask */
#define SAMHS_DEV_IDR_EORSTEC_Pos            3                                              /**< (SAMHS_DEV_IDR) End of Reset Interrupt Disable Position */
#define SAMHS_DEV_IDR_EORSTEC                (_U_(0x1) << SAMHS_DEV_IDR_EORSTEC_Pos)         /**< (SAMHS_DEV_IDR) End of Reset Interrupt Disable Mask */
#define SAMHS_DEV_IDR_WAKEUPEC_Pos           4                                              /**< (SAMHS_DEV_IDR) Wake-Up Interrupt Disable Position */
#define SAMHS_DEV_IDR_WAKEUPEC               (_U_(0x1) << SAMHS_DEV_IDR_WAKEUPEC_Pos)        /**< (SAMHS_DEV_IDR) Wake-Up Interrupt Disable Mask */
#define SAMHS_DEV_IDR_EORSMEC_Pos            5                                              /**< (SAMHS_DEV_IDR) End of Resume Interrupt Disable Position */
#define SAMHS_DEV_IDR_EORSMEC                (_U_(0x1) << SAMHS_DEV_IDR_EORSMEC_Pos)         /**< (SAMHS_DEV_IDR) End of Resume Interrupt Disable Mask */
#define SAMHS_DEV_IDR_UPRSMEC_Pos            6                                              /**< (SAMHS_DEV_IDR) Upstream Resume Interrupt Disable Position */
#define SAMHS_DEV_IDR_UPRSMEC                (_U_(0x1) << SAMHS_DEV_IDR_UPRSMEC_Pos)         /**< (SAMHS_DEV_IDR) Upstream Resume Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_0_Pos              12                                             /**< (SAMHS_DEV_IDR) Endpoint 0 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_0                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_0_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 0 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_1_Pos              13                                             /**< (SAMHS_DEV_IDR) Endpoint 1 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_1                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_1_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 1 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_2_Pos              14                                             /**< (SAMHS_DEV_IDR) Endpoint 2 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_2                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_2_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 2 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_3_Pos              15                                             /**< (SAMHS_DEV_IDR) Endpoint 3 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_3                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_3_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 3 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_4_Pos              16                                             /**< (SAMHS_DEV_IDR) Endpoint 4 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_4                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_4_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 4 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_5_Pos              17                                             /**< (SAMHS_DEV_IDR) Endpoint 5 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_5                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_5_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 5 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_6_Pos              18                                             /**< (SAMHS_DEV_IDR) Endpoint 6 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_6                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_6_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 6 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_7_Pos              19                                             /**< (SAMHS_DEV_IDR) Endpoint 7 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_7                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_7_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 7 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_8_Pos              20                                             /**< (SAMHS_DEV_IDR) Endpoint 8 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_8                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_8_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 8 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_PEP_9_Pos              21                                             /**< (SAMHS_DEV_IDR) Endpoint 9 Interrupt Disable Position */
#define SAMHS_DEV_IDR_PEP_9                  (_U_(0x1) << SAMHS_DEV_IDR_PEP_9_Pos)           /**< (SAMHS_DEV_IDR) Endpoint 9 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_1_Pos              25                                             /**< (SAMHS_DEV_IDR) DMA Channel 1 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_1                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_1_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 1 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_2_Pos              26                                             /**< (SAMHS_DEV_IDR) DMA Channel 2 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_2                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_2_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 2 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_3_Pos              27                                             /**< (SAMHS_DEV_IDR) DMA Channel 3 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_3                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_3_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 3 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_4_Pos              28                                             /**< (SAMHS_DEV_IDR) DMA Channel 4 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_4                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_4_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 4 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_5_Pos              29                                             /**< (SAMHS_DEV_IDR) DMA Channel 5 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_5                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_5_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 5 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_6_Pos              30                                             /**< (SAMHS_DEV_IDR) DMA Channel 6 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_6                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_6_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 6 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_DMA_7_Pos              31                                             /**< (SAMHS_DEV_IDR) DMA Channel 7 Interrupt Disable Position */
#define SAMHS_DEV_IDR_DMA_7                  (_U_(0x1) << SAMHS_DEV_IDR_DMA_7_Pos)           /**< (SAMHS_DEV_IDR) DMA Channel 7 Interrupt Disable Mask */
#define SAMHS_DEV_IDR_Msk                    _U_(0xFE3FF07F)                                /**< (SAMHS_DEV_IDR) Register Mask  */

#define SAMHS_DEV_IDR_PEP__Pos               12                                             /**< (SAMHS_DEV_IDR Position) Endpoint x Interrupt Disable */
#define SAMHS_DEV_IDR_PEP_                   (_U_(0x3FF) << SAMHS_DEV_IDR_PEP__Pos)          /**< (SAMHS_DEV_IDR Mask) PEP_ */
#define SAMHS_DEV_IDR_DMA__Pos               25                                             /**< (SAMHS_DEV_IDR Position) DMA Channel 7 Interrupt Disable */
#define SAMHS_DEV_IDR_DMA_                   (_U_(0x7F) << SAMHS_DEV_IDR_DMA__Pos)           /**< (SAMHS_DEV_IDR Mask) DMA_ */

/* -------- SAMHS_DEV_IER : (USBHS Offset: 0x18) (/W 32) Device Global Interrupt Enable Register -------- */

#define SAMHS_DEV_IER_OFFSET                 (0x18)                                        /**<  (SAMHS_DEV_IER) Device Global Interrupt Enable Register  Offset */

#define SAMHS_DEV_IER_SUSPES_Pos             0                                              /**< (SAMHS_DEV_IER) Suspend Interrupt Enable Position */
#define SAMHS_DEV_IER_SUSPES                 (_U_(0x1) << SAMHS_DEV_IER_SUSPES_Pos)          /**< (SAMHS_DEV_IER) Suspend Interrupt Enable Mask */
#define SAMHS_DEV_IER_MSOFES_Pos             1                                              /**< (SAMHS_DEV_IER) Micro Start of Frame Interrupt Enable Position */
#define SAMHS_DEV_IER_MSOFES                 (_U_(0x1) << SAMHS_DEV_IER_MSOFES_Pos)          /**< (SAMHS_DEV_IER) Micro Start of Frame Interrupt Enable Mask */
#define SAMHS_DEV_IER_SOFES_Pos              2                                              /**< (SAMHS_DEV_IER) Start of Frame Interrupt Enable Position */
#define SAMHS_DEV_IER_SOFES                  (_U_(0x1) << SAMHS_DEV_IER_SOFES_Pos)           /**< (SAMHS_DEV_IER) Start of Frame Interrupt Enable Mask */
#define SAMHS_DEV_IER_EORSTES_Pos            3                                              /**< (SAMHS_DEV_IER) End of Reset Interrupt Enable Position */
#define SAMHS_DEV_IER_EORSTES                (_U_(0x1) << SAMHS_DEV_IER_EORSTES_Pos)         /**< (SAMHS_DEV_IER) End of Reset Interrupt Enable Mask */
#define SAMHS_DEV_IER_WAKEUPES_Pos           4                                              /**< (SAMHS_DEV_IER) Wake-Up Interrupt Enable Position */
#define SAMHS_DEV_IER_WAKEUPES               (_U_(0x1) << SAMHS_DEV_IER_WAKEUPES_Pos)        /**< (SAMHS_DEV_IER) Wake-Up Interrupt Enable Mask */
#define SAMHS_DEV_IER_EORSMES_Pos            5                                              /**< (SAMHS_DEV_IER) End of Resume Interrupt Enable Position */
#define SAMHS_DEV_IER_EORSMES                (_U_(0x1) << SAMHS_DEV_IER_EORSMES_Pos)         /**< (SAMHS_DEV_IER) End of Resume Interrupt Enable Mask */
#define SAMHS_DEV_IER_UPRSMES_Pos            6                                              /**< (SAMHS_DEV_IER) Upstream Resume Interrupt Enable Position */
#define SAMHS_DEV_IER_UPRSMES                (_U_(0x1) << SAMHS_DEV_IER_UPRSMES_Pos)         /**< (SAMHS_DEV_IER) Upstream Resume Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_0_Pos              12                                             /**< (SAMHS_DEV_IER) Endpoint 0 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_0                  (_U_(0x1) << SAMHS_DEV_IER_PEP_0_Pos)           /**< (SAMHS_DEV_IER) Endpoint 0 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_1_Pos              13                                             /**< (SAMHS_DEV_IER) Endpoint 1 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_1                  (_U_(0x1) << SAMHS_DEV_IER_PEP_1_Pos)           /**< (SAMHS_DEV_IER) Endpoint 1 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_2_Pos              14                                             /**< (SAMHS_DEV_IER) Endpoint 2 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_2                  (_U_(0x1) << SAMHS_DEV_IER_PEP_2_Pos)           /**< (SAMHS_DEV_IER) Endpoint 2 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_3_Pos              15                                             /**< (SAMHS_DEV_IER) Endpoint 3 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_3                  (_U_(0x1) << SAMHS_DEV_IER_PEP_3_Pos)           /**< (SAMHS_DEV_IER) Endpoint 3 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_4_Pos              16                                             /**< (SAMHS_DEV_IER) Endpoint 4 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_4                  (_U_(0x1) << SAMHS_DEV_IER_PEP_4_Pos)           /**< (SAMHS_DEV_IER) Endpoint 4 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_5_Pos              17                                             /**< (SAMHS_DEV_IER) Endpoint 5 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_5                  (_U_(0x1) << SAMHS_DEV_IER_PEP_5_Pos)           /**< (SAMHS_DEV_IER) Endpoint 5 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_6_Pos              18                                             /**< (SAMHS_DEV_IER) Endpoint 6 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_6                  (_U_(0x1) << SAMHS_DEV_IER_PEP_6_Pos)           /**< (SAMHS_DEV_IER) Endpoint 6 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_7_Pos              19                                             /**< (SAMHS_DEV_IER) Endpoint 7 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_7                  (_U_(0x1) << SAMHS_DEV_IER_PEP_7_Pos)           /**< (SAMHS_DEV_IER) Endpoint 7 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_8_Pos              20                                             /**< (SAMHS_DEV_IER) Endpoint 8 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_8                  (_U_(0x1) << SAMHS_DEV_IER_PEP_8_Pos)           /**< (SAMHS_DEV_IER) Endpoint 8 Interrupt Enable Mask */
#define SAMHS_DEV_IER_PEP_9_Pos              21                                             /**< (SAMHS_DEV_IER) Endpoint 9 Interrupt Enable Position */
#define SAMHS_DEV_IER_PEP_9                  (_U_(0x1) << SAMHS_DEV_IER_PEP_9_Pos)           /**< (SAMHS_DEV_IER) Endpoint 9 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_1_Pos              25                                             /**< (SAMHS_DEV_IER) DMA Channel 1 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_1                  (_U_(0x1) << SAMHS_DEV_IER_DMA_1_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 1 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_2_Pos              26                                             /**< (SAMHS_DEV_IER) DMA Channel 2 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_2                  (_U_(0x1) << SAMHS_DEV_IER_DMA_2_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 2 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_3_Pos              27                                             /**< (SAMHS_DEV_IER) DMA Channel 3 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_3                  (_U_(0x1) << SAMHS_DEV_IER_DMA_3_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 3 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_4_Pos              28                                             /**< (SAMHS_DEV_IER) DMA Channel 4 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_4                  (_U_(0x1) << SAMHS_DEV_IER_DMA_4_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 4 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_5_Pos              29                                             /**< (SAMHS_DEV_IER) DMA Channel 5 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_5                  (_U_(0x1) << SAMHS_DEV_IER_DMA_5_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 5 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_6_Pos              30                                             /**< (SAMHS_DEV_IER) DMA Channel 6 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_6                  (_U_(0x1) << SAMHS_DEV_IER_DMA_6_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 6 Interrupt Enable Mask */
#define SAMHS_DEV_IER_DMA_7_Pos              31                                             /**< (SAMHS_DEV_IER) DMA Channel 7 Interrupt Enable Position */
#define SAMHS_DEV_IER_DMA_7                  (_U_(0x1) << SAMHS_DEV_IER_DMA_7_Pos)           /**< (SAMHS_DEV_IER) DMA Channel 7 Interrupt Enable Mask */
#define SAMHS_DEV_IER_Msk                    _U_(0xFE3FF07F)                                /**< (SAMHS_DEV_IER) Register Mask  */

#define SAMHS_DEV_IER_PEP__Pos               12                                             /**< (SAMHS_DEV_IER Position) Endpoint x Interrupt Enable */
#define SAMHS_DEV_IER_PEP_                   (_U_(0x3FF) << SAMHS_DEV_IER_PEP__Pos)          /**< (SAMHS_DEV_IER Mask) PEP_ */
#define SAMHS_DEV_IER_DMA__Pos               25                                             /**< (SAMHS_DEV_IER Position) DMA Channel 7 Interrupt Enable */
#define SAMHS_DEV_IER_DMA_                   (_U_(0x7F) << SAMHS_DEV_IER_DMA__Pos)           /**< (SAMHS_DEV_IER Mask) DMA_ */

/* -------- SAMHS_DEV_EPT : (USBHS Offset: 0x1c) (R/W 32) Device Endpoint Register -------- */

#define SAMHS_DEV_EPT_OFFSET                 (0x1C)                                        /**<  (SAMHS_DEV_EPT) Device Endpoint Register  Offset */

#define SAMHS_DEV_EPT_EPEN0_Pos              0                                              /**< (SAMHS_DEV_EPT) Endpoint 0 Enable Position */
#define SAMHS_DEV_EPT_EPEN0                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN0_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 0 Enable Mask */
#define SAMHS_DEV_EPT_EPEN1_Pos              1                                              /**< (SAMHS_DEV_EPT) Endpoint 1 Enable Position */
#define SAMHS_DEV_EPT_EPEN1                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN1_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 1 Enable Mask */
#define SAMHS_DEV_EPT_EPEN2_Pos              2                                              /**< (SAMHS_DEV_EPT) Endpoint 2 Enable Position */
#define SAMHS_DEV_EPT_EPEN2                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN2_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 2 Enable Mask */
#define SAMHS_DEV_EPT_EPEN3_Pos              3                                              /**< (SAMHS_DEV_EPT) Endpoint 3 Enable Position */
#define SAMHS_DEV_EPT_EPEN3                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN3_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 3 Enable Mask */
#define SAMHS_DEV_EPT_EPEN4_Pos              4                                              /**< (SAMHS_DEV_EPT) Endpoint 4 Enable Position */
#define SAMHS_DEV_EPT_EPEN4                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN4_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 4 Enable Mask */
#define SAMHS_DEV_EPT_EPEN5_Pos              5                                              /**< (SAMHS_DEV_EPT) Endpoint 5 Enable Position */
#define SAMHS_DEV_EPT_EPEN5                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN5_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 5 Enable Mask */
#define SAMHS_DEV_EPT_EPEN6_Pos              6                                              /**< (SAMHS_DEV_EPT) Endpoint 6 Enable Position */
#define SAMHS_DEV_EPT_EPEN6                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN6_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 6 Enable Mask */
#define SAMHS_DEV_EPT_EPEN7_Pos              7                                              /**< (SAMHS_DEV_EPT) Endpoint 7 Enable Position */
#define SAMHS_DEV_EPT_EPEN7                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN7_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 7 Enable Mask */
#define SAMHS_DEV_EPT_EPEN8_Pos              8                                              /**< (SAMHS_DEV_EPT) Endpoint 8 Enable Position */
#define SAMHS_DEV_EPT_EPEN8                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN8_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 8 Enable Mask */
#define SAMHS_DEV_EPT_EPEN9_Pos              9                                              /**< (SAMHS_DEV_EPT) Endpoint 9 Enable Position */
#define SAMHS_DEV_EPT_EPEN9                  (_U_(0x1) << SAMHS_DEV_EPT_EPEN9_Pos)           /**< (SAMHS_DEV_EPT) Endpoint 9 Enable Mask */
#define SAMHS_DEV_EPT_EPRST0_Pos             16                                             /**< (SAMHS_DEV_EPT) Endpoint 0 Reset Position */
#define SAMHS_DEV_EPT_EPRST0                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST0_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 0 Reset Mask */
#define SAMHS_DEV_EPT_EPRST1_Pos             17                                             /**< (SAMHS_DEV_EPT) Endpoint 1 Reset Position */
#define SAMHS_DEV_EPT_EPRST1                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST1_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 1 Reset Mask */
#define SAMHS_DEV_EPT_EPRST2_Pos             18                                             /**< (SAMHS_DEV_EPT) Endpoint 2 Reset Position */
#define SAMHS_DEV_EPT_EPRST2                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST2_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 2 Reset Mask */
#define SAMHS_DEV_EPT_EPRST3_Pos             19                                             /**< (SAMHS_DEV_EPT) Endpoint 3 Reset Position */
#define SAMHS_DEV_EPT_EPRST3                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST3_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 3 Reset Mask */
#define SAMHS_DEV_EPT_EPRST4_Pos             20                                             /**< (SAMHS_DEV_EPT) Endpoint 4 Reset Position */
#define SAMHS_DEV_EPT_EPRST4                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST4_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 4 Reset Mask */
#define SAMHS_DEV_EPT_EPRST5_Pos             21                                             /**< (SAMHS_DEV_EPT) Endpoint 5 Reset Position */
#define SAMHS_DEV_EPT_EPRST5                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST5_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 5 Reset Mask */
#define SAMHS_DEV_EPT_EPRST6_Pos             22                                             /**< (SAMHS_DEV_EPT) Endpoint 6 Reset Position */
#define SAMHS_DEV_EPT_EPRST6                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST6_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 6 Reset Mask */
#define SAMHS_DEV_EPT_EPRST7_Pos             23                                             /**< (SAMHS_DEV_EPT) Endpoint 7 Reset Position */
#define SAMHS_DEV_EPT_EPRST7                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST7_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 7 Reset Mask */
#define SAMHS_DEV_EPT_EPRST8_Pos             24                                             /**< (SAMHS_DEV_EPT) Endpoint 8 Reset Position */
#define SAMHS_DEV_EPT_EPRST8                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST8_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 8 Reset Mask */
#define SAMHS_DEV_EPT_EPRST9_Pos             25                                             /**< (SAMHS_DEV_EPT) Endpoint 9 Reset Position */
#define SAMHS_DEV_EPT_EPRST9                 (_U_(0x1) << SAMHS_DEV_EPT_EPRST9_Pos)          /**< (SAMHS_DEV_EPT) Endpoint 9 Reset Mask */
#define SAMHS_DEV_EPT_Msk                    _U_(0x3FF03FF)                                 /**< (SAMHS_DEV_EPT) Register Mask  */

#define SAMHS_DEV_EPT_EPEN_Pos               0                                              /**< (SAMHS_DEV_EPT Position) Endpoint x Enable */
#define SAMHS_DEV_EPT_EPEN                   (_U_(0x3FF) << SAMHS_DEV_EPT_EPEN_Pos)          /**< (SAMHS_DEV_EPT Mask) EPEN */
#define SAMHS_DEV_EPT_EPRST_Pos              16                                             /**< (SAMHS_DEV_EPT Position) Endpoint 9 Reset */
#define SAMHS_DEV_EPT_EPRST                  (_U_(0x3FF) << SAMHS_DEV_EPT_EPRST_Pos)         /**< (SAMHS_DEV_EPT Mask) EPRST */

/* -------- SAMHS_DEV_FNUM : (USBHS Offset: 0x20) (R/ 32) Device Frame Number Register -------- */

#define SAMHS_DEV_FNUM_OFFSET                (0x20)                                        /**<  (SAMHS_DEV_FNUM) Device Frame Number Register  Offset */

#define SAMHS_DEV_FNUM_MFNUM_Pos             0                                              /**< (SAMHS_DEV_FNUM) Micro Frame Number Position */
#define SAMHS_DEV_FNUM_MFNUM                 (_U_(0x7) << SAMHS_DEV_FNUM_MFNUM_Pos)          /**< (SAMHS_DEV_FNUM) Micro Frame Number Mask */
#define SAMHS_DEV_FNUM_FNUM_Pos              3                                              /**< (SAMHS_DEV_FNUM) Frame Number Position */
#define SAMHS_DEV_FNUM_FNUM                  (_U_(0x7FF) << SAMHS_DEV_FNUM_FNUM_Pos)         /**< (SAMHS_DEV_FNUM) Frame Number Mask */
#define SAMHS_DEV_FNUM_FNCERR_Pos            15                                             /**< (SAMHS_DEV_FNUM) Frame Number CRC Error Position */
#define SAMHS_DEV_FNUM_FNCERR                (_U_(0x1) << SAMHS_DEV_FNUM_FNCERR_Pos)         /**< (SAMHS_DEV_FNUM) Frame Number CRC Error Mask */
#define SAMHS_DEV_FNUM_Msk                   _U_(0xBFFF)                                    /**< (SAMHS_DEV_FNUM) Register Mask  */


/* -------- SAMHS_DEV_EPTCFG : (USBHS Offset: 0x100) (R/W 32) Device Endpoint Configuration Register -------- */

#define SAMHS_DEV_EPTCFG_OFFSET              (0x100)                                       /**<  (SAMHS_DEV_EPTCFG) Device Endpoint Configuration Register  Offset */

#define SAMHS_DEV_EPTCFG_ALLOC_Pos           1                                              /**< (SAMHS_DEV_EPTCFG) Endpoint Memory Allocate Position */
#define SAMHS_DEV_EPTCFG_ALLOC               (_U_(0x1) << SAMHS_DEV_EPTCFG_ALLOC_Pos)        /**< (SAMHS_DEV_EPTCFG) Endpoint Memory Allocate Mask */
#define SAMHS_DEV_EPTCFG_EPBK_Pos            2                                              /**< (SAMHS_DEV_EPTCFG) Endpoint Banks Position */
#define SAMHS_DEV_EPTCFG_EPBK                (_U_(0x3) << SAMHS_DEV_EPTCFG_EPBK_Pos)         /**< (SAMHS_DEV_EPTCFG) Endpoint Banks Mask */
#define   SAMHS_DEV_EPTCFG_EPBK_1_BANK_Val   _U_(0x0)                                       /**< (SAMHS_DEV_EPTCFG) Single-bank endpoint  */
#define   SAMHS_DEV_EPTCFG_EPBK_2_BANK_Val   _U_(0x1)                                       /**< (SAMHS_DEV_EPTCFG) Double-bank endpoint  */
#define   SAMHS_DEV_EPTCFG_EPBK_3_BANK_Val   _U_(0x2)                                       /**< (SAMHS_DEV_EPTCFG) Triple-bank endpoint  */
#define SAMHS_DEV_EPTCFG_EPBK_1_BANK         (SAMHS_DEV_EPTCFG_EPBK_1_BANK_Val << SAMHS_DEV_EPTCFG_EPBK_Pos)  /**< (SAMHS_DEV_EPTCFG) Single-bank endpoint Position  */
#define SAMHS_DEV_EPTCFG_EPBK_2_BANK         (SAMHS_DEV_EPTCFG_EPBK_2_BANK_Val << SAMHS_DEV_EPTCFG_EPBK_Pos)  /**< (SAMHS_DEV_EPTCFG) Double-bank endpoint Position  */
#define SAMHS_DEV_EPTCFG_EPBK_3_BANK         (SAMHS_DEV_EPTCFG_EPBK_3_BANK_Val << SAMHS_DEV_EPTCFG_EPBK_Pos)  /**< (SAMHS_DEV_EPTCFG) Triple-bank endpoint Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_Pos          4                                              /**< (SAMHS_DEV_EPTCFG) Endpoint Size Position */
#define SAMHS_DEV_EPTCFG_EPSIZE              (_U_(0x7) << SAMHS_DEV_EPTCFG_EPSIZE_Pos)       /**< (SAMHS_DEV_EPTCFG) Endpoint Size Mask */
#define   SAMHS_DEV_EPTCFG_EPSIZE_8_BYTE_Val _U_(0x0)                                       /**< (SAMHS_DEV_EPTCFG) 8 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_16_BYTE_Val _U_(0x1)                                       /**< (SAMHS_DEV_EPTCFG) 16 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_32_BYTE_Val _U_(0x2)                                       /**< (SAMHS_DEV_EPTCFG) 32 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_64_BYTE_Val _U_(0x3)                                       /**< (SAMHS_DEV_EPTCFG) 64 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_128_BYTE_Val _U_(0x4)                                       /**< (SAMHS_DEV_EPTCFG) 128 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_256_BYTE_Val _U_(0x5)                                       /**< (SAMHS_DEV_EPTCFG) 256 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_512_BYTE_Val _U_(0x6)                                       /**< (SAMHS_DEV_EPTCFG) 512 bytes  */
#define   SAMHS_DEV_EPTCFG_EPSIZE_1024_BYTE_Val _U_(0x7)                                       /**< (SAMHS_DEV_EPTCFG) 1024 bytes  */
#define SAMHS_DEV_EPTCFG_EPSIZE_8_BYTE       (SAMHS_DEV_EPTCFG_EPSIZE_8_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 8 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_16_BYTE      (SAMHS_DEV_EPTCFG_EPSIZE_16_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 16 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_32_BYTE      (SAMHS_DEV_EPTCFG_EPSIZE_32_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 32 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_64_BYTE      (SAMHS_DEV_EPTCFG_EPSIZE_64_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 64 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_128_BYTE     (SAMHS_DEV_EPTCFG_EPSIZE_128_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 128 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_256_BYTE     (SAMHS_DEV_EPTCFG_EPSIZE_256_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 256 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_512_BYTE     (SAMHS_DEV_EPTCFG_EPSIZE_512_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 512 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPSIZE_1024_BYTE    (SAMHS_DEV_EPTCFG_EPSIZE_1024_BYTE_Val << SAMHS_DEV_EPTCFG_EPSIZE_Pos)  /**< (SAMHS_DEV_EPTCFG) 1024 bytes Position  */
#define SAMHS_DEV_EPTCFG_EPDIR_Pos           8                                              /**< (SAMHS_DEV_EPTCFG) Endpoint Direction Position */
#define SAMHS_DEV_EPTCFG_EPDIR               (_U_(0x1) << SAMHS_DEV_EPTCFG_EPDIR_Pos)        /**< (SAMHS_DEV_EPTCFG) Endpoint Direction Mask */
#define   SAMHS_DEV_EPTCFG_EPDIR_OUT_Val     _U_(0x0)                                       /**< (SAMHS_DEV_EPTCFG) The endpoint direction is OUT.  */
#define   SAMHS_DEV_EPTCFG_EPDIR_IN_Val      _U_(0x1)                                       /**< (SAMHS_DEV_EPTCFG) The endpoint direction is IN (nor for control endpoints).  */
#define SAMHS_DEV_EPTCFG_EPDIR_OUT           (SAMHS_DEV_EPTCFG_EPDIR_OUT_Val << SAMHS_DEV_EPTCFG_EPDIR_Pos)  /**< (SAMHS_DEV_EPTCFG) The endpoint direction is OUT. Position  */
#define SAMHS_DEV_EPTCFG_EPDIR_IN            (SAMHS_DEV_EPTCFG_EPDIR_IN_Val << SAMHS_DEV_EPTCFG_EPDIR_Pos)  /**< (SAMHS_DEV_EPTCFG) The endpoint direction is IN (nor for control endpoints). Position  */
#define SAMHS_DEV_EPTCFG_AUTOSW_Pos          9                                              /**< (SAMHS_DEV_EPTCFG) Automatic Switch Position */
#define SAMHS_DEV_EPTCFG_AUTOSW              (_U_(0x1) << SAMHS_DEV_EPTCFG_AUTOSW_Pos)       /**< (SAMHS_DEV_EPTCFG) Automatic Switch Mask */
#define SAMHS_DEV_EPTCFG_EPTYPE_Pos          11                                             /**< (SAMHS_DEV_EPTCFG) Endpoint Type Position */
#define SAMHS_DEV_EPTCFG_EPTYPE              (_U_(0x3) << SAMHS_DEV_EPTCFG_EPTYPE_Pos)       /**< (SAMHS_DEV_EPTCFG) Endpoint Type Mask */
#define   SAMHS_DEV_EPTCFG_EPTYPE_CTRL_Val   _U_(0x0)                                       /**< (SAMHS_DEV_EPTCFG) Control  */
#define   SAMHS_DEV_EPTCFG_EPTYPE_ISO_Val    _U_(0x1)                                       /**< (SAMHS_DEV_EPTCFG) Isochronous  */
#define   SAMHS_DEV_EPTCFG_EPTYPE_BLK_Val    _U_(0x2)                                       /**< (SAMHS_DEV_EPTCFG) Bulk  */
#define   SAMHS_DEV_EPTCFG_EPTYPE_INTRPT_Val _U_(0x3)                                       /**< (SAMHS_DEV_EPTCFG) Interrupt  */
#define SAMHS_DEV_EPTCFG_EPTYPE_CTRL         (SAMHS_DEV_EPTCFG_EPTYPE_CTRL_Val << SAMHS_DEV_EPTCFG_EPTYPE_Pos)  /**< (SAMHS_DEV_EPTCFG) Control Position  */
#define SAMHS_DEV_EPTCFG_EPTYPE_ISO          (SAMHS_DEV_EPTCFG_EPTYPE_ISO_Val << SAMHS_DEV_EPTCFG_EPTYPE_Pos)  /**< (SAMHS_DEV_EPTCFG) Isochronous Position  */
#define SAMHS_DEV_EPTCFG_EPTYPE_BLK          (SAMHS_DEV_EPTCFG_EPTYPE_BLK_Val << SAMHS_DEV_EPTCFG_EPTYPE_Pos)  /**< (SAMHS_DEV_EPTCFG) Bulk Position  */
#define SAMHS_DEV_EPTCFG_EPTYPE_INTRPT       (SAMHS_DEV_EPTCFG_EPTYPE_INTRPT_Val << SAMHS_DEV_EPTCFG_EPTYPE_Pos)  /**< (SAMHS_DEV_EPTCFG) Interrupt Position  */
#define SAMHS_DEV_EPTCFG_NBTRANS_Pos         13                                             /**< (SAMHS_DEV_EPTCFG) Number of transactions per microframe for isochronous endpoint Position */
#define SAMHS_DEV_EPTCFG_NBTRANS             (_U_(0x3) << SAMHS_DEV_EPTCFG_NBTRANS_Pos)      /**< (SAMHS_DEV_EPTCFG) Number of transactions per microframe for isochronous endpoint Mask */
#define   SAMHS_DEV_EPTCFG_NBTRANS_0_TRANS_Val _U_(0x0)                                       /**< (SAMHS_DEV_EPTCFG) Reserved to endpoint that does not have the high-bandwidth isochronous capability.  */
#define   SAMHS_DEV_EPTCFG_NBTRANS_1_TRANS_Val _U_(0x1)                                       /**< (SAMHS_DEV_EPTCFG) Default value: one transaction per microframe.  */
#define   SAMHS_DEV_EPTCFG_NBTRANS_2_TRANS_Val _U_(0x2)                                       /**< (SAMHS_DEV_EPTCFG) Two transactions per microframe. This endpoint should be configured as double-bank.  */
#define   SAMHS_DEV_EPTCFG_NBTRANS_3_TRANS_Val _U_(0x3)                                       /**< (SAMHS_DEV_EPTCFG) Three transactions per microframe. This endpoint should be configured as triple-bank.  */
#define SAMHS_DEV_EPTCFG_NBTRANS_0_TRANS     (SAMHS_DEV_EPTCFG_NBTRANS_0_TRANS_Val << SAMHS_DEV_EPTCFG_NBTRANS_Pos)  /**< (SAMHS_DEV_EPTCFG) Reserved to endpoint that does not have the high-bandwidth isochronous capability. Position  */
#define SAMHS_DEV_EPTCFG_NBTRANS_1_TRANS     (SAMHS_DEV_EPTCFG_NBTRANS_1_TRANS_Val << SAMHS_DEV_EPTCFG_NBTRANS_Pos)  /**< (SAMHS_DEV_EPTCFG) Default value: one transaction per microframe. Position  */
#define SAMHS_DEV_EPTCFG_NBTRANS_2_TRANS     (SAMHS_DEV_EPTCFG_NBTRANS_2_TRANS_Val << SAMHS_DEV_EPTCFG_NBTRANS_Pos)  /**< (SAMHS_DEV_EPTCFG) Two transactions per microframe. This endpoint should be configured as double-bank. Position  */
#define SAMHS_DEV_EPTCFG_NBTRANS_3_TRANS     (SAMHS_DEV_EPTCFG_NBTRANS_3_TRANS_Val << SAMHS_DEV_EPTCFG_NBTRANS_Pos)  /**< (SAMHS_DEV_EPTCFG) Three transactions per microframe. This endpoint should be configured as triple-bank. Position  */
#define SAMHS_DEV_EPTCFG_Msk                 _U_(0x7B7E)                                    /**< (SAMHS_DEV_EPTCFG) Register Mask  */


/* -------- SAMHS_DEV_EPTISR : (USBHS Offset: 0x130) (R/ 32) Device Endpoint Interrupt Status Register -------- */

#define SAMHS_DEV_EPTISR_OFFSET              (0x130)                                       /**<  (SAMHS_DEV_EPTISR) Device Endpoint Interrupt Status Register  Offset */

#define SAMHS_DEV_EPTISR_TXINI_Pos           0                                              /**< (SAMHS_DEV_EPTISR) Transmitted IN Data Interrupt Position */
#define SAMHS_DEV_EPTISR_TXINI               (_U_(0x1) << SAMHS_DEV_EPTISR_TXINI_Pos)        /**< (SAMHS_DEV_EPTISR) Transmitted IN Data Interrupt Mask */
#define SAMHS_DEV_EPTISR_RXOUTI_Pos          1                                              /**< (SAMHS_DEV_EPTISR) Received OUT Data Interrupt Position */
#define SAMHS_DEV_EPTISR_RXOUTI              (_U_(0x1) << SAMHS_DEV_EPTISR_RXOUTI_Pos)       /**< (SAMHS_DEV_EPTISR) Received OUT Data Interrupt Mask */
#define SAMHS_DEV_EPTISR_OVERFI_Pos          5                                              /**< (SAMHS_DEV_EPTISR) Overflow Interrupt Position */
#define SAMHS_DEV_EPTISR_OVERFI              (_U_(0x1) << SAMHS_DEV_EPTISR_OVERFI_Pos)       /**< (SAMHS_DEV_EPTISR) Overflow Interrupt Mask */
#define SAMHS_DEV_EPTISR_SHORTPACKET_Pos     7                                              /**< (SAMHS_DEV_EPTISR) Short Packet Interrupt Position */
#define SAMHS_DEV_EPTISR_SHORTPACKET         (_U_(0x1) << SAMHS_DEV_EPTISR_SHORTPACKET_Pos)  /**< (SAMHS_DEV_EPTISR) Short Packet Interrupt Mask */
#define SAMHS_DEV_EPTISR_DTSEQ_Pos           8                                              /**< (SAMHS_DEV_EPTISR) Data Toggle Sequence Position */
#define SAMHS_DEV_EPTISR_DTSEQ               (_U_(0x3) << SAMHS_DEV_EPTISR_DTSEQ_Pos)        /**< (SAMHS_DEV_EPTISR) Data Toggle Sequence Mask */
#define   SAMHS_DEV_EPTISR_DTSEQ_DATA0_Val   _U_(0x0)                                       /**< (SAMHS_DEV_EPTISR) Data0 toggle sequence  */
#define   SAMHS_DEV_EPTISR_DTSEQ_DATA1_Val   _U_(0x1)                                       /**< (SAMHS_DEV_EPTISR) Data1 toggle sequence  */
#define   SAMHS_DEV_EPTISR_DTSEQ_DATA2_Val   _U_(0x2)                                       /**< (SAMHS_DEV_EPTISR) Reserved for high-bandwidth isochronous endpoint  */
#define   SAMHS_DEV_EPTISR_DTSEQ_MDATA_Val   _U_(0x3)                                       /**< (SAMHS_DEV_EPTISR) Reserved for high-bandwidth isochronous endpoint  */
#define SAMHS_DEV_EPTISR_DTSEQ_DATA0         (SAMHS_DEV_EPTISR_DTSEQ_DATA0_Val << SAMHS_DEV_EPTISR_DTSEQ_Pos)  /**< (SAMHS_DEV_EPTISR) Data0 toggle sequence Position  */
#define SAMHS_DEV_EPTISR_DTSEQ_DATA1         (SAMHS_DEV_EPTISR_DTSEQ_DATA1_Val << SAMHS_DEV_EPTISR_DTSEQ_Pos)  /**< (SAMHS_DEV_EPTISR) Data1 toggle sequence Position  */
#define SAMHS_DEV_EPTISR_DTSEQ_DATA2         (SAMHS_DEV_EPTISR_DTSEQ_DATA2_Val << SAMHS_DEV_EPTISR_DTSEQ_Pos)  /**< (SAMHS_DEV_EPTISR) Reserved for high-bandwidth isochronous endpoint Position  */
#define SAMHS_DEV_EPTISR_DTSEQ_MDATA         (SAMHS_DEV_EPTISR_DTSEQ_MDATA_Val << SAMHS_DEV_EPTISR_DTSEQ_Pos)  /**< (SAMHS_DEV_EPTISR) Reserved for high-bandwidth isochronous endpoint Position  */
#define SAMHS_DEV_EPTISR_NBUSYBK_Pos         12                                             /**< (SAMHS_DEV_EPTISR) Number of Busy Banks Position */
#define SAMHS_DEV_EPTISR_NBUSYBK             (_U_(0x3) << SAMHS_DEV_EPTISR_NBUSYBK_Pos)      /**< (SAMHS_DEV_EPTISR) Number of Busy Banks Mask */
#define   SAMHS_DEV_EPTISR_NBUSYBK_0_BUSY_Val _U_(0x0)                                       /**< (SAMHS_DEV_EPTISR) 0 busy bank (all banks free)  */
#define   SAMHS_DEV_EPTISR_NBUSYBK_1_BUSY_Val _U_(0x1)                                       /**< (SAMHS_DEV_EPTISR) 1 busy bank  */
#define   SAMHS_DEV_EPTISR_NBUSYBK_2_BUSY_Val _U_(0x2)                                       /**< (SAMHS_DEV_EPTISR) 2 busy banks  */
#define   SAMHS_DEV_EPTISR_NBUSYBK_3_BUSY_Val _U_(0x3)                                       /**< (SAMHS_DEV_EPTISR) 3 busy banks  */
#define SAMHS_DEV_EPTISR_NBUSYBK_0_BUSY      (SAMHS_DEV_EPTISR_NBUSYBK_0_BUSY_Val << SAMHS_DEV_EPTISR_NBUSYBK_Pos)  /**< (SAMHS_DEV_EPTISR) 0 busy bank (all banks free) Position  */
#define SAMHS_DEV_EPTISR_NBUSYBK_1_BUSY      (SAMHS_DEV_EPTISR_NBUSYBK_1_BUSY_Val << SAMHS_DEV_EPTISR_NBUSYBK_Pos)  /**< (SAMHS_DEV_EPTISR) 1 busy bank Position  */
#define SAMHS_DEV_EPTISR_NBUSYBK_2_BUSY      (SAMHS_DEV_EPTISR_NBUSYBK_2_BUSY_Val << SAMHS_DEV_EPTISR_NBUSYBK_Pos)  /**< (SAMHS_DEV_EPTISR) 2 busy banks Position  */
#define SAMHS_DEV_EPTISR_NBUSYBK_3_BUSY      (SAMHS_DEV_EPTISR_NBUSYBK_3_BUSY_Val << SAMHS_DEV_EPTISR_NBUSYBK_Pos)  /**< (SAMHS_DEV_EPTISR) 3 busy banks Position  */
#define SAMHS_DEV_EPTISR_CURRBK_Pos          14                                             /**< (SAMHS_DEV_EPTISR) Current Bank Position */
#define SAMHS_DEV_EPTISR_CURRBK              (_U_(0x3) << SAMHS_DEV_EPTISR_CURRBK_Pos)       /**< (SAMHS_DEV_EPTISR) Current Bank Mask */
#define   SAMHS_DEV_EPTISR_CURRBK_BANK0_Val  _U_(0x0)                                       /**< (SAMHS_DEV_EPTISR) Current bank is bank0  */
#define   SAMHS_DEV_EPTISR_CURRBK_BANK1_Val  _U_(0x1)                                       /**< (SAMHS_DEV_EPTISR) Current bank is bank1  */
#define   SAMHS_DEV_EPTISR_CURRBK_BANK2_Val  _U_(0x2)                                       /**< (SAMHS_DEV_EPTISR) Current bank is bank2  */
#define SAMHS_DEV_EPTISR_CURRBK_BANK0        (SAMHS_DEV_EPTISR_CURRBK_BANK0_Val << SAMHS_DEV_EPTISR_CURRBK_Pos)  /**< (SAMHS_DEV_EPTISR) Current bank is bank0 Position  */
#define SAMHS_DEV_EPTISR_CURRBK_BANK1        (SAMHS_DEV_EPTISR_CURRBK_BANK1_Val << SAMHS_DEV_EPTISR_CURRBK_Pos)  /**< (SAMHS_DEV_EPTISR) Current bank is bank1 Position  */
#define SAMHS_DEV_EPTISR_CURRBK_BANK2        (SAMHS_DEV_EPTISR_CURRBK_BANK2_Val << SAMHS_DEV_EPTISR_CURRBK_Pos)  /**< (SAMHS_DEV_EPTISR) Current bank is bank2 Position  */
#define SAMHS_DEV_EPTISR_RWALL_Pos           16                                             /**< (SAMHS_DEV_EPTISR) Read/Write Allowed Position */
#define SAMHS_DEV_EPTISR_RWALL               (_U_(0x1) << SAMHS_DEV_EPTISR_RWALL_Pos)        /**< (SAMHS_DEV_EPTISR) Read/Write Allowed Mask */
#define SAMHS_DEV_EPTISR_CFGOK_Pos           18                                             /**< (SAMHS_DEV_EPTISR) Configuration OK Status Position */
#define SAMHS_DEV_EPTISR_CFGOK               (_U_(0x1) << SAMHS_DEV_EPTISR_CFGOK_Pos)        /**< (SAMHS_DEV_EPTISR) Configuration OK Status Mask */
#define SAMHS_DEV_EPTISR_BYCT_Pos            20                                             /**< (SAMHS_DEV_EPTISR) Byte Count Position */
#define SAMHS_DEV_EPTISR_BYCT                (_U_(0x7FF) << SAMHS_DEV_EPTISR_BYCT_Pos)       /**< (SAMHS_DEV_EPTISR) Byte Count Mask */
#define SAMHS_DEV_EPTISR_Msk                 _U_(0x7FF5F3A3)                                /**< (SAMHS_DEV_EPTISR) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTISR_CTRL_RXSTPI_Pos     2                                              /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTISR_CTRL_RXSTPI         (_U_(0x1) << SAMHS_DEV_EPTISR_CTRL_RXSTPI_Pos)  /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTISR_CTRL_NAKOUTI_Pos    3                                              /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTISR_CTRL_NAKOUTI        (_U_(0x1) << SAMHS_DEV_EPTISR_CTRL_NAKOUTI_Pos)  /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTISR_CTRL_NAKINI_Pos     4                                              /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTISR_CTRL_NAKINI         (_U_(0x1) << SAMHS_DEV_EPTISR_CTRL_NAKINI_Pos)  /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTISR_CTRL_STALLEDI_Pos   6                                              /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTISR_CTRL_STALLEDI       (_U_(0x1) << SAMHS_DEV_EPTISR_CTRL_STALLEDI_Pos)  /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTISR_CTRL_CTRLDIR_Pos    17                                             /**< (SAMHS_DEV_EPTISR) Control Direction Position */
#define SAMHS_DEV_EPTISR_CTRL_CTRLDIR        (_U_(0x1) << SAMHS_DEV_EPTISR_CTRL_CTRLDIR_Pos)  /**< (SAMHS_DEV_EPTISR) Control Direction Mask */
#define SAMHS_DEV_EPTISR_CTRL_Msk            _U_(0x2005C)                                   /**< (SAMHS_DEV_EPTISR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTISR_ISO_UNDERFI_Pos     2                                              /**< (SAMHS_DEV_EPTISR) Underflow Interrupt Position */
#define SAMHS_DEV_EPTISR_ISO_UNDERFI         (_U_(0x1) << SAMHS_DEV_EPTISR_ISO_UNDERFI_Pos)  /**< (SAMHS_DEV_EPTISR) Underflow Interrupt Mask */
#define SAMHS_DEV_EPTISR_ISO_HBISOINERRI_Pos 3                                              /**< (SAMHS_DEV_EPTISR) High Bandwidth Isochronous IN Underflow Error Interrupt Position */
#define SAMHS_DEV_EPTISR_ISO_HBISOINERRI     (_U_(0x1) << SAMHS_DEV_EPTISR_ISO_HBISOINERRI_Pos)  /**< (SAMHS_DEV_EPTISR) High Bandwidth Isochronous IN Underflow Error Interrupt Mask */
#define SAMHS_DEV_EPTISR_ISO_HBISOFLUSHI_Pos 4                                              /**< (SAMHS_DEV_EPTISR) High Bandwidth Isochronous IN Flush Interrupt Position */
#define SAMHS_DEV_EPTISR_ISO_HBISOFLUSHI     (_U_(0x1) << SAMHS_DEV_EPTISR_ISO_HBISOFLUSHI_Pos)  /**< (SAMHS_DEV_EPTISR) High Bandwidth Isochronous IN Flush Interrupt Mask */
#define SAMHS_DEV_EPTISR_ISO_CRCERRI_Pos     6                                              /**< (SAMHS_DEV_EPTISR) CRC Error Interrupt Position */
#define SAMHS_DEV_EPTISR_ISO_CRCERRI         (_U_(0x1) << SAMHS_DEV_EPTISR_ISO_CRCERRI_Pos)  /**< (SAMHS_DEV_EPTISR) CRC Error Interrupt Mask */
#define SAMHS_DEV_EPTISR_ISO_ERRORTRANS_Pos  10                                             /**< (SAMHS_DEV_EPTISR) High-bandwidth Isochronous OUT Endpoint Transaction Error Interrupt Position */
#define SAMHS_DEV_EPTISR_ISO_ERRORTRANS      (_U_(0x1) << SAMHS_DEV_EPTISR_ISO_ERRORTRANS_Pos)  /**< (SAMHS_DEV_EPTISR) High-bandwidth Isochronous OUT Endpoint Transaction Error Interrupt Mask */
#define SAMHS_DEV_EPTISR_ISO_Msk             _U_(0x45C)                                     /**< (SAMHS_DEV_EPTISR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTISR_BLK_RXSTPI_Pos      2                                              /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTISR_BLK_RXSTPI          (_U_(0x1) << SAMHS_DEV_EPTISR_BLK_RXSTPI_Pos)   /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTISR_BLK_NAKOUTI_Pos     3                                              /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTISR_BLK_NAKOUTI         (_U_(0x1) << SAMHS_DEV_EPTISR_BLK_NAKOUTI_Pos)  /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTISR_BLK_NAKINI_Pos      4                                              /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTISR_BLK_NAKINI          (_U_(0x1) << SAMHS_DEV_EPTISR_BLK_NAKINI_Pos)   /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTISR_BLK_STALLEDI_Pos    6                                              /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTISR_BLK_STALLEDI        (_U_(0x1) << SAMHS_DEV_EPTISR_BLK_STALLEDI_Pos)  /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTISR_BLK_CTRLDIR_Pos     17                                             /**< (SAMHS_DEV_EPTISR) Control Direction Position */
#define SAMHS_DEV_EPTISR_BLK_CTRLDIR         (_U_(0x1) << SAMHS_DEV_EPTISR_BLK_CTRLDIR_Pos)  /**< (SAMHS_DEV_EPTISR) Control Direction Mask */
#define SAMHS_DEV_EPTISR_BLK_Msk             _U_(0x2005C)                                   /**< (SAMHS_DEV_EPTISR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTISR_INTRPT_RXSTPI_Pos   2                                              /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTISR_INTRPT_RXSTPI       (_U_(0x1) << SAMHS_DEV_EPTISR_INTRPT_RXSTPI_Pos)  /**< (SAMHS_DEV_EPTISR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTISR_INTRPT_NAKOUTI_Pos  3                                              /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTISR_INTRPT_NAKOUTI      (_U_(0x1) << SAMHS_DEV_EPTISR_INTRPT_NAKOUTI_Pos)  /**< (SAMHS_DEV_EPTISR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTISR_INTRPT_NAKINI_Pos   4                                              /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTISR_INTRPT_NAKINI       (_U_(0x1) << SAMHS_DEV_EPTISR_INTRPT_NAKINI_Pos)  /**< (SAMHS_DEV_EPTISR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTISR_INTRPT_STALLEDI_Pos 6                                              /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTISR_INTRPT_STALLEDI     (_U_(0x1) << SAMHS_DEV_EPTISR_INTRPT_STALLEDI_Pos)  /**< (SAMHS_DEV_EPTISR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTISR_INTRPT_CTRLDIR_Pos  17                                             /**< (SAMHS_DEV_EPTISR) Control Direction Position */
#define SAMHS_DEV_EPTISR_INTRPT_CTRLDIR      (_U_(0x1) << SAMHS_DEV_EPTISR_INTRPT_CTRLDIR_Pos)  /**< (SAMHS_DEV_EPTISR) Control Direction Mask */
#define SAMHS_DEV_EPTISR_INTRPT_Msk          _U_(0x2005C)                                   /**< (SAMHS_DEV_EPTISR_INTRPT) Register Mask  */


/* -------- SAMHS_DEV_EPTICR : (USBHS Offset: 0x160) (/W 32) Device Endpoint Interrupt Clear Register -------- */

#define SAMHS_DEV_EPTICR_OFFSET              (0x160)                                       /**<  (SAMHS_DEV_EPTICR) Device Endpoint Interrupt Clear Register  Offset */

#define SAMHS_DEV_EPTICR_TXINIC_Pos          0                                              /**< (SAMHS_DEV_EPTICR) Transmitted IN Data Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_TXINIC              (_U_(0x1) << SAMHS_DEV_EPTICR_TXINIC_Pos)       /**< (SAMHS_DEV_EPTICR) Transmitted IN Data Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_RXOUTIC_Pos         1                                              /**< (SAMHS_DEV_EPTICR) Received OUT Data Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_RXOUTIC             (_U_(0x1) << SAMHS_DEV_EPTICR_RXOUTIC_Pos)      /**< (SAMHS_DEV_EPTICR) Received OUT Data Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_OVERFIC_Pos         5                                              /**< (SAMHS_DEV_EPTICR) Overflow Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_OVERFIC             (_U_(0x1) << SAMHS_DEV_EPTICR_OVERFIC_Pos)      /**< (SAMHS_DEV_EPTICR) Overflow Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_SHORTPACKETC_Pos    7                                              /**< (SAMHS_DEV_EPTICR) Short Packet Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_SHORTPACKETC        (_U_(0x1) << SAMHS_DEV_EPTICR_SHORTPACKETC_Pos)  /**< (SAMHS_DEV_EPTICR) Short Packet Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_Msk                 _U_(0xA3)                                      /**< (SAMHS_DEV_EPTICR) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTICR_CTRL_RXSTPIC_Pos    2                                              /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_CTRL_RXSTPIC        (_U_(0x1) << SAMHS_DEV_EPTICR_CTRL_RXSTPIC_Pos)  /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_CTRL_NAKOUTIC_Pos   3                                              /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_CTRL_NAKOUTIC       (_U_(0x1) << SAMHS_DEV_EPTICR_CTRL_NAKOUTIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_CTRL_NAKINIC_Pos    4                                              /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_CTRL_NAKINIC        (_U_(0x1) << SAMHS_DEV_EPTICR_CTRL_NAKINIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_CTRL_STALLEDIC_Pos  6                                              /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_CTRL_STALLEDIC      (_U_(0x1) << SAMHS_DEV_EPTICR_CTRL_STALLEDIC_Pos)  /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_CTRL_Msk            _U_(0x5C)                                      /**< (SAMHS_DEV_EPTICR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTICR_ISO_UNDERFIC_Pos    2                                              /**< (SAMHS_DEV_EPTICR) Underflow Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_ISO_UNDERFIC        (_U_(0x1) << SAMHS_DEV_EPTICR_ISO_UNDERFIC_Pos)  /**< (SAMHS_DEV_EPTICR) Underflow Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_ISO_HBISOINERRIC_Pos 3                                              /**< (SAMHS_DEV_EPTICR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_ISO_HBISOINERRIC     (_U_(0x1) << SAMHS_DEV_EPTICR_ISO_HBISOINERRIC_Pos)  /**< (SAMHS_DEV_EPTICR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_ISO_HBISOFLUSHIC_Pos 4                                              /**< (SAMHS_DEV_EPTICR) High Bandwidth Isochronous IN Flush Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_ISO_HBISOFLUSHIC     (_U_(0x1) << SAMHS_DEV_EPTICR_ISO_HBISOFLUSHIC_Pos)  /**< (SAMHS_DEV_EPTICR) High Bandwidth Isochronous IN Flush Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_ISO_CRCERRIC_Pos    6                                              /**< (SAMHS_DEV_EPTICR) CRC Error Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_ISO_CRCERRIC        (_U_(0x1) << SAMHS_DEV_EPTICR_ISO_CRCERRIC_Pos)  /**< (SAMHS_DEV_EPTICR) CRC Error Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_ISO_Msk             _U_(0x5C)                                      /**< (SAMHS_DEV_EPTICR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTICR_BLK_RXSTPIC_Pos     2                                              /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_BLK_RXSTPIC         (_U_(0x1) << SAMHS_DEV_EPTICR_BLK_RXSTPIC_Pos)  /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_BLK_NAKOUTIC_Pos    3                                              /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_BLK_NAKOUTIC        (_U_(0x1) << SAMHS_DEV_EPTICR_BLK_NAKOUTIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_BLK_NAKINIC_Pos     4                                              /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_BLK_NAKINIC         (_U_(0x1) << SAMHS_DEV_EPTICR_BLK_NAKINIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_BLK_STALLEDIC_Pos   6                                              /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_BLK_STALLEDIC       (_U_(0x1) << SAMHS_DEV_EPTICR_BLK_STALLEDIC_Pos)  /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_BLK_Msk             _U_(0x5C)                                      /**< (SAMHS_DEV_EPTICR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTICR_INTRPT_RXSTPIC_Pos  2                                              /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_INTRPT_RXSTPIC      (_U_(0x1) << SAMHS_DEV_EPTICR_INTRPT_RXSTPIC_Pos)  /**< (SAMHS_DEV_EPTICR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_INTRPT_NAKOUTIC_Pos 3                                              /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_INTRPT_NAKOUTIC     (_U_(0x1) << SAMHS_DEV_EPTICR_INTRPT_NAKOUTIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_INTRPT_NAKINIC_Pos  4                                              /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_INTRPT_NAKINIC      (_U_(0x1) << SAMHS_DEV_EPTICR_INTRPT_NAKINIC_Pos)  /**< (SAMHS_DEV_EPTICR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_INTRPT_STALLEDIC_Pos 6                                              /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTICR_INTRPT_STALLEDIC     (_U_(0x1) << SAMHS_DEV_EPTICR_INTRPT_STALLEDIC_Pos)  /**< (SAMHS_DEV_EPTICR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTICR_INTRPT_Msk          _U_(0x5C)                                      /**< (SAMHS_DEV_EPTICR_INTRPT) Register Mask  */


/* -------- SAMHS_DEV_EPTIFR : (USBHS Offset: 0x190) (/W 32) Device Endpoint Interrupt Set Register -------- */

#define SAMHS_DEV_EPTIFR_OFFSET              (0x190)                                       /**<  (SAMHS_DEV_EPTIFR) Device Endpoint Interrupt Set Register  Offset */

#define SAMHS_DEV_EPTIFR_TXINIS_Pos          0                                              /**< (SAMHS_DEV_EPTIFR) Transmitted IN Data Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_TXINIS              (_U_(0x1) << SAMHS_DEV_EPTIFR_TXINIS_Pos)       /**< (SAMHS_DEV_EPTIFR) Transmitted IN Data Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_RXOUTIS_Pos         1                                              /**< (SAMHS_DEV_EPTIFR) Received OUT Data Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_RXOUTIS             (_U_(0x1) << SAMHS_DEV_EPTIFR_RXOUTIS_Pos)      /**< (SAMHS_DEV_EPTIFR) Received OUT Data Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_OVERFIS_Pos         5                                              /**< (SAMHS_DEV_EPTIFR) Overflow Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_OVERFIS             (_U_(0x1) << SAMHS_DEV_EPTIFR_OVERFIS_Pos)      /**< (SAMHS_DEV_EPTIFR) Overflow Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_SHORTPACKETS_Pos    7                                              /**< (SAMHS_DEV_EPTIFR) Short Packet Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_SHORTPACKETS        (_U_(0x1) << SAMHS_DEV_EPTIFR_SHORTPACKETS_Pos)  /**< (SAMHS_DEV_EPTIFR) Short Packet Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_NBUSYBKS_Pos        12                                             /**< (SAMHS_DEV_EPTIFR) Number of Busy Banks Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_NBUSYBKS            (_U_(0x1) << SAMHS_DEV_EPTIFR_NBUSYBKS_Pos)     /**< (SAMHS_DEV_EPTIFR) Number of Busy Banks Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_Msk                 _U_(0x10A3)                                    /**< (SAMHS_DEV_EPTIFR) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTIFR_CTRL_RXSTPIS_Pos    2                                              /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_CTRL_RXSTPIS        (_U_(0x1) << SAMHS_DEV_EPTIFR_CTRL_RXSTPIS_Pos)  /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_CTRL_NAKOUTIS_Pos   3                                              /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_CTRL_NAKOUTIS       (_U_(0x1) << SAMHS_DEV_EPTIFR_CTRL_NAKOUTIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_CTRL_NAKINIS_Pos    4                                              /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_CTRL_NAKINIS        (_U_(0x1) << SAMHS_DEV_EPTIFR_CTRL_NAKINIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_CTRL_STALLEDIS_Pos  6                                              /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_CTRL_STALLEDIS      (_U_(0x1) << SAMHS_DEV_EPTIFR_CTRL_STALLEDIS_Pos)  /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_CTRL_Msk            _U_(0x5C)                                      /**< (SAMHS_DEV_EPTIFR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTIFR_ISO_UNDERFIS_Pos    2                                              /**< (SAMHS_DEV_EPTIFR) Underflow Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_ISO_UNDERFIS        (_U_(0x1) << SAMHS_DEV_EPTIFR_ISO_UNDERFIS_Pos)  /**< (SAMHS_DEV_EPTIFR) Underflow Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_ISO_HBISOINERRIS_Pos 3                                              /**< (SAMHS_DEV_EPTIFR) High Bandwidth Isochronous IN Underflow Error Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_ISO_HBISOINERRIS     (_U_(0x1) << SAMHS_DEV_EPTIFR_ISO_HBISOINERRIS_Pos)  /**< (SAMHS_DEV_EPTIFR) High Bandwidth Isochronous IN Underflow Error Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_ISO_HBISOFLUSHIS_Pos 4                                              /**< (SAMHS_DEV_EPTIFR) High Bandwidth Isochronous IN Flush Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_ISO_HBISOFLUSHIS     (_U_(0x1) << SAMHS_DEV_EPTIFR_ISO_HBISOFLUSHIS_Pos)  /**< (SAMHS_DEV_EPTIFR) High Bandwidth Isochronous IN Flush Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_ISO_CRCERRIS_Pos    6                                              /**< (SAMHS_DEV_EPTIFR) CRC Error Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_ISO_CRCERRIS        (_U_(0x1) << SAMHS_DEV_EPTIFR_ISO_CRCERRIS_Pos)  /**< (SAMHS_DEV_EPTIFR) CRC Error Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_ISO_Msk             _U_(0x5C)                                      /**< (SAMHS_DEV_EPTIFR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTIFR_BLK_RXSTPIS_Pos     2                                              /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_BLK_RXSTPIS         (_U_(0x1) << SAMHS_DEV_EPTIFR_BLK_RXSTPIS_Pos)  /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_BLK_NAKOUTIS_Pos    3                                              /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_BLK_NAKOUTIS        (_U_(0x1) << SAMHS_DEV_EPTIFR_BLK_NAKOUTIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_BLK_NAKINIS_Pos     4                                              /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_BLK_NAKINIS         (_U_(0x1) << SAMHS_DEV_EPTIFR_BLK_NAKINIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_BLK_STALLEDIS_Pos   6                                              /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_BLK_STALLEDIS       (_U_(0x1) << SAMHS_DEV_EPTIFR_BLK_STALLEDIS_Pos)  /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_BLK_Msk             _U_(0x5C)                                      /**< (SAMHS_DEV_EPTIFR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTIFR_INTRPT_RXSTPIS_Pos  2                                              /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_INTRPT_RXSTPIS      (_U_(0x1) << SAMHS_DEV_EPTIFR_INTRPT_RXSTPIS_Pos)  /**< (SAMHS_DEV_EPTIFR) Received SETUP Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_INTRPT_NAKOUTIS_Pos 3                                              /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_INTRPT_NAKOUTIS     (_U_(0x1) << SAMHS_DEV_EPTIFR_INTRPT_NAKOUTIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed OUT Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_INTRPT_NAKINIS_Pos  4                                              /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_INTRPT_NAKINIS      (_U_(0x1) << SAMHS_DEV_EPTIFR_INTRPT_NAKINIS_Pos)  /**< (SAMHS_DEV_EPTIFR) NAKed IN Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_INTRPT_STALLEDIS_Pos 6                                              /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Position */
#define SAMHS_DEV_EPTIFR_INTRPT_STALLEDIS     (_U_(0x1) << SAMHS_DEV_EPTIFR_INTRPT_STALLEDIS_Pos)  /**< (SAMHS_DEV_EPTIFR) STALLed Interrupt Set Mask */
#define SAMHS_DEV_EPTIFR_INTRPT_Msk          _U_(0x5C)                                      /**< (SAMHS_DEV_EPTIFR_INTRPT) Register Mask  */


/* -------- SAMHS_DEV_EPTIMR : (USBHS Offset: 0x1c0) (R/ 32) Device Endpoint Interrupt Mask Register -------- */

#define SAMHS_DEV_EPTIMR_OFFSET              (0x1C0)                                       /**<  (SAMHS_DEV_EPTIMR) Device Endpoint Interrupt Mask Register  Offset */

#define SAMHS_DEV_EPTIMR_TXINE_Pos           0                                              /**< (SAMHS_DEV_EPTIMR) Transmitted IN Data Interrupt Position */
#define SAMHS_DEV_EPTIMR_TXINE               (_U_(0x1) << SAMHS_DEV_EPTIMR_TXINE_Pos)        /**< (SAMHS_DEV_EPTIMR) Transmitted IN Data Interrupt Mask */
#define SAMHS_DEV_EPTIMR_RXOUTE_Pos          1                                              /**< (SAMHS_DEV_EPTIMR) Received OUT Data Interrupt Position */
#define SAMHS_DEV_EPTIMR_RXOUTE              (_U_(0x1) << SAMHS_DEV_EPTIMR_RXOUTE_Pos)       /**< (SAMHS_DEV_EPTIMR) Received OUT Data Interrupt Mask */
#define SAMHS_DEV_EPTIMR_OVERFE_Pos          5                                              /**< (SAMHS_DEV_EPTIMR) Overflow Interrupt Position */
#define SAMHS_DEV_EPTIMR_OVERFE              (_U_(0x1) << SAMHS_DEV_EPTIMR_OVERFE_Pos)       /**< (SAMHS_DEV_EPTIMR) Overflow Interrupt Mask */
#define SAMHS_DEV_EPTIMR_SHORTPACKETE_Pos    7                                              /**< (SAMHS_DEV_EPTIMR) Short Packet Interrupt Position */
#define SAMHS_DEV_EPTIMR_SHORTPACKETE        (_U_(0x1) << SAMHS_DEV_EPTIMR_SHORTPACKETE_Pos)  /**< (SAMHS_DEV_EPTIMR) Short Packet Interrupt Mask */
#define SAMHS_DEV_EPTIMR_NBUSYBKE_Pos        12                                             /**< (SAMHS_DEV_EPTIMR) Number of Busy Banks Interrupt Position */
#define SAMHS_DEV_EPTIMR_NBUSYBKE            (_U_(0x1) << SAMHS_DEV_EPTIMR_NBUSYBKE_Pos)     /**< (SAMHS_DEV_EPTIMR) Number of Busy Banks Interrupt Mask */
#define SAMHS_DEV_EPTIMR_KILLBK_Pos          13                                             /**< (SAMHS_DEV_EPTIMR) Kill IN Bank Position */
#define SAMHS_DEV_EPTIMR_KILLBK              (_U_(0x1) << SAMHS_DEV_EPTIMR_KILLBK_Pos)       /**< (SAMHS_DEV_EPTIMR) Kill IN Bank Mask */
#define SAMHS_DEV_EPTIMR_FIFOCON_Pos         14                                             /**< (SAMHS_DEV_EPTIMR) FIFO Control Position */
#define SAMHS_DEV_EPTIMR_FIFOCON             (_U_(0x1) << SAMHS_DEV_EPTIMR_FIFOCON_Pos)      /**< (SAMHS_DEV_EPTIMR) FIFO Control Mask */
#define SAMHS_DEV_EPTIMR_EPDISHDMA_Pos       16                                             /**< (SAMHS_DEV_EPTIMR) Endpoint Interrupts Disable HDMA Request Position */
#define SAMHS_DEV_EPTIMR_EPDISHDMA           (_U_(0x1) << SAMHS_DEV_EPTIMR_EPDISHDMA_Pos)    /**< (SAMHS_DEV_EPTIMR) Endpoint Interrupts Disable HDMA Request Mask */
#define SAMHS_DEV_EPTIMR_RSTDT_Pos           18                                             /**< (SAMHS_DEV_EPTIMR) Reset Data Toggle Position */
#define SAMHS_DEV_EPTIMR_RSTDT               (_U_(0x1) << SAMHS_DEV_EPTIMR_RSTDT_Pos)        /**< (SAMHS_DEV_EPTIMR) Reset Data Toggle Mask */
#define SAMHS_DEV_EPTIMR_Msk                 _U_(0x570A3)                                   /**< (SAMHS_DEV_EPTIMR) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTIMR_CTRL_RXSTPE_Pos     2                                              /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTIMR_CTRL_RXSTPE         (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_RXSTPE_Pos)  /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTIMR_CTRL_NAKOUTE_Pos    3                                              /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTIMR_CTRL_NAKOUTE        (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_NAKOUTE_Pos)  /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTIMR_CTRL_NAKINE_Pos     4                                              /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTIMR_CTRL_NAKINE         (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_NAKINE_Pos)  /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTIMR_CTRL_STALLEDE_Pos   6                                              /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTIMR_CTRL_STALLEDE       (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_STALLEDE_Pos)  /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTIMR_CTRL_NYETDIS_Pos    17                                             /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Position */
#define SAMHS_DEV_EPTIMR_CTRL_NYETDIS        (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_NYETDIS_Pos)  /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Mask */
#define SAMHS_DEV_EPTIMR_CTRL_STALLRQ_Pos    19                                             /**< (SAMHS_DEV_EPTIMR) STALL Request Position */
#define SAMHS_DEV_EPTIMR_CTRL_STALLRQ        (_U_(0x1) << SAMHS_DEV_EPTIMR_CTRL_STALLRQ_Pos)  /**< (SAMHS_DEV_EPTIMR) STALL Request Mask */
#define SAMHS_DEV_EPTIMR_CTRL_Msk            _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIMR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTIMR_ISO_UNDERFE_Pos     2                                              /**< (SAMHS_DEV_EPTIMR) Underflow Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_UNDERFE         (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_UNDERFE_Pos)  /**< (SAMHS_DEV_EPTIMR) Underflow Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_HBISOINERRE_Pos 3                                              /**< (SAMHS_DEV_EPTIMR) High Bandwidth Isochronous IN Underflow Error Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_HBISOINERRE     (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_HBISOINERRE_Pos)  /**< (SAMHS_DEV_EPTIMR) High Bandwidth Isochronous IN Underflow Error Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_HBISOFLUSHE_Pos 4                                              /**< (SAMHS_DEV_EPTIMR) High Bandwidth Isochronous IN Flush Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_HBISOFLUSHE     (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_HBISOFLUSHE_Pos)  /**< (SAMHS_DEV_EPTIMR) High Bandwidth Isochronous IN Flush Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_CRCERRE_Pos     6                                              /**< (SAMHS_DEV_EPTIMR) CRC Error Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_CRCERRE         (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_CRCERRE_Pos)  /**< (SAMHS_DEV_EPTIMR) CRC Error Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_MDATAE_Pos      8                                              /**< (SAMHS_DEV_EPTIMR) MData Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_MDATAE          (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_MDATAE_Pos)   /**< (SAMHS_DEV_EPTIMR) MData Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_DATAXE_Pos      9                                              /**< (SAMHS_DEV_EPTIMR) DataX Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_DATAXE          (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_DATAXE_Pos)   /**< (SAMHS_DEV_EPTIMR) DataX Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_ERRORTRANSE_Pos 10                                             /**< (SAMHS_DEV_EPTIMR) Transaction Error Interrupt Position */
#define SAMHS_DEV_EPTIMR_ISO_ERRORTRANSE     (_U_(0x1) << SAMHS_DEV_EPTIMR_ISO_ERRORTRANSE_Pos)  /**< (SAMHS_DEV_EPTIMR) Transaction Error Interrupt Mask */
#define SAMHS_DEV_EPTIMR_ISO_Msk             _U_(0x75C)                                     /**< (SAMHS_DEV_EPTIMR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTIMR_BLK_RXSTPE_Pos      2                                              /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTIMR_BLK_RXSTPE          (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_RXSTPE_Pos)   /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTIMR_BLK_NAKOUTE_Pos     3                                              /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTIMR_BLK_NAKOUTE         (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_NAKOUTE_Pos)  /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTIMR_BLK_NAKINE_Pos      4                                              /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTIMR_BLK_NAKINE          (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_NAKINE_Pos)   /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTIMR_BLK_STALLEDE_Pos    6                                              /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTIMR_BLK_STALLEDE        (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_STALLEDE_Pos)  /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTIMR_BLK_NYETDIS_Pos     17                                             /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Position */
#define SAMHS_DEV_EPTIMR_BLK_NYETDIS         (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_NYETDIS_Pos)  /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Mask */
#define SAMHS_DEV_EPTIMR_BLK_STALLRQ_Pos     19                                             /**< (SAMHS_DEV_EPTIMR) STALL Request Position */
#define SAMHS_DEV_EPTIMR_BLK_STALLRQ         (_U_(0x1) << SAMHS_DEV_EPTIMR_BLK_STALLRQ_Pos)  /**< (SAMHS_DEV_EPTIMR) STALL Request Mask */
#define SAMHS_DEV_EPTIMR_BLK_Msk             _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIMR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTIMR_INTRPT_RXSTPE_Pos   2                                              /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Position */
#define SAMHS_DEV_EPTIMR_INTRPT_RXSTPE       (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_RXSTPE_Pos)  /**< (SAMHS_DEV_EPTIMR) Received SETUP Interrupt Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_NAKOUTE_Pos  3                                              /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Position */
#define SAMHS_DEV_EPTIMR_INTRPT_NAKOUTE      (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_NAKOUTE_Pos)  /**< (SAMHS_DEV_EPTIMR) NAKed OUT Interrupt Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_NAKINE_Pos   4                                              /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Position */
#define SAMHS_DEV_EPTIMR_INTRPT_NAKINE       (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_NAKINE_Pos)  /**< (SAMHS_DEV_EPTIMR) NAKed IN Interrupt Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_STALLEDE_Pos 6                                              /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Position */
#define SAMHS_DEV_EPTIMR_INTRPT_STALLEDE     (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_STALLEDE_Pos)  /**< (SAMHS_DEV_EPTIMR) STALLed Interrupt Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_NYETDIS_Pos  17                                             /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Position */
#define SAMHS_DEV_EPTIMR_INTRPT_NYETDIS      (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_NYETDIS_Pos)  /**< (SAMHS_DEV_EPTIMR) NYET Token Disable Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_STALLRQ_Pos  19                                             /**< (SAMHS_DEV_EPTIMR) STALL Request Position */
#define SAMHS_DEV_EPTIMR_INTRPT_STALLRQ      (_U_(0x1) << SAMHS_DEV_EPTIMR_INTRPT_STALLRQ_Pos)  /**< (SAMHS_DEV_EPTIMR) STALL Request Mask */
#define SAMHS_DEV_EPTIMR_INTRPT_Msk          _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIMR_INTRPT) Register Mask  */


/* -------- SAMHS_DEV_EPTIER : (USBHS Offset: 0x1f0) (/W 32) Device Endpoint Interrupt Enable Register -------- */

#define SAMHS_DEV_EPTIER_OFFSET              (0x1F0)                                       /**<  (SAMHS_DEV_EPTIER) Device Endpoint Interrupt Enable Register  Offset */

#define SAMHS_DEV_EPTIER_TXINES_Pos          0                                              /**< (SAMHS_DEV_EPTIER) Transmitted IN Data Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_TXINES              (_U_(0x1) << SAMHS_DEV_EPTIER_TXINES_Pos)       /**< (SAMHS_DEV_EPTIER) Transmitted IN Data Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_RXOUTES_Pos         1                                              /**< (SAMHS_DEV_EPTIER) Received OUT Data Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_RXOUTES             (_U_(0x1) << SAMHS_DEV_EPTIER_RXOUTES_Pos)      /**< (SAMHS_DEV_EPTIER) Received OUT Data Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_OVERFES_Pos         5                                              /**< (SAMHS_DEV_EPTIER) Overflow Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_OVERFES             (_U_(0x1) << SAMHS_DEV_EPTIER_OVERFES_Pos)      /**< (SAMHS_DEV_EPTIER) Overflow Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_SHORTPACKETES_Pos   7                                              /**< (SAMHS_DEV_EPTIER) Short Packet Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_SHORTPACKETES       (_U_(0x1) << SAMHS_DEV_EPTIER_SHORTPACKETES_Pos)  /**< (SAMHS_DEV_EPTIER) Short Packet Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_NBUSYBKES_Pos       12                                             /**< (SAMHS_DEV_EPTIER) Number of Busy Banks Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_NBUSYBKES           (_U_(0x1) << SAMHS_DEV_EPTIER_NBUSYBKES_Pos)    /**< (SAMHS_DEV_EPTIER) Number of Busy Banks Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_KILLBKS_Pos         13                                             /**< (SAMHS_DEV_EPTIER) Kill IN Bank Position */
#define SAMHS_DEV_EPTIER_KILLBKS             (_U_(0x1) << SAMHS_DEV_EPTIER_KILLBKS_Pos)      /**< (SAMHS_DEV_EPTIER) Kill IN Bank Mask */
#define SAMHS_DEV_EPTIER_FIFOCONS_Pos        14                                             /**< (SAMHS_DEV_EPTIER) FIFO Control Position */
#define SAMHS_DEV_EPTIER_FIFOCONS            (_U_(0x1) << SAMHS_DEV_EPTIER_FIFOCONS_Pos)     /**< (SAMHS_DEV_EPTIER) FIFO Control Mask */
#define SAMHS_DEV_EPTIER_EPDISHDMAS_Pos      16                                             /**< (SAMHS_DEV_EPTIER) Endpoint Interrupts Disable HDMA Request Enable Position */
#define SAMHS_DEV_EPTIER_EPDISHDMAS          (_U_(0x1) << SAMHS_DEV_EPTIER_EPDISHDMAS_Pos)   /**< (SAMHS_DEV_EPTIER) Endpoint Interrupts Disable HDMA Request Enable Mask */
#define SAMHS_DEV_EPTIER_RSTDTS_Pos          18                                             /**< (SAMHS_DEV_EPTIER) Reset Data Toggle Enable Position */
#define SAMHS_DEV_EPTIER_RSTDTS              (_U_(0x1) << SAMHS_DEV_EPTIER_RSTDTS_Pos)       /**< (SAMHS_DEV_EPTIER) Reset Data Toggle Enable Mask */
#define SAMHS_DEV_EPTIER_Msk                 _U_(0x570A3)                                   /**< (SAMHS_DEV_EPTIER) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTIER_CTRL_RXSTPES_Pos    2                                              /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_RXSTPES        (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_RXSTPES_Pos)  /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_NAKOUTES_Pos   3                                              /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_NAKOUTES       (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_NAKOUTES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_NAKINES_Pos    4                                              /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_NAKINES        (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_NAKINES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_STALLEDES_Pos  6                                              /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_STALLEDES      (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_STALLEDES_Pos)  /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_NYETDISS_Pos   17                                             /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_NYETDISS       (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_NYETDISS_Pos)  /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_STALLRQS_Pos   19                                             /**< (SAMHS_DEV_EPTIER) STALL Request Enable Position */
#define SAMHS_DEV_EPTIER_CTRL_STALLRQS       (_U_(0x1) << SAMHS_DEV_EPTIER_CTRL_STALLRQS_Pos)  /**< (SAMHS_DEV_EPTIER) STALL Request Enable Mask */
#define SAMHS_DEV_EPTIER_CTRL_Msk            _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIER_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTIER_ISO_UNDERFES_Pos    2                                              /**< (SAMHS_DEV_EPTIER) Underflow Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_UNDERFES        (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_UNDERFES_Pos)  /**< (SAMHS_DEV_EPTIER) Underflow Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_HBISOINERRES_Pos 3                                              /**< (SAMHS_DEV_EPTIER) High Bandwidth Isochronous IN Underflow Error Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_HBISOINERRES     (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_HBISOINERRES_Pos)  /**< (SAMHS_DEV_EPTIER) High Bandwidth Isochronous IN Underflow Error Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_HBISOFLUSHES_Pos 4                                              /**< (SAMHS_DEV_EPTIER) High Bandwidth Isochronous IN Flush Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_HBISOFLUSHES     (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_HBISOFLUSHES_Pos)  /**< (SAMHS_DEV_EPTIER) High Bandwidth Isochronous IN Flush Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_CRCERRES_Pos    6                                              /**< (SAMHS_DEV_EPTIER) CRC Error Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_CRCERRES        (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_CRCERRES_Pos)  /**< (SAMHS_DEV_EPTIER) CRC Error Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_MDATAES_Pos     8                                              /**< (SAMHS_DEV_EPTIER) MData Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_MDATAES         (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_MDATAES_Pos)  /**< (SAMHS_DEV_EPTIER) MData Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_DATAXES_Pos     9                                              /**< (SAMHS_DEV_EPTIER) DataX Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_DATAXES         (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_DATAXES_Pos)  /**< (SAMHS_DEV_EPTIER) DataX Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_ERRORTRANSES_Pos 10                                             /**< (SAMHS_DEV_EPTIER) Transaction Error Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_ISO_ERRORTRANSES     (_U_(0x1) << SAMHS_DEV_EPTIER_ISO_ERRORTRANSES_Pos)  /**< (SAMHS_DEV_EPTIER) Transaction Error Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_ISO_Msk             _U_(0x75C)                                     /**< (SAMHS_DEV_EPTIER_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTIER_BLK_RXSTPES_Pos     2                                              /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_BLK_RXSTPES         (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_RXSTPES_Pos)  /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_NAKOUTES_Pos    3                                              /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_BLK_NAKOUTES        (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_NAKOUTES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_NAKINES_Pos     4                                              /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_BLK_NAKINES         (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_NAKINES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_STALLEDES_Pos   6                                              /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_BLK_STALLEDES       (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_STALLEDES_Pos)  /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_NYETDISS_Pos    17                                             /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Position */
#define SAMHS_DEV_EPTIER_BLK_NYETDISS        (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_NYETDISS_Pos)  /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_STALLRQS_Pos    19                                             /**< (SAMHS_DEV_EPTIER) STALL Request Enable Position */
#define SAMHS_DEV_EPTIER_BLK_STALLRQS        (_U_(0x1) << SAMHS_DEV_EPTIER_BLK_STALLRQS_Pos)  /**< (SAMHS_DEV_EPTIER) STALL Request Enable Mask */
#define SAMHS_DEV_EPTIER_BLK_Msk             _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIER_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTIER_INTRPT_RXSTPES_Pos  2                                              /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_RXSTPES      (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_RXSTPES_Pos)  /**< (SAMHS_DEV_EPTIER) Received SETUP Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_NAKOUTES_Pos 3                                              /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_NAKOUTES     (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_NAKOUTES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed OUT Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_NAKINES_Pos  4                                              /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_NAKINES      (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_NAKINES_Pos)  /**< (SAMHS_DEV_EPTIER) NAKed IN Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_STALLEDES_Pos 6                                              /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_STALLEDES     (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_STALLEDES_Pos)  /**< (SAMHS_DEV_EPTIER) STALLed Interrupt Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_NYETDISS_Pos 17                                             /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_NYETDISS     (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_NYETDISS_Pos)  /**< (SAMHS_DEV_EPTIER) NYET Token Disable Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_STALLRQS_Pos 19                                             /**< (SAMHS_DEV_EPTIER) STALL Request Enable Position */
#define SAMHS_DEV_EPTIER_INTRPT_STALLRQS     (_U_(0x1) << SAMHS_DEV_EPTIER_INTRPT_STALLRQS_Pos)  /**< (SAMHS_DEV_EPTIER) STALL Request Enable Mask */
#define SAMHS_DEV_EPTIER_INTRPT_Msk          _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIER_INTRPT) Register Mask  */


/* -------- SAMHS_DEV_EPTIDR : (USBHS Offset: 0x220) (/W 32) Device Endpoint Interrupt Disable Register -------- */

#define SAMHS_DEV_EPTIDR_OFFSET              (0x220)                                       /**<  (SAMHS_DEV_EPTIDR) Device Endpoint Interrupt Disable Register  Offset */

#define SAMHS_DEV_EPTIDR_TXINEC_Pos          0                                              /**< (SAMHS_DEV_EPTIDR) Transmitted IN Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_TXINEC              (_U_(0x1) << SAMHS_DEV_EPTIDR_TXINEC_Pos)       /**< (SAMHS_DEV_EPTIDR) Transmitted IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_RXOUTEC_Pos         1                                              /**< (SAMHS_DEV_EPTIDR) Received OUT Data Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_RXOUTEC             (_U_(0x1) << SAMHS_DEV_EPTIDR_RXOUTEC_Pos)      /**< (SAMHS_DEV_EPTIDR) Received OUT Data Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_OVERFEC_Pos         5                                              /**< (SAMHS_DEV_EPTIDR) Overflow Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_OVERFEC             (_U_(0x1) << SAMHS_DEV_EPTIDR_OVERFEC_Pos)      /**< (SAMHS_DEV_EPTIDR) Overflow Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_SHORTPACKETEC_Pos   7                                              /**< (SAMHS_DEV_EPTIDR) Shortpacket Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_SHORTPACKETEC       (_U_(0x1) << SAMHS_DEV_EPTIDR_SHORTPACKETEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Shortpacket Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_NBUSYBKEC_Pos       12                                             /**< (SAMHS_DEV_EPTIDR) Number of Busy Banks Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_NBUSYBKEC           (_U_(0x1) << SAMHS_DEV_EPTIDR_NBUSYBKEC_Pos)    /**< (SAMHS_DEV_EPTIDR) Number of Busy Banks Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_FIFOCONC_Pos        14                                             /**< (SAMHS_DEV_EPTIDR) FIFO Control Clear Position */
#define SAMHS_DEV_EPTIDR_FIFOCONC            (_U_(0x1) << SAMHS_DEV_EPTIDR_FIFOCONC_Pos)     /**< (SAMHS_DEV_EPTIDR) FIFO Control Clear Mask */
#define SAMHS_DEV_EPTIDR_EPDISHDMAC_Pos      16                                             /**< (SAMHS_DEV_EPTIDR) Endpoint Interrupts Disable HDMA Request Clear Position */
#define SAMHS_DEV_EPTIDR_EPDISHDMAC          (_U_(0x1) << SAMHS_DEV_EPTIDR_EPDISHDMAC_Pos)   /**< (SAMHS_DEV_EPTIDR) Endpoint Interrupts Disable HDMA Request Clear Mask */
#define SAMHS_DEV_EPTIDR_Msk                 _U_(0x150A3)                                   /**< (SAMHS_DEV_EPTIDR) Register Mask  */

/* CTRL mode */
#define SAMHS_DEV_EPTIDR_CTRL_RXSTPEC_Pos    2                                              /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_RXSTPEC        (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_RXSTPEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_NAKOUTEC_Pos   3                                              /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_NAKOUTEC       (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_NAKOUTEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_NAKINEC_Pos    4                                              /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_NAKINEC        (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_NAKINEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_STALLEDEC_Pos  6                                              /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_STALLEDEC      (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_STALLEDEC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_NYETDISC_Pos   17                                             /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_NYETDISC       (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_NYETDISC_Pos)  /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_STALLRQC_Pos   19                                             /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Position */
#define SAMHS_DEV_EPTIDR_CTRL_STALLRQC       (_U_(0x1) << SAMHS_DEV_EPTIDR_CTRL_STALLRQC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Mask */
#define SAMHS_DEV_EPTIDR_CTRL_Msk            _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIDR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_DEV_EPTIDR_ISO_UNDERFEC_Pos    2                                              /**< (SAMHS_DEV_EPTIDR) Underflow Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_UNDERFEC        (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_UNDERFEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Underflow Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_HBISOINERREC_Pos 3                                              /**< (SAMHS_DEV_EPTIDR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_HBISOINERREC     (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_HBISOINERREC_Pos)  /**< (SAMHS_DEV_EPTIDR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_HBISOFLUSHEC_Pos 4                                              /**< (SAMHS_DEV_EPTIDR) High Bandwidth Isochronous IN Flush Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_HBISOFLUSHEC     (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_HBISOFLUSHEC_Pos)  /**< (SAMHS_DEV_EPTIDR) High Bandwidth Isochronous IN Flush Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_MDATAEC_Pos     8                                              /**< (SAMHS_DEV_EPTIDR) MData Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_MDATAEC         (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_MDATAEC_Pos)  /**< (SAMHS_DEV_EPTIDR) MData Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_DATAXEC_Pos     9                                              /**< (SAMHS_DEV_EPTIDR) DataX Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_DATAXEC         (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_DATAXEC_Pos)  /**< (SAMHS_DEV_EPTIDR) DataX Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_ERRORTRANSEC_Pos 10                                             /**< (SAMHS_DEV_EPTIDR) Transaction Error Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_ISO_ERRORTRANSEC     (_U_(0x1) << SAMHS_DEV_EPTIDR_ISO_ERRORTRANSEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Transaction Error Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_ISO_Msk             _U_(0x71C)                                     /**< (SAMHS_DEV_EPTIDR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_DEV_EPTIDR_BLK_RXSTPEC_Pos     2                                              /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_RXSTPEC         (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_RXSTPEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_NAKOUTEC_Pos    3                                              /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_NAKOUTEC        (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_NAKOUTEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_NAKINEC_Pos     4                                              /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_NAKINEC         (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_NAKINEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_STALLEDEC_Pos   6                                              /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_STALLEDEC       (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_STALLEDEC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_NYETDISC_Pos    17                                             /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_NYETDISC        (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_NYETDISC_Pos)  /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_STALLRQC_Pos    19                                             /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Position */
#define SAMHS_DEV_EPTIDR_BLK_STALLRQC        (_U_(0x1) << SAMHS_DEV_EPTIDR_BLK_STALLRQC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Mask */
#define SAMHS_DEV_EPTIDR_BLK_Msk             _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIDR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_DEV_EPTIDR_INTRPT_RXSTPEC_Pos  2                                              /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_RXSTPEC      (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_RXSTPEC_Pos)  /**< (SAMHS_DEV_EPTIDR) Received SETUP Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_NAKOUTEC_Pos 3                                              /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_NAKOUTEC     (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_NAKOUTEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed OUT Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_NAKINEC_Pos  4                                              /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_NAKINEC      (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_NAKINEC_Pos)  /**< (SAMHS_DEV_EPTIDR) NAKed IN Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_STALLEDEC_Pos 6                                              /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_STALLEDEC     (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_STALLEDEC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALLed Interrupt Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_NYETDISC_Pos 17                                             /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_NYETDISC     (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_NYETDISC_Pos)  /**< (SAMHS_DEV_EPTIDR) NYET Token Disable Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_STALLRQC_Pos 19                                             /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Position */
#define SAMHS_DEV_EPTIDR_INTRPT_STALLRQC     (_U_(0x1) << SAMHS_DEV_EPTIDR_INTRPT_STALLRQC_Pos)  /**< (SAMHS_DEV_EPTIDR) STALL Request Clear Mask */
#define SAMHS_DEV_EPTIDR_INTRPT_Msk          _U_(0xA005C)                                   /**< (SAMHS_DEV_EPTIDR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_CTRL : (USBHS Offset: 0x400) (R/W 32) Host General Control Register -------- */

#define SAMHS_HST_CTRL_OFFSET                (0x400)                                       /**<  (SAMHS_HST_CTRL) Host General Control Register  Offset */

#define SAMHS_HST_CTRL_SOFE_Pos              8                                              /**< (SAMHS_HST_CTRL) Start of Frame Generation Enable Position */
#define SAMHS_HST_CTRL_SOFE                  (_U_(0x1) << SAMHS_HST_CTRL_SOFE_Pos)           /**< (SAMHS_HST_CTRL) Start of Frame Generation Enable Mask */
#define SAMHS_HST_CTRL_RESET_Pos             9                                              /**< (SAMHS_HST_CTRL) Send USB Reset Position */
#define SAMHS_HST_CTRL_RESET                 (_U_(0x1) << SAMHS_HST_CTRL_RESET_Pos)          /**< (SAMHS_HST_CTRL) Send USB Reset Mask */
#define SAMHS_HST_CTRL_RESUME_Pos            10                                             /**< (SAMHS_HST_CTRL) Send USB Resume Position */
#define SAMHS_HST_CTRL_RESUME                (_U_(0x1) << SAMHS_HST_CTRL_RESUME_Pos)         /**< (SAMHS_HST_CTRL) Send USB Resume Mask */
#define SAMHS_HST_CTRL_SPDCONF_Pos           12                                             /**< (SAMHS_HST_CTRL) Mode Configuration Position */
#define SAMHS_HST_CTRL_SPDCONF               (_U_(0x3) << SAMHS_HST_CTRL_SPDCONF_Pos)        /**< (SAMHS_HST_CTRL) Mode Configuration Mask */
#define   SAMHS_HST_CTRL_SPDCONF_NORMAL_Val  _U_(0x0)                                       /**< (SAMHS_HST_CTRL) The host starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the downstream peripheral is high-speed capable.  */
#define   SAMHS_HST_CTRL_SPDCONF_LOW_POWER_Val _U_(0x1)                                       /**< (SAMHS_HST_CTRL) For a better consumption, if high speed is not needed.  */
#define   SAMHS_HST_CTRL_SPDCONF_HIGH_SPEED_Val _U_(0x2)                                       /**< (SAMHS_HST_CTRL) Forced high speed.  */
#define   SAMHS_HST_CTRL_SPDCONF_FORCED_FS_Val _U_(0x3)                                       /**< (SAMHS_HST_CTRL) The host remains in Full-speed mode whatever the peripheral speed capability.  */
#define SAMHS_HST_CTRL_SPDCONF_NORMAL        (SAMHS_HST_CTRL_SPDCONF_NORMAL_Val << SAMHS_HST_CTRL_SPDCONF_Pos)  /**< (SAMHS_HST_CTRL) The host starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the downstream peripheral is high-speed capable. Position  */
#define SAMHS_HST_CTRL_SPDCONF_LOW_POWER     (SAMHS_HST_CTRL_SPDCONF_LOW_POWER_Val << SAMHS_HST_CTRL_SPDCONF_Pos)  /**< (SAMHS_HST_CTRL) For a better consumption, if high speed is not needed. Position  */
#define SAMHS_HST_CTRL_SPDCONF_HIGH_SPEED    (SAMHS_HST_CTRL_SPDCONF_HIGH_SPEED_Val << SAMHS_HST_CTRL_SPDCONF_Pos)  /**< (SAMHS_HST_CTRL) Forced high speed. Position  */
#define SAMHS_HST_CTRL_SPDCONF_FORCED_FS     (SAMHS_HST_CTRL_SPDCONF_FORCED_FS_Val << SAMHS_HST_CTRL_SPDCONF_Pos)  /**< (SAMHS_HST_CTRL) The host remains in Full-speed mode whatever the peripheral speed capability. Position  */
#define SAMHS_HST_CTRL_Msk                   _U_(0x3700)                                    /**< (SAMHS_HST_CTRL) Register Mask  */


/* -------- SAMHS_HST_ISR : (USBHS Offset: 0x404) (R/ 32) Host Global Interrupt Status Register -------- */

#define SAMHS_HST_ISR_OFFSET                 (0x404)                                       /**<  (SAMHS_HST_ISR) Host Global Interrupt Status Register  Offset */

#define SAMHS_HST_ISR_DCONNI_Pos             0                                              /**< (SAMHS_HST_ISR) Device Connection Interrupt Position */
#define SAMHS_HST_ISR_DCONNI                 (_U_(0x1) << SAMHS_HST_ISR_DCONNI_Pos)          /**< (SAMHS_HST_ISR) Device Connection Interrupt Mask */
#define SAMHS_HST_ISR_DDISCI_Pos             1                                              /**< (SAMHS_HST_ISR) Device Disconnection Interrupt Position */
#define SAMHS_HST_ISR_DDISCI                 (_U_(0x1) << SAMHS_HST_ISR_DDISCI_Pos)          /**< (SAMHS_HST_ISR) Device Disconnection Interrupt Mask */
#define SAMHS_HST_ISR_RSTI_Pos               2                                              /**< (SAMHS_HST_ISR) USB Reset Sent Interrupt Position */
#define SAMHS_HST_ISR_RSTI                   (_U_(0x1) << SAMHS_HST_ISR_RSTI_Pos)            /**< (SAMHS_HST_ISR) USB Reset Sent Interrupt Mask */
#define SAMHS_HST_ISR_RSMEDI_Pos             3                                              /**< (SAMHS_HST_ISR) Downstream Resume Sent Interrupt Position */
#define SAMHS_HST_ISR_RSMEDI                 (_U_(0x1) << SAMHS_HST_ISR_RSMEDI_Pos)          /**< (SAMHS_HST_ISR) Downstream Resume Sent Interrupt Mask */
#define SAMHS_HST_ISR_RXRSMI_Pos             4                                              /**< (SAMHS_HST_ISR) Upstream Resume Received Interrupt Position */
#define SAMHS_HST_ISR_RXRSMI                 (_U_(0x1) << SAMHS_HST_ISR_RXRSMI_Pos)          /**< (SAMHS_HST_ISR) Upstream Resume Received Interrupt Mask */
#define SAMHS_HST_ISR_HSOFI_Pos              5                                              /**< (SAMHS_HST_ISR) Host Start of Frame Interrupt Position */
#define SAMHS_HST_ISR_HSOFI                  (_U_(0x1) << SAMHS_HST_ISR_HSOFI_Pos)           /**< (SAMHS_HST_ISR) Host Start of Frame Interrupt Mask */
#define SAMHS_HST_ISR_HWUPI_Pos              6                                              /**< (SAMHS_HST_ISR) Host Wake-Up Interrupt Position */
#define SAMHS_HST_ISR_HWUPI                  (_U_(0x1) << SAMHS_HST_ISR_HWUPI_Pos)           /**< (SAMHS_HST_ISR) Host Wake-Up Interrupt Mask */
#define SAMHS_HST_ISR_PEP_0_Pos              8                                              /**< (SAMHS_HST_ISR) Pipe 0 Interrupt Position */
#define SAMHS_HST_ISR_PEP_0                  (_U_(0x1) << SAMHS_HST_ISR_PEP_0_Pos)           /**< (SAMHS_HST_ISR) Pipe 0 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_1_Pos              9                                              /**< (SAMHS_HST_ISR) Pipe 1 Interrupt Position */
#define SAMHS_HST_ISR_PEP_1                  (_U_(0x1) << SAMHS_HST_ISR_PEP_1_Pos)           /**< (SAMHS_HST_ISR) Pipe 1 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_2_Pos              10                                             /**< (SAMHS_HST_ISR) Pipe 2 Interrupt Position */
#define SAMHS_HST_ISR_PEP_2                  (_U_(0x1) << SAMHS_HST_ISR_PEP_2_Pos)           /**< (SAMHS_HST_ISR) Pipe 2 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_3_Pos              11                                             /**< (SAMHS_HST_ISR) Pipe 3 Interrupt Position */
#define SAMHS_HST_ISR_PEP_3                  (_U_(0x1) << SAMHS_HST_ISR_PEP_3_Pos)           /**< (SAMHS_HST_ISR) Pipe 3 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_4_Pos              12                                             /**< (SAMHS_HST_ISR) Pipe 4 Interrupt Position */
#define SAMHS_HST_ISR_PEP_4                  (_U_(0x1) << SAMHS_HST_ISR_PEP_4_Pos)           /**< (SAMHS_HST_ISR) Pipe 4 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_5_Pos              13                                             /**< (SAMHS_HST_ISR) Pipe 5 Interrupt Position */
#define SAMHS_HST_ISR_PEP_5                  (_U_(0x1) << SAMHS_HST_ISR_PEP_5_Pos)           /**< (SAMHS_HST_ISR) Pipe 5 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_6_Pos              14                                             /**< (SAMHS_HST_ISR) Pipe 6 Interrupt Position */
#define SAMHS_HST_ISR_PEP_6                  (_U_(0x1) << SAMHS_HST_ISR_PEP_6_Pos)           /**< (SAMHS_HST_ISR) Pipe 6 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_7_Pos              15                                             /**< (SAMHS_HST_ISR) Pipe 7 Interrupt Position */
#define SAMHS_HST_ISR_PEP_7                  (_U_(0x1) << SAMHS_HST_ISR_PEP_7_Pos)           /**< (SAMHS_HST_ISR) Pipe 7 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_8_Pos              16                                             /**< (SAMHS_HST_ISR) Pipe 8 Interrupt Position */
#define SAMHS_HST_ISR_PEP_8                  (_U_(0x1) << SAMHS_HST_ISR_PEP_8_Pos)           /**< (SAMHS_HST_ISR) Pipe 8 Interrupt Mask */
#define SAMHS_HST_ISR_PEP_9_Pos              17                                             /**< (SAMHS_HST_ISR) Pipe 9 Interrupt Position */
#define SAMHS_HST_ISR_PEP_9                  (_U_(0x1) << SAMHS_HST_ISR_PEP_9_Pos)           /**< (SAMHS_HST_ISR) Pipe 9 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_0_Pos              25                                             /**< (SAMHS_HST_ISR) DMA Channel 0 Interrupt Position */
#define SAMHS_HST_ISR_DMA_0                  (_U_(0x1) << SAMHS_HST_ISR_DMA_0_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 0 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_1_Pos              26                                             /**< (SAMHS_HST_ISR) DMA Channel 1 Interrupt Position */
#define SAMHS_HST_ISR_DMA_1                  (_U_(0x1) << SAMHS_HST_ISR_DMA_1_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 1 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_2_Pos              27                                             /**< (SAMHS_HST_ISR) DMA Channel 2 Interrupt Position */
#define SAMHS_HST_ISR_DMA_2                  (_U_(0x1) << SAMHS_HST_ISR_DMA_2_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 2 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_3_Pos              28                                             /**< (SAMHS_HST_ISR) DMA Channel 3 Interrupt Position */
#define SAMHS_HST_ISR_DMA_3                  (_U_(0x1) << SAMHS_HST_ISR_DMA_3_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 3 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_4_Pos              29                                             /**< (SAMHS_HST_ISR) DMA Channel 4 Interrupt Position */
#define SAMHS_HST_ISR_DMA_4                  (_U_(0x1) << SAMHS_HST_ISR_DMA_4_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 4 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_5_Pos              30                                             /**< (SAMHS_HST_ISR) DMA Channel 5 Interrupt Position */
#define SAMHS_HST_ISR_DMA_5                  (_U_(0x1) << SAMHS_HST_ISR_DMA_5_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 5 Interrupt Mask */
#define SAMHS_HST_ISR_DMA_6_Pos              31                                             /**< (SAMHS_HST_ISR) DMA Channel 6 Interrupt Position */
#define SAMHS_HST_ISR_DMA_6                  (_U_(0x1) << SAMHS_HST_ISR_DMA_6_Pos)           /**< (SAMHS_HST_ISR) DMA Channel 6 Interrupt Mask */
#define SAMHS_HST_ISR_Msk                    _U_(0xFE03FF7F)                                /**< (SAMHS_HST_ISR) Register Mask  */

#define SAMHS_HST_ISR_PEP__Pos               8                                              /**< (SAMHS_HST_ISR Position) Pipe x Interrupt */
#define SAMHS_HST_ISR_PEP_                   (_U_(0x3FF) << SAMHS_HST_ISR_PEP__Pos)          /**< (SAMHS_HST_ISR Mask) PEP_ */
#define SAMHS_HST_ISR_DMA__Pos               25                                             /**< (SAMHS_HST_ISR Position) DMA Channel 6 Interrupt */
#define SAMHS_HST_ISR_DMA_                   (_U_(0x7F) << SAMHS_HST_ISR_DMA__Pos)           /**< (SAMHS_HST_ISR Mask) DMA_ */

/* -------- SAMHS_HST_ICR : (USBHS Offset: 0x408) (/W 32) Host Global Interrupt Clear Register -------- */

#define SAMHS_HST_ICR_OFFSET                 (0x408)                                       /**<  (SAMHS_HST_ICR) Host Global Interrupt Clear Register  Offset */

#define SAMHS_HST_ICR_DCONNIC_Pos            0                                              /**< (SAMHS_HST_ICR) Device Connection Interrupt Clear Position */
#define SAMHS_HST_ICR_DCONNIC                (_U_(0x1) << SAMHS_HST_ICR_DCONNIC_Pos)         /**< (SAMHS_HST_ICR) Device Connection Interrupt Clear Mask */
#define SAMHS_HST_ICR_DDISCIC_Pos            1                                              /**< (SAMHS_HST_ICR) Device Disconnection Interrupt Clear Position */
#define SAMHS_HST_ICR_DDISCIC                (_U_(0x1) << SAMHS_HST_ICR_DDISCIC_Pos)         /**< (SAMHS_HST_ICR) Device Disconnection Interrupt Clear Mask */
#define SAMHS_HST_ICR_RSTIC_Pos              2                                              /**< (SAMHS_HST_ICR) USB Reset Sent Interrupt Clear Position */
#define SAMHS_HST_ICR_RSTIC                  (_U_(0x1) << SAMHS_HST_ICR_RSTIC_Pos)           /**< (SAMHS_HST_ICR) USB Reset Sent Interrupt Clear Mask */
#define SAMHS_HST_ICR_RSMEDIC_Pos            3                                              /**< (SAMHS_HST_ICR) Downstream Resume Sent Interrupt Clear Position */
#define SAMHS_HST_ICR_RSMEDIC                (_U_(0x1) << SAMHS_HST_ICR_RSMEDIC_Pos)         /**< (SAMHS_HST_ICR) Downstream Resume Sent Interrupt Clear Mask */
#define SAMHS_HST_ICR_RXRSMIC_Pos            4                                              /**< (SAMHS_HST_ICR) Upstream Resume Received Interrupt Clear Position */
#define SAMHS_HST_ICR_RXRSMIC                (_U_(0x1) << SAMHS_HST_ICR_RXRSMIC_Pos)         /**< (SAMHS_HST_ICR) Upstream Resume Received Interrupt Clear Mask */
#define SAMHS_HST_ICR_HSOFIC_Pos             5                                              /**< (SAMHS_HST_ICR) Host Start of Frame Interrupt Clear Position */
#define SAMHS_HST_ICR_HSOFIC                 (_U_(0x1) << SAMHS_HST_ICR_HSOFIC_Pos)          /**< (SAMHS_HST_ICR) Host Start of Frame Interrupt Clear Mask */
#define SAMHS_HST_ICR_HWUPIC_Pos             6                                              /**< (SAMHS_HST_ICR) Host Wake-Up Interrupt Clear Position */
#define SAMHS_HST_ICR_HWUPIC                 (_U_(0x1) << SAMHS_HST_ICR_HWUPIC_Pos)          /**< (SAMHS_HST_ICR) Host Wake-Up Interrupt Clear Mask */
#define SAMHS_HST_ICR_Msk                    _U_(0x7F)                                      /**< (SAMHS_HST_ICR) Register Mask  */


/* -------- SAMHS_HST_IFR : (USBHS Offset: 0x40c) (/W 32) Host Global Interrupt Set Register -------- */

#define SAMHS_HST_IFR_OFFSET                 (0x40C)                                       /**<  (SAMHS_HST_IFR) Host Global Interrupt Set Register  Offset */

#define SAMHS_HST_IFR_DCONNIS_Pos            0                                              /**< (SAMHS_HST_IFR) Device Connection Interrupt Set Position */
#define SAMHS_HST_IFR_DCONNIS                (_U_(0x1) << SAMHS_HST_IFR_DCONNIS_Pos)         /**< (SAMHS_HST_IFR) Device Connection Interrupt Set Mask */
#define SAMHS_HST_IFR_DDISCIS_Pos            1                                              /**< (SAMHS_HST_IFR) Device Disconnection Interrupt Set Position */
#define SAMHS_HST_IFR_DDISCIS                (_U_(0x1) << SAMHS_HST_IFR_DDISCIS_Pos)         /**< (SAMHS_HST_IFR) Device Disconnection Interrupt Set Mask */
#define SAMHS_HST_IFR_RSTIS_Pos              2                                              /**< (SAMHS_HST_IFR) USB Reset Sent Interrupt Set Position */
#define SAMHS_HST_IFR_RSTIS                  (_U_(0x1) << SAMHS_HST_IFR_RSTIS_Pos)           /**< (SAMHS_HST_IFR) USB Reset Sent Interrupt Set Mask */
#define SAMHS_HST_IFR_RSMEDIS_Pos            3                                              /**< (SAMHS_HST_IFR) Downstream Resume Sent Interrupt Set Position */
#define SAMHS_HST_IFR_RSMEDIS                (_U_(0x1) << SAMHS_HST_IFR_RSMEDIS_Pos)         /**< (SAMHS_HST_IFR) Downstream Resume Sent Interrupt Set Mask */
#define SAMHS_HST_IFR_RXRSMIS_Pos            4                                              /**< (SAMHS_HST_IFR) Upstream Resume Received Interrupt Set Position */
#define SAMHS_HST_IFR_RXRSMIS                (_U_(0x1) << SAMHS_HST_IFR_RXRSMIS_Pos)         /**< (SAMHS_HST_IFR) Upstream Resume Received Interrupt Set Mask */
#define SAMHS_HST_IFR_HSOFIS_Pos             5                                              /**< (SAMHS_HST_IFR) Host Start of Frame Interrupt Set Position */
#define SAMHS_HST_IFR_HSOFIS                 (_U_(0x1) << SAMHS_HST_IFR_HSOFIS_Pos)          /**< (SAMHS_HST_IFR) Host Start of Frame Interrupt Set Mask */
#define SAMHS_HST_IFR_HWUPIS_Pos             6                                              /**< (SAMHS_HST_IFR) Host Wake-Up Interrupt Set Position */
#define SAMHS_HST_IFR_HWUPIS                 (_U_(0x1) << SAMHS_HST_IFR_HWUPIS_Pos)          /**< (SAMHS_HST_IFR) Host Wake-Up Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_0_Pos              25                                             /**< (SAMHS_HST_IFR) DMA Channel 0 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_0                  (_U_(0x1) << SAMHS_HST_IFR_DMA_0_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 0 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_1_Pos              26                                             /**< (SAMHS_HST_IFR) DMA Channel 1 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_1                  (_U_(0x1) << SAMHS_HST_IFR_DMA_1_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 1 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_2_Pos              27                                             /**< (SAMHS_HST_IFR) DMA Channel 2 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_2                  (_U_(0x1) << SAMHS_HST_IFR_DMA_2_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 2 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_3_Pos              28                                             /**< (SAMHS_HST_IFR) DMA Channel 3 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_3                  (_U_(0x1) << SAMHS_HST_IFR_DMA_3_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 3 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_4_Pos              29                                             /**< (SAMHS_HST_IFR) DMA Channel 4 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_4                  (_U_(0x1) << SAMHS_HST_IFR_DMA_4_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 4 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_5_Pos              30                                             /**< (SAMHS_HST_IFR) DMA Channel 5 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_5                  (_U_(0x1) << SAMHS_HST_IFR_DMA_5_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 5 Interrupt Set Mask */
#define SAMHS_HST_IFR_DMA_6_Pos              31                                             /**< (SAMHS_HST_IFR) DMA Channel 6 Interrupt Set Position */
#define SAMHS_HST_IFR_DMA_6                  (_U_(0x1) << SAMHS_HST_IFR_DMA_6_Pos)           /**< (SAMHS_HST_IFR) DMA Channel 6 Interrupt Set Mask */
#define SAMHS_HST_IFR_Msk                    _U_(0xFE00007F)                                /**< (SAMHS_HST_IFR) Register Mask  */

#define SAMHS_HST_IFR_DMA__Pos               25                                             /**< (SAMHS_HST_IFR Position) DMA Channel 6 Interrupt Set */
#define SAMHS_HST_IFR_DMA_                   (_U_(0x7F) << SAMHS_HST_IFR_DMA__Pos)           /**< (SAMHS_HST_IFR Mask) DMA_ */

/* -------- SAMHS_HST_IMR : (USBHS Offset: 0x410) (R/ 32) Host Global Interrupt Mask Register -------- */

#define SAMHS_HST_IMR_OFFSET                 (0x410)                                       /**<  (SAMHS_HST_IMR) Host Global Interrupt Mask Register  Offset */

#define SAMHS_HST_IMR_DCONNIE_Pos            0                                              /**< (SAMHS_HST_IMR) Device Connection Interrupt Enable Position */
#define SAMHS_HST_IMR_DCONNIE                (_U_(0x1) << SAMHS_HST_IMR_DCONNIE_Pos)         /**< (SAMHS_HST_IMR) Device Connection Interrupt Enable Mask */
#define SAMHS_HST_IMR_DDISCIE_Pos            1                                              /**< (SAMHS_HST_IMR) Device Disconnection Interrupt Enable Position */
#define SAMHS_HST_IMR_DDISCIE                (_U_(0x1) << SAMHS_HST_IMR_DDISCIE_Pos)         /**< (SAMHS_HST_IMR) Device Disconnection Interrupt Enable Mask */
#define SAMHS_HST_IMR_RSTIE_Pos              2                                              /**< (SAMHS_HST_IMR) USB Reset Sent Interrupt Enable Position */
#define SAMHS_HST_IMR_RSTIE                  (_U_(0x1) << SAMHS_HST_IMR_RSTIE_Pos)           /**< (SAMHS_HST_IMR) USB Reset Sent Interrupt Enable Mask */
#define SAMHS_HST_IMR_RSMEDIE_Pos            3                                              /**< (SAMHS_HST_IMR) Downstream Resume Sent Interrupt Enable Position */
#define SAMHS_HST_IMR_RSMEDIE                (_U_(0x1) << SAMHS_HST_IMR_RSMEDIE_Pos)         /**< (SAMHS_HST_IMR) Downstream Resume Sent Interrupt Enable Mask */
#define SAMHS_HST_IMR_RXRSMIE_Pos            4                                              /**< (SAMHS_HST_IMR) Upstream Resume Received Interrupt Enable Position */
#define SAMHS_HST_IMR_RXRSMIE                (_U_(0x1) << SAMHS_HST_IMR_RXRSMIE_Pos)         /**< (SAMHS_HST_IMR) Upstream Resume Received Interrupt Enable Mask */
#define SAMHS_HST_IMR_HSOFIE_Pos             5                                              /**< (SAMHS_HST_IMR) Host Start of Frame Interrupt Enable Position */
#define SAMHS_HST_IMR_HSOFIE                 (_U_(0x1) << SAMHS_HST_IMR_HSOFIE_Pos)          /**< (SAMHS_HST_IMR) Host Start of Frame Interrupt Enable Mask */
#define SAMHS_HST_IMR_HWUPIE_Pos             6                                              /**< (SAMHS_HST_IMR) Host Wake-Up Interrupt Enable Position */
#define SAMHS_HST_IMR_HWUPIE                 (_U_(0x1) << SAMHS_HST_IMR_HWUPIE_Pos)          /**< (SAMHS_HST_IMR) Host Wake-Up Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_0_Pos              8                                              /**< (SAMHS_HST_IMR) Pipe 0 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_0                  (_U_(0x1) << SAMHS_HST_IMR_PEP_0_Pos)           /**< (SAMHS_HST_IMR) Pipe 0 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_1_Pos              9                                              /**< (SAMHS_HST_IMR) Pipe 1 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_1                  (_U_(0x1) << SAMHS_HST_IMR_PEP_1_Pos)           /**< (SAMHS_HST_IMR) Pipe 1 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_2_Pos              10                                             /**< (SAMHS_HST_IMR) Pipe 2 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_2                  (_U_(0x1) << SAMHS_HST_IMR_PEP_2_Pos)           /**< (SAMHS_HST_IMR) Pipe 2 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_3_Pos              11                                             /**< (SAMHS_HST_IMR) Pipe 3 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_3                  (_U_(0x1) << SAMHS_HST_IMR_PEP_3_Pos)           /**< (SAMHS_HST_IMR) Pipe 3 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_4_Pos              12                                             /**< (SAMHS_HST_IMR) Pipe 4 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_4                  (_U_(0x1) << SAMHS_HST_IMR_PEP_4_Pos)           /**< (SAMHS_HST_IMR) Pipe 4 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_5_Pos              13                                             /**< (SAMHS_HST_IMR) Pipe 5 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_5                  (_U_(0x1) << SAMHS_HST_IMR_PEP_5_Pos)           /**< (SAMHS_HST_IMR) Pipe 5 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_6_Pos              14                                             /**< (SAMHS_HST_IMR) Pipe 6 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_6                  (_U_(0x1) << SAMHS_HST_IMR_PEP_6_Pos)           /**< (SAMHS_HST_IMR) Pipe 6 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_7_Pos              15                                             /**< (SAMHS_HST_IMR) Pipe 7 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_7                  (_U_(0x1) << SAMHS_HST_IMR_PEP_7_Pos)           /**< (SAMHS_HST_IMR) Pipe 7 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_8_Pos              16                                             /**< (SAMHS_HST_IMR) Pipe 8 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_8                  (_U_(0x1) << SAMHS_HST_IMR_PEP_8_Pos)           /**< (SAMHS_HST_IMR) Pipe 8 Interrupt Enable Mask */
#define SAMHS_HST_IMR_PEP_9_Pos              17                                             /**< (SAMHS_HST_IMR) Pipe 9 Interrupt Enable Position */
#define SAMHS_HST_IMR_PEP_9                  (_U_(0x1) << SAMHS_HST_IMR_PEP_9_Pos)           /**< (SAMHS_HST_IMR) Pipe 9 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_0_Pos              25                                             /**< (SAMHS_HST_IMR) DMA Channel 0 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_0                  (_U_(0x1) << SAMHS_HST_IMR_DMA_0_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 0 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_1_Pos              26                                             /**< (SAMHS_HST_IMR) DMA Channel 1 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_1                  (_U_(0x1) << SAMHS_HST_IMR_DMA_1_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 1 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_2_Pos              27                                             /**< (SAMHS_HST_IMR) DMA Channel 2 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_2                  (_U_(0x1) << SAMHS_HST_IMR_DMA_2_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 2 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_3_Pos              28                                             /**< (SAMHS_HST_IMR) DMA Channel 3 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_3                  (_U_(0x1) << SAMHS_HST_IMR_DMA_3_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 3 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_4_Pos              29                                             /**< (SAMHS_HST_IMR) DMA Channel 4 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_4                  (_U_(0x1) << SAMHS_HST_IMR_DMA_4_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 4 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_5_Pos              30                                             /**< (SAMHS_HST_IMR) DMA Channel 5 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_5                  (_U_(0x1) << SAMHS_HST_IMR_DMA_5_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 5 Interrupt Enable Mask */
#define SAMHS_HST_IMR_DMA_6_Pos              31                                             /**< (SAMHS_HST_IMR) DMA Channel 6 Interrupt Enable Position */
#define SAMHS_HST_IMR_DMA_6                  (_U_(0x1) << SAMHS_HST_IMR_DMA_6_Pos)           /**< (SAMHS_HST_IMR) DMA Channel 6 Interrupt Enable Mask */
#define SAMHS_HST_IMR_Msk                    _U_(0xFE03FF7F)                                /**< (SAMHS_HST_IMR) Register Mask  */

#define SAMHS_HST_IMR_PEP__Pos               8                                              /**< (SAMHS_HST_IMR Position) Pipe x Interrupt Enable */
#define SAMHS_HST_IMR_PEP_                   (_U_(0x3FF) << SAMHS_HST_IMR_PEP__Pos)          /**< (SAMHS_HST_IMR Mask) PEP_ */
#define SAMHS_HST_IMR_DMA__Pos               25                                             /**< (SAMHS_HST_IMR Position) DMA Channel 6 Interrupt Enable */
#define SAMHS_HST_IMR_DMA_                   (_U_(0x7F) << SAMHS_HST_IMR_DMA__Pos)           /**< (SAMHS_HST_IMR Mask) DMA_ */

/* -------- SAMHS_HST_IDR : (USBHS Offset: 0x414) (/W 32) Host Global Interrupt Disable Register -------- */

#define SAMHS_HST_IDR_OFFSET                 (0x414)                                       /**<  (SAMHS_HST_IDR) Host Global Interrupt Disable Register  Offset */

#define SAMHS_HST_IDR_DCONNIEC_Pos           0                                              /**< (SAMHS_HST_IDR) Device Connection Interrupt Disable Position */
#define SAMHS_HST_IDR_DCONNIEC               (_U_(0x1) << SAMHS_HST_IDR_DCONNIEC_Pos)        /**< (SAMHS_HST_IDR) Device Connection Interrupt Disable Mask */
#define SAMHS_HST_IDR_DDISCIEC_Pos           1                                              /**< (SAMHS_HST_IDR) Device Disconnection Interrupt Disable Position */
#define SAMHS_HST_IDR_DDISCIEC               (_U_(0x1) << SAMHS_HST_IDR_DDISCIEC_Pos)        /**< (SAMHS_HST_IDR) Device Disconnection Interrupt Disable Mask */
#define SAMHS_HST_IDR_RSTIEC_Pos             2                                              /**< (SAMHS_HST_IDR) USB Reset Sent Interrupt Disable Position */
#define SAMHS_HST_IDR_RSTIEC                 (_U_(0x1) << SAMHS_HST_IDR_RSTIEC_Pos)          /**< (SAMHS_HST_IDR) USB Reset Sent Interrupt Disable Mask */
#define SAMHS_HST_IDR_RSMEDIEC_Pos           3                                              /**< (SAMHS_HST_IDR) Downstream Resume Sent Interrupt Disable Position */
#define SAMHS_HST_IDR_RSMEDIEC               (_U_(0x1) << SAMHS_HST_IDR_RSMEDIEC_Pos)        /**< (SAMHS_HST_IDR) Downstream Resume Sent Interrupt Disable Mask */
#define SAMHS_HST_IDR_RXRSMIEC_Pos           4                                              /**< (SAMHS_HST_IDR) Upstream Resume Received Interrupt Disable Position */
#define SAMHS_HST_IDR_RXRSMIEC               (_U_(0x1) << SAMHS_HST_IDR_RXRSMIEC_Pos)        /**< (SAMHS_HST_IDR) Upstream Resume Received Interrupt Disable Mask */
#define SAMHS_HST_IDR_HSOFIEC_Pos            5                                              /**< (SAMHS_HST_IDR) Host Start of Frame Interrupt Disable Position */
#define SAMHS_HST_IDR_HSOFIEC                (_U_(0x1) << SAMHS_HST_IDR_HSOFIEC_Pos)         /**< (SAMHS_HST_IDR) Host Start of Frame Interrupt Disable Mask */
#define SAMHS_HST_IDR_HWUPIEC_Pos            6                                              /**< (SAMHS_HST_IDR) Host Wake-Up Interrupt Disable Position */
#define SAMHS_HST_IDR_HWUPIEC                (_U_(0x1) << SAMHS_HST_IDR_HWUPIEC_Pos)         /**< (SAMHS_HST_IDR) Host Wake-Up Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_0_Pos              8                                              /**< (SAMHS_HST_IDR) Pipe 0 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_0                  (_U_(0x1) << SAMHS_HST_IDR_PEP_0_Pos)           /**< (SAMHS_HST_IDR) Pipe 0 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_1_Pos              9                                              /**< (SAMHS_HST_IDR) Pipe 1 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_1                  (_U_(0x1) << SAMHS_HST_IDR_PEP_1_Pos)           /**< (SAMHS_HST_IDR) Pipe 1 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_2_Pos              10                                             /**< (SAMHS_HST_IDR) Pipe 2 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_2                  (_U_(0x1) << SAMHS_HST_IDR_PEP_2_Pos)           /**< (SAMHS_HST_IDR) Pipe 2 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_3_Pos              11                                             /**< (SAMHS_HST_IDR) Pipe 3 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_3                  (_U_(0x1) << SAMHS_HST_IDR_PEP_3_Pos)           /**< (SAMHS_HST_IDR) Pipe 3 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_4_Pos              12                                             /**< (SAMHS_HST_IDR) Pipe 4 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_4                  (_U_(0x1) << SAMHS_HST_IDR_PEP_4_Pos)           /**< (SAMHS_HST_IDR) Pipe 4 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_5_Pos              13                                             /**< (SAMHS_HST_IDR) Pipe 5 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_5                  (_U_(0x1) << SAMHS_HST_IDR_PEP_5_Pos)           /**< (SAMHS_HST_IDR) Pipe 5 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_6_Pos              14                                             /**< (SAMHS_HST_IDR) Pipe 6 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_6                  (_U_(0x1) << SAMHS_HST_IDR_PEP_6_Pos)           /**< (SAMHS_HST_IDR) Pipe 6 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_7_Pos              15                                             /**< (SAMHS_HST_IDR) Pipe 7 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_7                  (_U_(0x1) << SAMHS_HST_IDR_PEP_7_Pos)           /**< (SAMHS_HST_IDR) Pipe 7 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_8_Pos              16                                             /**< (SAMHS_HST_IDR) Pipe 8 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_8                  (_U_(0x1) << SAMHS_HST_IDR_PEP_8_Pos)           /**< (SAMHS_HST_IDR) Pipe 8 Interrupt Disable Mask */
#define SAMHS_HST_IDR_PEP_9_Pos              17                                             /**< (SAMHS_HST_IDR) Pipe 9 Interrupt Disable Position */
#define SAMHS_HST_IDR_PEP_9                  (_U_(0x1) << SAMHS_HST_IDR_PEP_9_Pos)           /**< (SAMHS_HST_IDR) Pipe 9 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_0_Pos              25                                             /**< (SAMHS_HST_IDR) DMA Channel 0 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_0                  (_U_(0x1) << SAMHS_HST_IDR_DMA_0_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 0 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_1_Pos              26                                             /**< (SAMHS_HST_IDR) DMA Channel 1 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_1                  (_U_(0x1) << SAMHS_HST_IDR_DMA_1_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 1 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_2_Pos              27                                             /**< (SAMHS_HST_IDR) DMA Channel 2 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_2                  (_U_(0x1) << SAMHS_HST_IDR_DMA_2_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 2 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_3_Pos              28                                             /**< (SAMHS_HST_IDR) DMA Channel 3 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_3                  (_U_(0x1) << SAMHS_HST_IDR_DMA_3_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 3 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_4_Pos              29                                             /**< (SAMHS_HST_IDR) DMA Channel 4 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_4                  (_U_(0x1) << SAMHS_HST_IDR_DMA_4_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 4 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_5_Pos              30                                             /**< (SAMHS_HST_IDR) DMA Channel 5 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_5                  (_U_(0x1) << SAMHS_HST_IDR_DMA_5_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 5 Interrupt Disable Mask */
#define SAMHS_HST_IDR_DMA_6_Pos              31                                             /**< (SAMHS_HST_IDR) DMA Channel 6 Interrupt Disable Position */
#define SAMHS_HST_IDR_DMA_6                  (_U_(0x1) << SAMHS_HST_IDR_DMA_6_Pos)           /**< (SAMHS_HST_IDR) DMA Channel 6 Interrupt Disable Mask */
#define SAMHS_HST_IDR_Msk                    _U_(0xFE03FF7F)                                /**< (SAMHS_HST_IDR) Register Mask  */

#define SAMHS_HST_IDR_PEP__Pos               8                                              /**< (SAMHS_HST_IDR Position) Pipe x Interrupt Disable */
#define SAMHS_HST_IDR_PEP_                   (_U_(0x3FF) << SAMHS_HST_IDR_PEP__Pos)          /**< (SAMHS_HST_IDR Mask) PEP_ */
#define SAMHS_HST_IDR_DMA__Pos               25                                             /**< (SAMHS_HST_IDR Position) DMA Channel 6 Interrupt Disable */
#define SAMHS_HST_IDR_DMA_                   (_U_(0x7F) << SAMHS_HST_IDR_DMA__Pos)           /**< (SAMHS_HST_IDR Mask) DMA_ */

/* -------- SAMHS_HST_IER : (USBHS Offset: 0x418) (/W 32) Host Global Interrupt Enable Register -------- */

#define SAMHS_HST_IER_OFFSET                 (0x418)                                       /**<  (SAMHS_HST_IER) Host Global Interrupt Enable Register  Offset */

#define SAMHS_HST_IER_DCONNIES_Pos           0                                              /**< (SAMHS_HST_IER) Device Connection Interrupt Enable Position */
#define SAMHS_HST_IER_DCONNIES               (_U_(0x1) << SAMHS_HST_IER_DCONNIES_Pos)        /**< (SAMHS_HST_IER) Device Connection Interrupt Enable Mask */
#define SAMHS_HST_IER_DDISCIES_Pos           1                                              /**< (SAMHS_HST_IER) Device Disconnection Interrupt Enable Position */
#define SAMHS_HST_IER_DDISCIES               (_U_(0x1) << SAMHS_HST_IER_DDISCIES_Pos)        /**< (SAMHS_HST_IER) Device Disconnection Interrupt Enable Mask */
#define SAMHS_HST_IER_RSTIES_Pos             2                                              /**< (SAMHS_HST_IER) USB Reset Sent Interrupt Enable Position */
#define SAMHS_HST_IER_RSTIES                 (_U_(0x1) << SAMHS_HST_IER_RSTIES_Pos)          /**< (SAMHS_HST_IER) USB Reset Sent Interrupt Enable Mask */
#define SAMHS_HST_IER_RSMEDIES_Pos           3                                              /**< (SAMHS_HST_IER) Downstream Resume Sent Interrupt Enable Position */
#define SAMHS_HST_IER_RSMEDIES               (_U_(0x1) << SAMHS_HST_IER_RSMEDIES_Pos)        /**< (SAMHS_HST_IER) Downstream Resume Sent Interrupt Enable Mask */
#define SAMHS_HST_IER_RXRSMIES_Pos           4                                              /**< (SAMHS_HST_IER) Upstream Resume Received Interrupt Enable Position */
#define SAMHS_HST_IER_RXRSMIES               (_U_(0x1) << SAMHS_HST_IER_RXRSMIES_Pos)        /**< (SAMHS_HST_IER) Upstream Resume Received Interrupt Enable Mask */
#define SAMHS_HST_IER_HSOFIES_Pos            5                                              /**< (SAMHS_HST_IER) Host Start of Frame Interrupt Enable Position */
#define SAMHS_HST_IER_HSOFIES                (_U_(0x1) << SAMHS_HST_IER_HSOFIES_Pos)         /**< (SAMHS_HST_IER) Host Start of Frame Interrupt Enable Mask */
#define SAMHS_HST_IER_HWUPIES_Pos            6                                              /**< (SAMHS_HST_IER) Host Wake-Up Interrupt Enable Position */
#define SAMHS_HST_IER_HWUPIES                (_U_(0x1) << SAMHS_HST_IER_HWUPIES_Pos)         /**< (SAMHS_HST_IER) Host Wake-Up Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_0_Pos              8                                              /**< (SAMHS_HST_IER) Pipe 0 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_0                  (_U_(0x1) << SAMHS_HST_IER_PEP_0_Pos)           /**< (SAMHS_HST_IER) Pipe 0 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_1_Pos              9                                              /**< (SAMHS_HST_IER) Pipe 1 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_1                  (_U_(0x1) << SAMHS_HST_IER_PEP_1_Pos)           /**< (SAMHS_HST_IER) Pipe 1 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_2_Pos              10                                             /**< (SAMHS_HST_IER) Pipe 2 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_2                  (_U_(0x1) << SAMHS_HST_IER_PEP_2_Pos)           /**< (SAMHS_HST_IER) Pipe 2 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_3_Pos              11                                             /**< (SAMHS_HST_IER) Pipe 3 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_3                  (_U_(0x1) << SAMHS_HST_IER_PEP_3_Pos)           /**< (SAMHS_HST_IER) Pipe 3 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_4_Pos              12                                             /**< (SAMHS_HST_IER) Pipe 4 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_4                  (_U_(0x1) << SAMHS_HST_IER_PEP_4_Pos)           /**< (SAMHS_HST_IER) Pipe 4 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_5_Pos              13                                             /**< (SAMHS_HST_IER) Pipe 5 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_5                  (_U_(0x1) << SAMHS_HST_IER_PEP_5_Pos)           /**< (SAMHS_HST_IER) Pipe 5 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_6_Pos              14                                             /**< (SAMHS_HST_IER) Pipe 6 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_6                  (_U_(0x1) << SAMHS_HST_IER_PEP_6_Pos)           /**< (SAMHS_HST_IER) Pipe 6 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_7_Pos              15                                             /**< (SAMHS_HST_IER) Pipe 7 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_7                  (_U_(0x1) << SAMHS_HST_IER_PEP_7_Pos)           /**< (SAMHS_HST_IER) Pipe 7 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_8_Pos              16                                             /**< (SAMHS_HST_IER) Pipe 8 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_8                  (_U_(0x1) << SAMHS_HST_IER_PEP_8_Pos)           /**< (SAMHS_HST_IER) Pipe 8 Interrupt Enable Mask */
#define SAMHS_HST_IER_PEP_9_Pos              17                                             /**< (SAMHS_HST_IER) Pipe 9 Interrupt Enable Position */
#define SAMHS_HST_IER_PEP_9                  (_U_(0x1) << SAMHS_HST_IER_PEP_9_Pos)           /**< (SAMHS_HST_IER) Pipe 9 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_0_Pos              25                                             /**< (SAMHS_HST_IER) DMA Channel 0 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_0                  (_U_(0x1) << SAMHS_HST_IER_DMA_0_Pos)           /**< (SAMHS_HST_IER) DMA Channel 0 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_1_Pos              26                                             /**< (SAMHS_HST_IER) DMA Channel 1 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_1                  (_U_(0x1) << SAMHS_HST_IER_DMA_1_Pos)           /**< (SAMHS_HST_IER) DMA Channel 1 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_2_Pos              27                                             /**< (SAMHS_HST_IER) DMA Channel 2 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_2                  (_U_(0x1) << SAMHS_HST_IER_DMA_2_Pos)           /**< (SAMHS_HST_IER) DMA Channel 2 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_3_Pos              28                                             /**< (SAMHS_HST_IER) DMA Channel 3 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_3                  (_U_(0x1) << SAMHS_HST_IER_DMA_3_Pos)           /**< (SAMHS_HST_IER) DMA Channel 3 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_4_Pos              29                                             /**< (SAMHS_HST_IER) DMA Channel 4 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_4                  (_U_(0x1) << SAMHS_HST_IER_DMA_4_Pos)           /**< (SAMHS_HST_IER) DMA Channel 4 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_5_Pos              30                                             /**< (SAMHS_HST_IER) DMA Channel 5 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_5                  (_U_(0x1) << SAMHS_HST_IER_DMA_5_Pos)           /**< (SAMHS_HST_IER) DMA Channel 5 Interrupt Enable Mask */
#define SAMHS_HST_IER_DMA_6_Pos              31                                             /**< (SAMHS_HST_IER) DMA Channel 6 Interrupt Enable Position */
#define SAMHS_HST_IER_DMA_6                  (_U_(0x1) << SAMHS_HST_IER_DMA_6_Pos)           /**< (SAMHS_HST_IER) DMA Channel 6 Interrupt Enable Mask */
#define SAMHS_HST_IER_Msk                    _U_(0xFE03FF7F)                                /**< (SAMHS_HST_IER) Register Mask  */

#define SAMHS_HST_IER_PEP__Pos               8                                              /**< (SAMHS_HST_IER Position) Pipe x Interrupt Enable */
#define SAMHS_HST_IER_PEP_                   (_U_(0x3FF) << SAMHS_HST_IER_PEP__Pos)          /**< (SAMHS_HST_IER Mask) PEP_ */
#define SAMHS_HST_IER_DMA__Pos               25                                             /**< (SAMHS_HST_IER Position) DMA Channel 6 Interrupt Enable */
#define SAMHS_HST_IER_DMA_                   (_U_(0x7F) << SAMHS_HST_IER_DMA__Pos)           /**< (SAMHS_HST_IER Mask) DMA_ */

/* -------- SAMHS_HST_PIP : (USBHS Offset: 0x41c) (R/W 32) Host Pipe Register -------- */

#define SAMHS_HST_PIP_OFFSET                 (0x41C)                                       /**<  (SAMHS_HST_PIP) Host Pipe Register  Offset */

#define SAMHS_HST_PIP_PEN0_Pos               0                                              /**< (SAMHS_HST_PIP) Pipe 0 Enable Position */
#define SAMHS_HST_PIP_PEN0                   (_U_(0x1) << SAMHS_HST_PIP_PEN0_Pos)            /**< (SAMHS_HST_PIP) Pipe 0 Enable Mask */
#define SAMHS_HST_PIP_PEN1_Pos               1                                              /**< (SAMHS_HST_PIP) Pipe 1 Enable Position */
#define SAMHS_HST_PIP_PEN1                   (_U_(0x1) << SAMHS_HST_PIP_PEN1_Pos)            /**< (SAMHS_HST_PIP) Pipe 1 Enable Mask */
#define SAMHS_HST_PIP_PEN2_Pos               2                                              /**< (SAMHS_HST_PIP) Pipe 2 Enable Position */
#define SAMHS_HST_PIP_PEN2                   (_U_(0x1) << SAMHS_HST_PIP_PEN2_Pos)            /**< (SAMHS_HST_PIP) Pipe 2 Enable Mask */
#define SAMHS_HST_PIP_PEN3_Pos               3                                              /**< (SAMHS_HST_PIP) Pipe 3 Enable Position */
#define SAMHS_HST_PIP_PEN3                   (_U_(0x1) << SAMHS_HST_PIP_PEN3_Pos)            /**< (SAMHS_HST_PIP) Pipe 3 Enable Mask */
#define SAMHS_HST_PIP_PEN4_Pos               4                                              /**< (SAMHS_HST_PIP) Pipe 4 Enable Position */
#define SAMHS_HST_PIP_PEN4                   (_U_(0x1) << SAMHS_HST_PIP_PEN4_Pos)            /**< (SAMHS_HST_PIP) Pipe 4 Enable Mask */
#define SAMHS_HST_PIP_PEN5_Pos               5                                              /**< (SAMHS_HST_PIP) Pipe 5 Enable Position */
#define SAMHS_HST_PIP_PEN5                   (_U_(0x1) << SAMHS_HST_PIP_PEN5_Pos)            /**< (SAMHS_HST_PIP) Pipe 5 Enable Mask */
#define SAMHS_HST_PIP_PEN6_Pos               6                                              /**< (SAMHS_HST_PIP) Pipe 6 Enable Position */
#define SAMHS_HST_PIP_PEN6                   (_U_(0x1) << SAMHS_HST_PIP_PEN6_Pos)            /**< (SAMHS_HST_PIP) Pipe 6 Enable Mask */
#define SAMHS_HST_PIP_PEN7_Pos               7                                              /**< (SAMHS_HST_PIP) Pipe 7 Enable Position */
#define SAMHS_HST_PIP_PEN7                   (_U_(0x1) << SAMHS_HST_PIP_PEN7_Pos)            /**< (SAMHS_HST_PIP) Pipe 7 Enable Mask */
#define SAMHS_HST_PIP_PEN8_Pos               8                                              /**< (SAMHS_HST_PIP) Pipe 8 Enable Position */
#define SAMHS_HST_PIP_PEN8                   (_U_(0x1) << SAMHS_HST_PIP_PEN8_Pos)            /**< (SAMHS_HST_PIP) Pipe 8 Enable Mask */
#define SAMHS_HST_PIP_PRST0_Pos              16                                             /**< (SAMHS_HST_PIP) Pipe 0 Reset Position */
#define SAMHS_HST_PIP_PRST0                  (_U_(0x1) << SAMHS_HST_PIP_PRST0_Pos)           /**< (SAMHS_HST_PIP) Pipe 0 Reset Mask */
#define SAMHS_HST_PIP_PRST1_Pos              17                                             /**< (SAMHS_HST_PIP) Pipe 1 Reset Position */
#define SAMHS_HST_PIP_PRST1                  (_U_(0x1) << SAMHS_HST_PIP_PRST1_Pos)           /**< (SAMHS_HST_PIP) Pipe 1 Reset Mask */
#define SAMHS_HST_PIP_PRST2_Pos              18                                             /**< (SAMHS_HST_PIP) Pipe 2 Reset Position */
#define SAMHS_HST_PIP_PRST2                  (_U_(0x1) << SAMHS_HST_PIP_PRST2_Pos)           /**< (SAMHS_HST_PIP) Pipe 2 Reset Mask */
#define SAMHS_HST_PIP_PRST3_Pos              19                                             /**< (SAMHS_HST_PIP) Pipe 3 Reset Position */
#define SAMHS_HST_PIP_PRST3                  (_U_(0x1) << SAMHS_HST_PIP_PRST3_Pos)           /**< (SAMHS_HST_PIP) Pipe 3 Reset Mask */
#define SAMHS_HST_PIP_PRST4_Pos              20                                             /**< (SAMHS_HST_PIP) Pipe 4 Reset Position */
#define SAMHS_HST_PIP_PRST4                  (_U_(0x1) << SAMHS_HST_PIP_PRST4_Pos)           /**< (SAMHS_HST_PIP) Pipe 4 Reset Mask */
#define SAMHS_HST_PIP_PRST5_Pos              21                                             /**< (SAMHS_HST_PIP) Pipe 5 Reset Position */
#define SAMHS_HST_PIP_PRST5                  (_U_(0x1) << SAMHS_HST_PIP_PRST5_Pos)           /**< (SAMHS_HST_PIP) Pipe 5 Reset Mask */
#define SAMHS_HST_PIP_PRST6_Pos              22                                             /**< (SAMHS_HST_PIP) Pipe 6 Reset Position */
#define SAMHS_HST_PIP_PRST6                  (_U_(0x1) << SAMHS_HST_PIP_PRST6_Pos)           /**< (SAMHS_HST_PIP) Pipe 6 Reset Mask */
#define SAMHS_HST_PIP_PRST7_Pos              23                                             /**< (SAMHS_HST_PIP) Pipe 7 Reset Position */
#define SAMHS_HST_PIP_PRST7                  (_U_(0x1) << SAMHS_HST_PIP_PRST7_Pos)           /**< (SAMHS_HST_PIP) Pipe 7 Reset Mask */
#define SAMHS_HST_PIP_PRST8_Pos              24                                             /**< (SAMHS_HST_PIP) Pipe 8 Reset Position */
#define SAMHS_HST_PIP_PRST8                  (_U_(0x1) << SAMHS_HST_PIP_PRST8_Pos)           /**< (SAMHS_HST_PIP) Pipe 8 Reset Mask */
#define SAMHS_HST_PIP_Msk                    _U_(0x1FF01FF)                                 /**< (SAMHS_HST_PIP) Register Mask  */

#define SAMHS_HST_PIP_PEN_Pos                0                                              /**< (SAMHS_HST_PIP Position) Pipe x Enable */
#define SAMHS_HST_PIP_PEN                    (_U_(0x1FF) << SAMHS_HST_PIP_PEN_Pos)           /**< (SAMHS_HST_PIP Mask) PEN */
#define SAMHS_HST_PIP_PRST_Pos               16                                             /**< (SAMHS_HST_PIP Position) Pipe 8 Reset */
#define SAMHS_HST_PIP_PRST                   (_U_(0x1FF) << SAMHS_HST_PIP_PRST_Pos)          /**< (SAMHS_HST_PIP Mask) PRST */

/* -------- SAMHS_HST_FNUM : (USBHS Offset: 0x420) (R/W 32) Host Frame Number Register -------- */

#define SAMHS_HST_FNUM_OFFSET                (0x420)                                       /**<  (SAMHS_HST_FNUM) Host Frame Number Register  Offset */

#define SAMHS_HST_FNUM_MFNUM_Pos             0                                              /**< (SAMHS_HST_FNUM) Micro Frame Number Position */
#define SAMHS_HST_FNUM_MFNUM                 (_U_(0x7) << SAMHS_HST_FNUM_MFNUM_Pos)          /**< (SAMHS_HST_FNUM) Micro Frame Number Mask */
#define SAMHS_HST_FNUM_FNUM_Pos              3                                              /**< (SAMHS_HST_FNUM) Frame Number Position */
#define SAMHS_HST_FNUM_FNUM                  (_U_(0x7FF) << SAMHS_HST_FNUM_FNUM_Pos)         /**< (SAMHS_HST_FNUM) Frame Number Mask */
#define SAMHS_HST_FNUM_FLENHIGH_Pos          16                                             /**< (SAMHS_HST_FNUM) Frame Length Position */
#define SAMHS_HST_FNUM_FLENHIGH              (_U_(0xFF) << SAMHS_HST_FNUM_FLENHIGH_Pos)      /**< (SAMHS_HST_FNUM) Frame Length Mask */
#define SAMHS_HST_FNUM_Msk                   _U_(0xFF3FFF)                                  /**< (SAMHS_HST_FNUM) Register Mask  */


/* -------- SAMHS_HST_ADDR1 : (USBHS Offset: 0x424) (R/W 32) Host Address 1 Register -------- */

#define SAMHS_HST_ADDR1_OFFSET               (0x424)                                       /**<  (SAMHS_HST_ADDR1) Host Address 1 Register  Offset */

#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP0_Pos        0                                              /**< (SAMHS_HST_ADDR1) USB Host Address Position */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP0            (_U_(0x7F) << SAMHS_HST_ADDR1_SAMHS_HST_ADDRP0_Pos)    /**< (SAMHS_HST_ADDR1) USB Host Address Mask */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP1_Pos        8                                              /**< (SAMHS_HST_ADDR1) USB Host Address Position */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP1            (_U_(0x7F) << SAMHS_HST_ADDR1_SAMHS_HST_ADDRP1_Pos)    /**< (SAMHS_HST_ADDR1) USB Host Address Mask */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP2_Pos        16                                             /**< (SAMHS_HST_ADDR1) USB Host Address Position */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP2            (_U_(0x7F) << SAMHS_HST_ADDR1_SAMHS_HST_ADDRP2_Pos)    /**< (SAMHS_HST_ADDR1) USB Host Address Mask */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP3_Pos        24                                             /**< (SAMHS_HST_ADDR1) USB Host Address Position */
#define SAMHS_HST_ADDR1_SAMHS_HST_ADDRP3            (_U_(0x7F) << SAMHS_HST_ADDR1_SAMHS_HST_ADDRP3_Pos)    /**< (SAMHS_HST_ADDR1) USB Host Address Mask */
#define SAMHS_HST_ADDR1_Msk                  _U_(0x7F7F7F7F)                                /**< (SAMHS_HST_ADDR1) Register Mask  */


/* -------- SAMHS_HST_ADDR2 : (USBHS Offset: 0x428) (R/W 32) Host Address 2 Register -------- */

#define SAMHS_HST_ADDR2_OFFSET               (0x428)                                       /**<  (SAMHS_HST_ADDR2) Host Address 2 Register  Offset */

#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP4_Pos        0                                              /**< (SAMHS_HST_ADDR2) USB Host Address Position */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP4            (_U_(0x7F) << SAMHS_HST_ADDR2_SAMHS_HST_ADDRP4_Pos)    /**< (SAMHS_HST_ADDR2) USB Host Address Mask */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP5_Pos        8                                              /**< (SAMHS_HST_ADDR2) USB Host Address Position */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP5            (_U_(0x7F) << SAMHS_HST_ADDR2_SAMHS_HST_ADDRP5_Pos)    /**< (SAMHS_HST_ADDR2) USB Host Address Mask */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP6_Pos        16                                             /**< (SAMHS_HST_ADDR2) USB Host Address Position */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP6            (_U_(0x7F) << SAMHS_HST_ADDR2_SAMHS_HST_ADDRP6_Pos)    /**< (SAMHS_HST_ADDR2) USB Host Address Mask */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP7_Pos        24                                             /**< (SAMHS_HST_ADDR2) USB Host Address Position */
#define SAMHS_HST_ADDR2_SAMHS_HST_ADDRP7            (_U_(0x7F) << SAMHS_HST_ADDR2_SAMHS_HST_ADDRP7_Pos)    /**< (SAMHS_HST_ADDR2) USB Host Address Mask */
#define SAMHS_HST_ADDR2_Msk                  _U_(0x7F7F7F7F)                                /**< (SAMHS_HST_ADDR2) Register Mask  */


/* -------- SAMHS_HST_ADDR3 : (USBHS Offset: 0x42c) (R/W 32) Host Address 3 Register -------- */

#define SAMHS_HST_ADDR3_OFFSET               (0x42C)                                       /**<  (SAMHS_HST_ADDR3) Host Address 3 Register  Offset */

#define SAMHS_HST_ADDR3_SAMHS_HST_ADDRP8_Pos        0                                              /**< (SAMHS_HST_ADDR3) USB Host Address Position */
#define SAMHS_HST_ADDR3_SAMHS_HST_ADDRP8            (_U_(0x7F) << SAMHS_HST_ADDR3_SAMHS_HST_ADDRP8_Pos)    /**< (SAMHS_HST_ADDR3) USB Host Address Mask */
#define SAMHS_HST_ADDR3_SAMHS_HST_ADDRP9_Pos        8                                              /**< (SAMHS_HST_ADDR3) USB Host Address Position */
#define SAMHS_HST_ADDR3_SAMHS_HST_ADDRP9            (_U_(0x7F) << SAMHS_HST_ADDR3_SAMHS_HST_ADDRP9_Pos)    /**< (SAMHS_HST_ADDR3) USB Host Address Mask */
#define SAMHS_HST_ADDR3_Msk                  _U_(0x7F7F)                                    /**< (SAMHS_HST_ADDR3) Register Mask  */


/* -------- SAMHS_HST_PIPCFG : (USBHS Offset: 0x500) (R/W 32) Host Pipe Configuration Register -------- */

#define SAMHS_HST_PIPCFG_OFFSET              (0x500)                                       /**<  (SAMHS_HST_PIPCFG) Host Pipe Configuration Register  Offset */

#define SAMHS_HST_PIPCFG_ALLOC_Pos           1                                              /**< (SAMHS_HST_PIPCFG) Pipe Memory Allocate Position */
#define SAMHS_HST_PIPCFG_ALLOC               (_U_(0x1) << SAMHS_HST_PIPCFG_ALLOC_Pos)        /**< (SAMHS_HST_PIPCFG) Pipe Memory Allocate Mask */
#define SAMHS_HST_PIPCFG_PBK_Pos             2                                              /**< (SAMHS_HST_PIPCFG) Pipe Banks Position */
#define SAMHS_HST_PIPCFG_PBK                 (_U_(0x3) << SAMHS_HST_PIPCFG_PBK_Pos)          /**< (SAMHS_HST_PIPCFG) Pipe Banks Mask */
#define   SAMHS_HST_PIPCFG_PBK_1_BANK_Val    _U_(0x0)                                       /**< (SAMHS_HST_PIPCFG) Single-bank pipe  */
#define   SAMHS_HST_PIPCFG_PBK_2_BANK_Val    _U_(0x1)                                       /**< (SAMHS_HST_PIPCFG) Double-bank pipe  */
#define   SAMHS_HST_PIPCFG_PBK_3_BANK_Val    _U_(0x2)                                       /**< (SAMHS_HST_PIPCFG) Triple-bank pipe  */
#define SAMHS_HST_PIPCFG_PBK_1_BANK          (SAMHS_HST_PIPCFG_PBK_1_BANK_Val << SAMHS_HST_PIPCFG_PBK_Pos)  /**< (SAMHS_HST_PIPCFG) Single-bank pipe Position  */
#define SAMHS_HST_PIPCFG_PBK_2_BANK          (SAMHS_HST_PIPCFG_PBK_2_BANK_Val << SAMHS_HST_PIPCFG_PBK_Pos)  /**< (SAMHS_HST_PIPCFG) Double-bank pipe Position  */
#define SAMHS_HST_PIPCFG_PBK_3_BANK          (SAMHS_HST_PIPCFG_PBK_3_BANK_Val << SAMHS_HST_PIPCFG_PBK_Pos)  /**< (SAMHS_HST_PIPCFG) Triple-bank pipe Position  */
#define SAMHS_HST_PIPCFG_PSIZE_Pos           4                                              /**< (SAMHS_HST_PIPCFG) Pipe Size Position */
#define SAMHS_HST_PIPCFG_PSIZE               (_U_(0x7) << SAMHS_HST_PIPCFG_PSIZE_Pos)        /**< (SAMHS_HST_PIPCFG) Pipe Size Mask */
#define   SAMHS_HST_PIPCFG_PSIZE_8_BYTE_Val  _U_(0x0)                                       /**< (SAMHS_HST_PIPCFG) 8 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_16_BYTE_Val _U_(0x1)                                       /**< (SAMHS_HST_PIPCFG) 16 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_32_BYTE_Val _U_(0x2)                                       /**< (SAMHS_HST_PIPCFG) 32 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_64_BYTE_Val _U_(0x3)                                       /**< (SAMHS_HST_PIPCFG) 64 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_128_BYTE_Val _U_(0x4)                                       /**< (SAMHS_HST_PIPCFG) 128 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_256_BYTE_Val _U_(0x5)                                       /**< (SAMHS_HST_PIPCFG) 256 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_512_BYTE_Val _U_(0x6)                                       /**< (SAMHS_HST_PIPCFG) 512 bytes  */
#define   SAMHS_HST_PIPCFG_PSIZE_1024_BYTE_Val _U_(0x7)                                       /**< (SAMHS_HST_PIPCFG) 1024 bytes  */
#define SAMHS_HST_PIPCFG_PSIZE_8_BYTE        (SAMHS_HST_PIPCFG_PSIZE_8_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 8 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_16_BYTE       (SAMHS_HST_PIPCFG_PSIZE_16_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 16 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_32_BYTE       (SAMHS_HST_PIPCFG_PSIZE_32_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 32 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_64_BYTE       (SAMHS_HST_PIPCFG_PSIZE_64_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 64 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_128_BYTE      (SAMHS_HST_PIPCFG_PSIZE_128_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 128 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_256_BYTE      (SAMHS_HST_PIPCFG_PSIZE_256_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 256 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_512_BYTE      (SAMHS_HST_PIPCFG_PSIZE_512_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 512 bytes Position  */
#define SAMHS_HST_PIPCFG_PSIZE_1024_BYTE     (SAMHS_HST_PIPCFG_PSIZE_1024_BYTE_Val << SAMHS_HST_PIPCFG_PSIZE_Pos)  /**< (SAMHS_HST_PIPCFG) 1024 bytes Position  */
#define SAMHS_HST_PIPCFG_PTOKEN_Pos          8                                              /**< (SAMHS_HST_PIPCFG) Pipe Token Position */
#define SAMHS_HST_PIPCFG_PTOKEN              (_U_(0x3) << SAMHS_HST_PIPCFG_PTOKEN_Pos)       /**< (SAMHS_HST_PIPCFG) Pipe Token Mask */
#define   SAMHS_HST_PIPCFG_PTOKEN_SETUP_Val  _U_(0x0)                                       /**< (SAMHS_HST_PIPCFG) SETUP  */
#define   SAMHS_HST_PIPCFG_PTOKEN_IN_Val     _U_(0x1)                                       /**< (SAMHS_HST_PIPCFG) IN  */
#define   SAMHS_HST_PIPCFG_PTOKEN_OUT_Val    _U_(0x2)                                       /**< (SAMHS_HST_PIPCFG) OUT  */
#define SAMHS_HST_PIPCFG_PTOKEN_SETUP        (SAMHS_HST_PIPCFG_PTOKEN_SETUP_Val << SAMHS_HST_PIPCFG_PTOKEN_Pos)  /**< (SAMHS_HST_PIPCFG) SETUP Position  */
#define SAMHS_HST_PIPCFG_PTOKEN_IN           (SAMHS_HST_PIPCFG_PTOKEN_IN_Val << SAMHS_HST_PIPCFG_PTOKEN_Pos)  /**< (SAMHS_HST_PIPCFG) IN Position  */
#define SAMHS_HST_PIPCFG_PTOKEN_OUT          (SAMHS_HST_PIPCFG_PTOKEN_OUT_Val << SAMHS_HST_PIPCFG_PTOKEN_Pos)  /**< (SAMHS_HST_PIPCFG) OUT Position  */
#define SAMHS_HST_PIPCFG_AUTOSW_Pos          10                                             /**< (SAMHS_HST_PIPCFG) Automatic Switch Position */
#define SAMHS_HST_PIPCFG_AUTOSW              (_U_(0x1) << SAMHS_HST_PIPCFG_AUTOSW_Pos)       /**< (SAMHS_HST_PIPCFG) Automatic Switch Mask */
#define SAMHS_HST_PIPCFG_PTYPE_Pos           12                                             /**< (SAMHS_HST_PIPCFG) Pipe Type Position */
#define SAMHS_HST_PIPCFG_PTYPE               (_U_(0x3) << SAMHS_HST_PIPCFG_PTYPE_Pos)        /**< (SAMHS_HST_PIPCFG) Pipe Type Mask */
#define   SAMHS_HST_PIPCFG_PTYPE_CTRL_Val    _U_(0x0)                                       /**< (SAMHS_HST_PIPCFG) Control  */
#define   SAMHS_HST_PIPCFG_PTYPE_ISO_Val     _U_(0x1)                                       /**< (SAMHS_HST_PIPCFG) Isochronous  */
#define   SAMHS_HST_PIPCFG_PTYPE_BLK_Val     _U_(0x2)                                       /**< (SAMHS_HST_PIPCFG) Bulk  */
#define   SAMHS_HST_PIPCFG_PTYPE_INTRPT_Val  _U_(0x3)                                       /**< (SAMHS_HST_PIPCFG) Interrupt  */
#define SAMHS_HST_PIPCFG_PTYPE_CTRL          (SAMHS_HST_PIPCFG_PTYPE_CTRL_Val << SAMHS_HST_PIPCFG_PTYPE_Pos)  /**< (SAMHS_HST_PIPCFG) Control Position  */
#define SAMHS_HST_PIPCFG_PTYPE_ISO           (SAMHS_HST_PIPCFG_PTYPE_ISO_Val << SAMHS_HST_PIPCFG_PTYPE_Pos)  /**< (SAMHS_HST_PIPCFG) Isochronous Position  */
#define SAMHS_HST_PIPCFG_PTYPE_BLK           (SAMHS_HST_PIPCFG_PTYPE_BLK_Val << SAMHS_HST_PIPCFG_PTYPE_Pos)  /**< (SAMHS_HST_PIPCFG) Bulk Position  */
#define SAMHS_HST_PIPCFG_PTYPE_INTRPT        (SAMHS_HST_PIPCFG_PTYPE_INTRPT_Val << SAMHS_HST_PIPCFG_PTYPE_Pos)  /**< (SAMHS_HST_PIPCFG) Interrupt Position  */
#define SAMHS_HST_PIPCFG_PEPNUM_Pos          16                                             /**< (SAMHS_HST_PIPCFG) Pipe Endpoint Number Position */
#define SAMHS_HST_PIPCFG_PEPNUM              (_U_(0xF) << SAMHS_HST_PIPCFG_PEPNUM_Pos)       /**< (SAMHS_HST_PIPCFG) Pipe Endpoint Number Mask */
#define SAMHS_HST_PIPCFG_INTFRQ_Pos          24                                             /**< (SAMHS_HST_PIPCFG) Pipe Interrupt Request Frequency Position */
#define SAMHS_HST_PIPCFG_INTFRQ              (_U_(0xFF) << SAMHS_HST_PIPCFG_INTFRQ_Pos)      /**< (SAMHS_HST_PIPCFG) Pipe Interrupt Request Frequency Mask */
#define SAMHS_HST_PIPCFG_Msk                 _U_(0xFF0F377E)                                /**< (SAMHS_HST_PIPCFG) Register Mask  */

/* CTRL_BULK mode */
#define SAMHS_HST_PIPCFG_CTRL_BULK_PINGEN_Pos 20                                             /**< (SAMHS_HST_PIPCFG) Ping Enable Position */
#define SAMHS_HST_PIPCFG_CTRL_BULK_PINGEN     (_U_(0x1) << SAMHS_HST_PIPCFG_CTRL_BULK_PINGEN_Pos)  /**< (SAMHS_HST_PIPCFG) Ping Enable Mask */
#define SAMHS_HST_PIPCFG_CTRL_BULK_BINTERVAL_Pos 24                                             /**< (SAMHS_HST_PIPCFG) bInterval Parameter for the Bulk-Out/Ping Transaction Position */
#define SAMHS_HST_PIPCFG_CTRL_BULK_BINTERVAL     (_U_(0xFF) << SAMHS_HST_PIPCFG_CTRL_BULK_BINTERVAL_Pos)  /**< (SAMHS_HST_PIPCFG) bInterval Parameter for the Bulk-Out/Ping Transaction Mask */
#define SAMHS_HST_PIPCFG_CTRL_BULK_Msk       _U_(0xFF100000)                                /**< (SAMHS_HST_PIPCFG_CTRL_BULK) Register Mask  */


/* -------- SAMHS_HST_PIPISR : (USBHS Offset: 0x530) (R/ 32) Host Pipe Status Register -------- */

#define SAMHS_HST_PIPISR_OFFSET              (0x530)                                       /**<  (SAMHS_HST_PIPISR) Host Pipe Status Register  Offset */

#define SAMHS_HST_PIPISR_RXINI_Pos           0                                              /**< (SAMHS_HST_PIPISR) Received IN Data Interrupt Position */
#define SAMHS_HST_PIPISR_RXINI               (_U_(0x1) << SAMHS_HST_PIPISR_RXINI_Pos)        /**< (SAMHS_HST_PIPISR) Received IN Data Interrupt Mask */
#define SAMHS_HST_PIPISR_TXOUTI_Pos          1                                              /**< (SAMHS_HST_PIPISR) Transmitted OUT Data Interrupt Position */
#define SAMHS_HST_PIPISR_TXOUTI              (_U_(0x1) << SAMHS_HST_PIPISR_TXOUTI_Pos)       /**< (SAMHS_HST_PIPISR) Transmitted OUT Data Interrupt Mask */
#define SAMHS_HST_PIPISR_PERRI_Pos           3                                              /**< (SAMHS_HST_PIPISR) Pipe Error Interrupt Position */
#define SAMHS_HST_PIPISR_PERRI               (_U_(0x1) << SAMHS_HST_PIPISR_PERRI_Pos)        /**< (SAMHS_HST_PIPISR) Pipe Error Interrupt Mask */
#define SAMHS_HST_PIPISR_NAKEDI_Pos          4                                              /**< (SAMHS_HST_PIPISR) NAKed Interrupt Position */
#define SAMHS_HST_PIPISR_NAKEDI              (_U_(0x1) << SAMHS_HST_PIPISR_NAKEDI_Pos)       /**< (SAMHS_HST_PIPISR) NAKed Interrupt Mask */
#define SAMHS_HST_PIPISR_OVERFI_Pos          5                                              /**< (SAMHS_HST_PIPISR) Overflow Interrupt Position */
#define SAMHS_HST_PIPISR_OVERFI              (_U_(0x1) << SAMHS_HST_PIPISR_OVERFI_Pos)       /**< (SAMHS_HST_PIPISR) Overflow Interrupt Mask */
#define SAMHS_HST_PIPISR_SHORTPACKETI_Pos    7                                              /**< (SAMHS_HST_PIPISR) Short Packet Interrupt Position */
#define SAMHS_HST_PIPISR_SHORTPACKETI        (_U_(0x1) << SAMHS_HST_PIPISR_SHORTPACKETI_Pos)  /**< (SAMHS_HST_PIPISR) Short Packet Interrupt Mask */
#define SAMHS_HST_PIPISR_DTSEQ_Pos           8                                              /**< (SAMHS_HST_PIPISR) Data Toggle Sequence Position */
#define SAMHS_HST_PIPISR_DTSEQ               (_U_(0x3) << SAMHS_HST_PIPISR_DTSEQ_Pos)        /**< (SAMHS_HST_PIPISR) Data Toggle Sequence Mask */
#define   SAMHS_HST_PIPISR_DTSEQ_DATA0_Val   _U_(0x0)                                       /**< (SAMHS_HST_PIPISR) Data0 toggle sequence  */
#define   SAMHS_HST_PIPISR_DTSEQ_DATA1_Val   _U_(0x1)                                       /**< (SAMHS_HST_PIPISR) Data1 toggle sequence  */
#define SAMHS_HST_PIPISR_DTSEQ_DATA0         (SAMHS_HST_PIPISR_DTSEQ_DATA0_Val << SAMHS_HST_PIPISR_DTSEQ_Pos)  /**< (SAMHS_HST_PIPISR) Data0 toggle sequence Position  */
#define SAMHS_HST_PIPISR_DTSEQ_DATA1         (SAMHS_HST_PIPISR_DTSEQ_DATA1_Val << SAMHS_HST_PIPISR_DTSEQ_Pos)  /**< (SAMHS_HST_PIPISR) Data1 toggle sequence Position  */
#define SAMHS_HST_PIPISR_NBUSYBK_Pos         12                                             /**< (SAMHS_HST_PIPISR) Number of Busy Banks Position */
#define SAMHS_HST_PIPISR_NBUSYBK             (_U_(0x3) << SAMHS_HST_PIPISR_NBUSYBK_Pos)      /**< (SAMHS_HST_PIPISR) Number of Busy Banks Mask */
#define   SAMHS_HST_PIPISR_NBUSYBK_0_BUSY_Val _U_(0x0)                                       /**< (SAMHS_HST_PIPISR) 0 busy bank (all banks free)  */
#define   SAMHS_HST_PIPISR_NBUSYBK_1_BUSY_Val _U_(0x1)                                       /**< (SAMHS_HST_PIPISR) 1 busy bank  */
#define   SAMHS_HST_PIPISR_NBUSYBK_2_BUSY_Val _U_(0x2)                                       /**< (SAMHS_HST_PIPISR) 2 busy banks  */
#define   SAMHS_HST_PIPISR_NBUSYBK_3_BUSY_Val _U_(0x3)                                       /**< (SAMHS_HST_PIPISR) 3 busy banks  */
#define SAMHS_HST_PIPISR_NBUSYBK_0_BUSY      (SAMHS_HST_PIPISR_NBUSYBK_0_BUSY_Val << SAMHS_HST_PIPISR_NBUSYBK_Pos)  /**< (SAMHS_HST_PIPISR) 0 busy bank (all banks free) Position  */
#define SAMHS_HST_PIPISR_NBUSYBK_1_BUSY      (SAMHS_HST_PIPISR_NBUSYBK_1_BUSY_Val << SAMHS_HST_PIPISR_NBUSYBK_Pos)  /**< (SAMHS_HST_PIPISR) 1 busy bank Position  */
#define SAMHS_HST_PIPISR_NBUSYBK_2_BUSY      (SAMHS_HST_PIPISR_NBUSYBK_2_BUSY_Val << SAMHS_HST_PIPISR_NBUSYBK_Pos)  /**< (SAMHS_HST_PIPISR) 2 busy banks Position  */
#define SAMHS_HST_PIPISR_NBUSYBK_3_BUSY      (SAMHS_HST_PIPISR_NBUSYBK_3_BUSY_Val << SAMHS_HST_PIPISR_NBUSYBK_Pos)  /**< (SAMHS_HST_PIPISR) 3 busy banks Position  */
#define SAMHS_HST_PIPISR_CURRBK_Pos          14                                             /**< (SAMHS_HST_PIPISR) Current Bank Position */
#define SAMHS_HST_PIPISR_CURRBK              (_U_(0x3) << SAMHS_HST_PIPISR_CURRBK_Pos)       /**< (SAMHS_HST_PIPISR) Current Bank Mask */
#define   SAMHS_HST_PIPISR_CURRBK_BANK0_Val  _U_(0x0)                                       /**< (SAMHS_HST_PIPISR) Current bank is bank0  */
#define   SAMHS_HST_PIPISR_CURRBK_BANK1_Val  _U_(0x1)                                       /**< (SAMHS_HST_PIPISR) Current bank is bank1  */
#define   SAMHS_HST_PIPISR_CURRBK_BANK2_Val  _U_(0x2)                                       /**< (SAMHS_HST_PIPISR) Current bank is bank2  */
#define SAMHS_HST_PIPISR_CURRBK_BANK0        (SAMHS_HST_PIPISR_CURRBK_BANK0_Val << SAMHS_HST_PIPISR_CURRBK_Pos)  /**< (SAMHS_HST_PIPISR) Current bank is bank0 Position  */
#define SAMHS_HST_PIPISR_CURRBK_BANK1        (SAMHS_HST_PIPISR_CURRBK_BANK1_Val << SAMHS_HST_PIPISR_CURRBK_Pos)  /**< (SAMHS_HST_PIPISR) Current bank is bank1 Position  */
#define SAMHS_HST_PIPISR_CURRBK_BANK2        (SAMHS_HST_PIPISR_CURRBK_BANK2_Val << SAMHS_HST_PIPISR_CURRBK_Pos)  /**< (SAMHS_HST_PIPISR) Current bank is bank2 Position  */
#define SAMHS_HST_PIPISR_RWALL_Pos           16                                             /**< (SAMHS_HST_PIPISR) Read/Write Allowed Position */
#define SAMHS_HST_PIPISR_RWALL               (_U_(0x1) << SAMHS_HST_PIPISR_RWALL_Pos)        /**< (SAMHS_HST_PIPISR) Read/Write Allowed Mask */
#define SAMHS_HST_PIPISR_CFGOK_Pos           18                                             /**< (SAMHS_HST_PIPISR) Configuration OK Status Position */
#define SAMHS_HST_PIPISR_CFGOK               (_U_(0x1) << SAMHS_HST_PIPISR_CFGOK_Pos)        /**< (SAMHS_HST_PIPISR) Configuration OK Status Mask */
#define SAMHS_HST_PIPISR_PBYCT_Pos           20                                             /**< (SAMHS_HST_PIPISR) Pipe Byte Count Position */
#define SAMHS_HST_PIPISR_PBYCT               (_U_(0x7FF) << SAMHS_HST_PIPISR_PBYCT_Pos)      /**< (SAMHS_HST_PIPISR) Pipe Byte Count Mask */
#define SAMHS_HST_PIPISR_Msk                 _U_(0x7FF5F3BB)                                /**< (SAMHS_HST_PIPISR) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPISR_CTRL_TXSTPI_Pos     2                                              /**< (SAMHS_HST_PIPISR) Transmitted SETUP Interrupt Position */
#define SAMHS_HST_PIPISR_CTRL_TXSTPI         (_U_(0x1) << SAMHS_HST_PIPISR_CTRL_TXSTPI_Pos)  /**< (SAMHS_HST_PIPISR) Transmitted SETUP Interrupt Mask */
#define SAMHS_HST_PIPISR_CTRL_RXSTALLDI_Pos  6                                              /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Position */
#define SAMHS_HST_PIPISR_CTRL_RXSTALLDI      (_U_(0x1) << SAMHS_HST_PIPISR_CTRL_RXSTALLDI_Pos)  /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Mask */
#define SAMHS_HST_PIPISR_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPISR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPISR_ISO_UNDERFI_Pos     2                                              /**< (SAMHS_HST_PIPISR) Underflow Interrupt Position */
#define SAMHS_HST_PIPISR_ISO_UNDERFI         (_U_(0x1) << SAMHS_HST_PIPISR_ISO_UNDERFI_Pos)  /**< (SAMHS_HST_PIPISR) Underflow Interrupt Mask */
#define SAMHS_HST_PIPISR_ISO_CRCERRI_Pos     6                                              /**< (SAMHS_HST_PIPISR) CRC Error Interrupt Position */
#define SAMHS_HST_PIPISR_ISO_CRCERRI         (_U_(0x1) << SAMHS_HST_PIPISR_ISO_CRCERRI_Pos)  /**< (SAMHS_HST_PIPISR) CRC Error Interrupt Mask */
#define SAMHS_HST_PIPISR_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPISR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPISR_BLK_TXSTPI_Pos      2                                              /**< (SAMHS_HST_PIPISR) Transmitted SETUP Interrupt Position */
#define SAMHS_HST_PIPISR_BLK_TXSTPI          (_U_(0x1) << SAMHS_HST_PIPISR_BLK_TXSTPI_Pos)   /**< (SAMHS_HST_PIPISR) Transmitted SETUP Interrupt Mask */
#define SAMHS_HST_PIPISR_BLK_RXSTALLDI_Pos   6                                              /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Position */
#define SAMHS_HST_PIPISR_BLK_RXSTALLDI       (_U_(0x1) << SAMHS_HST_PIPISR_BLK_RXSTALLDI_Pos)  /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Mask */
#define SAMHS_HST_PIPISR_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPISR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPISR_INTRPT_UNDERFI_Pos  2                                              /**< (SAMHS_HST_PIPISR) Underflow Interrupt Position */
#define SAMHS_HST_PIPISR_INTRPT_UNDERFI      (_U_(0x1) << SAMHS_HST_PIPISR_INTRPT_UNDERFI_Pos)  /**< (SAMHS_HST_PIPISR) Underflow Interrupt Mask */
#define SAMHS_HST_PIPISR_INTRPT_RXSTALLDI_Pos 6                                              /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Position */
#define SAMHS_HST_PIPISR_INTRPT_RXSTALLDI     (_U_(0x1) << SAMHS_HST_PIPISR_INTRPT_RXSTALLDI_Pos)  /**< (SAMHS_HST_PIPISR) Received STALLed Interrupt Mask */
#define SAMHS_HST_PIPISR_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPISR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPICR : (USBHS Offset: 0x560) (/W 32) Host Pipe Clear Register -------- */

#define SAMHS_HST_PIPICR_OFFSET              (0x560)                                       /**<  (SAMHS_HST_PIPICR) Host Pipe Clear Register  Offset */

#define SAMHS_HST_PIPICR_RXINIC_Pos          0                                              /**< (SAMHS_HST_PIPICR) Received IN Data Interrupt Clear Position */
#define SAMHS_HST_PIPICR_RXINIC              (_U_(0x1) << SAMHS_HST_PIPICR_RXINIC_Pos)       /**< (SAMHS_HST_PIPICR) Received IN Data Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_TXOUTIC_Pos         1                                              /**< (SAMHS_HST_PIPICR) Transmitted OUT Data Interrupt Clear Position */
#define SAMHS_HST_PIPICR_TXOUTIC             (_U_(0x1) << SAMHS_HST_PIPICR_TXOUTIC_Pos)      /**< (SAMHS_HST_PIPICR) Transmitted OUT Data Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_NAKEDIC_Pos         4                                              /**< (SAMHS_HST_PIPICR) NAKed Interrupt Clear Position */
#define SAMHS_HST_PIPICR_NAKEDIC             (_U_(0x1) << SAMHS_HST_PIPICR_NAKEDIC_Pos)      /**< (SAMHS_HST_PIPICR) NAKed Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_OVERFIC_Pos         5                                              /**< (SAMHS_HST_PIPICR) Overflow Interrupt Clear Position */
#define SAMHS_HST_PIPICR_OVERFIC             (_U_(0x1) << SAMHS_HST_PIPICR_OVERFIC_Pos)      /**< (SAMHS_HST_PIPICR) Overflow Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_SHORTPACKETIC_Pos   7                                              /**< (SAMHS_HST_PIPICR) Short Packet Interrupt Clear Position */
#define SAMHS_HST_PIPICR_SHORTPACKETIC       (_U_(0x1) << SAMHS_HST_PIPICR_SHORTPACKETIC_Pos)  /**< (SAMHS_HST_PIPICR) Short Packet Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_Msk                 _U_(0xB3)                                      /**< (SAMHS_HST_PIPICR) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPICR_CTRL_TXSTPIC_Pos    2                                              /**< (SAMHS_HST_PIPICR) Transmitted SETUP Interrupt Clear Position */
#define SAMHS_HST_PIPICR_CTRL_TXSTPIC        (_U_(0x1) << SAMHS_HST_PIPICR_CTRL_TXSTPIC_Pos)  /**< (SAMHS_HST_PIPICR) Transmitted SETUP Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_CTRL_RXSTALLDIC_Pos 6                                              /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Position */
#define SAMHS_HST_PIPICR_CTRL_RXSTALLDIC     (_U_(0x1) << SAMHS_HST_PIPICR_CTRL_RXSTALLDIC_Pos)  /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPICR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPICR_ISO_UNDERFIC_Pos    2                                              /**< (SAMHS_HST_PIPICR) Underflow Interrupt Clear Position */
#define SAMHS_HST_PIPICR_ISO_UNDERFIC        (_U_(0x1) << SAMHS_HST_PIPICR_ISO_UNDERFIC_Pos)  /**< (SAMHS_HST_PIPICR) Underflow Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_ISO_CRCERRIC_Pos    6                                              /**< (SAMHS_HST_PIPICR) CRC Error Interrupt Clear Position */
#define SAMHS_HST_PIPICR_ISO_CRCERRIC        (_U_(0x1) << SAMHS_HST_PIPICR_ISO_CRCERRIC_Pos)  /**< (SAMHS_HST_PIPICR) CRC Error Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPICR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPICR_BLK_TXSTPIC_Pos     2                                              /**< (SAMHS_HST_PIPICR) Transmitted SETUP Interrupt Clear Position */
#define SAMHS_HST_PIPICR_BLK_TXSTPIC         (_U_(0x1) << SAMHS_HST_PIPICR_BLK_TXSTPIC_Pos)  /**< (SAMHS_HST_PIPICR) Transmitted SETUP Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_BLK_RXSTALLDIC_Pos  6                                              /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Position */
#define SAMHS_HST_PIPICR_BLK_RXSTALLDIC      (_U_(0x1) << SAMHS_HST_PIPICR_BLK_RXSTALLDIC_Pos)  /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPICR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPICR_INTRPT_UNDERFIC_Pos 2                                              /**< (SAMHS_HST_PIPICR) Underflow Interrupt Clear Position */
#define SAMHS_HST_PIPICR_INTRPT_UNDERFIC     (_U_(0x1) << SAMHS_HST_PIPICR_INTRPT_UNDERFIC_Pos)  /**< (SAMHS_HST_PIPICR) Underflow Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_INTRPT_RXSTALLDIC_Pos 6                                              /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Position */
#define SAMHS_HST_PIPICR_INTRPT_RXSTALLDIC     (_U_(0x1) << SAMHS_HST_PIPICR_INTRPT_RXSTALLDIC_Pos)  /**< (SAMHS_HST_PIPICR) Received STALLed Interrupt Clear Mask */
#define SAMHS_HST_PIPICR_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPICR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPIFR : (USBHS Offset: 0x590) (/W 32) Host Pipe Set Register -------- */

#define SAMHS_HST_PIPIFR_OFFSET              (0x590)                                       /**<  (SAMHS_HST_PIPIFR) Host Pipe Set Register  Offset */

#define SAMHS_HST_PIPIFR_RXINIS_Pos          0                                              /**< (SAMHS_HST_PIPIFR) Received IN Data Interrupt Set Position */
#define SAMHS_HST_PIPIFR_RXINIS              (_U_(0x1) << SAMHS_HST_PIPIFR_RXINIS_Pos)       /**< (SAMHS_HST_PIPIFR) Received IN Data Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_TXOUTIS_Pos         1                                              /**< (SAMHS_HST_PIPIFR) Transmitted OUT Data Interrupt Set Position */
#define SAMHS_HST_PIPIFR_TXOUTIS             (_U_(0x1) << SAMHS_HST_PIPIFR_TXOUTIS_Pos)      /**< (SAMHS_HST_PIPIFR) Transmitted OUT Data Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_PERRIS_Pos          3                                              /**< (SAMHS_HST_PIPIFR) Pipe Error Interrupt Set Position */
#define SAMHS_HST_PIPIFR_PERRIS              (_U_(0x1) << SAMHS_HST_PIPIFR_PERRIS_Pos)       /**< (SAMHS_HST_PIPIFR) Pipe Error Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_NAKEDIS_Pos         4                                              /**< (SAMHS_HST_PIPIFR) NAKed Interrupt Set Position */
#define SAMHS_HST_PIPIFR_NAKEDIS             (_U_(0x1) << SAMHS_HST_PIPIFR_NAKEDIS_Pos)      /**< (SAMHS_HST_PIPIFR) NAKed Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_OVERFIS_Pos         5                                              /**< (SAMHS_HST_PIPIFR) Overflow Interrupt Set Position */
#define SAMHS_HST_PIPIFR_OVERFIS             (_U_(0x1) << SAMHS_HST_PIPIFR_OVERFIS_Pos)      /**< (SAMHS_HST_PIPIFR) Overflow Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_SHORTPACKETIS_Pos   7                                              /**< (SAMHS_HST_PIPIFR) Short Packet Interrupt Set Position */
#define SAMHS_HST_PIPIFR_SHORTPACKETIS       (_U_(0x1) << SAMHS_HST_PIPIFR_SHORTPACKETIS_Pos)  /**< (SAMHS_HST_PIPIFR) Short Packet Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_NBUSYBKS_Pos        12                                             /**< (SAMHS_HST_PIPIFR) Number of Busy Banks Set Position */
#define SAMHS_HST_PIPIFR_NBUSYBKS            (_U_(0x1) << SAMHS_HST_PIPIFR_NBUSYBKS_Pos)     /**< (SAMHS_HST_PIPIFR) Number of Busy Banks Set Mask */
#define SAMHS_HST_PIPIFR_Msk                 _U_(0x10BB)                                    /**< (SAMHS_HST_PIPIFR) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPIFR_CTRL_TXSTPIS_Pos    2                                              /**< (SAMHS_HST_PIPIFR) Transmitted SETUP Interrupt Set Position */
#define SAMHS_HST_PIPIFR_CTRL_TXSTPIS        (_U_(0x1) << SAMHS_HST_PIPIFR_CTRL_TXSTPIS_Pos)  /**< (SAMHS_HST_PIPIFR) Transmitted SETUP Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_CTRL_RXSTALLDIS_Pos 6                                              /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Position */
#define SAMHS_HST_PIPIFR_CTRL_RXSTALLDIS     (_U_(0x1) << SAMHS_HST_PIPIFR_CTRL_RXSTALLDIS_Pos)  /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPIFR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPIFR_ISO_UNDERFIS_Pos    2                                              /**< (SAMHS_HST_PIPIFR) Underflow Interrupt Set Position */
#define SAMHS_HST_PIPIFR_ISO_UNDERFIS        (_U_(0x1) << SAMHS_HST_PIPIFR_ISO_UNDERFIS_Pos)  /**< (SAMHS_HST_PIPIFR) Underflow Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_ISO_CRCERRIS_Pos    6                                              /**< (SAMHS_HST_PIPIFR) CRC Error Interrupt Set Position */
#define SAMHS_HST_PIPIFR_ISO_CRCERRIS        (_U_(0x1) << SAMHS_HST_PIPIFR_ISO_CRCERRIS_Pos)  /**< (SAMHS_HST_PIPIFR) CRC Error Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIFR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPIFR_BLK_TXSTPIS_Pos     2                                              /**< (SAMHS_HST_PIPIFR) Transmitted SETUP Interrupt Set Position */
#define SAMHS_HST_PIPIFR_BLK_TXSTPIS         (_U_(0x1) << SAMHS_HST_PIPIFR_BLK_TXSTPIS_Pos)  /**< (SAMHS_HST_PIPIFR) Transmitted SETUP Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_BLK_RXSTALLDIS_Pos  6                                              /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Position */
#define SAMHS_HST_PIPIFR_BLK_RXSTALLDIS      (_U_(0x1) << SAMHS_HST_PIPIFR_BLK_RXSTALLDIS_Pos)  /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIFR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPIFR_INTRPT_UNDERFIS_Pos 2                                              /**< (SAMHS_HST_PIPIFR) Underflow Interrupt Set Position */
#define SAMHS_HST_PIPIFR_INTRPT_UNDERFIS     (_U_(0x1) << SAMHS_HST_PIPIFR_INTRPT_UNDERFIS_Pos)  /**< (SAMHS_HST_PIPIFR) Underflow Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_INTRPT_RXSTALLDIS_Pos 6                                              /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Position */
#define SAMHS_HST_PIPIFR_INTRPT_RXSTALLDIS     (_U_(0x1) << SAMHS_HST_PIPIFR_INTRPT_RXSTALLDIS_Pos)  /**< (SAMHS_HST_PIPIFR) Received STALLed Interrupt Set Mask */
#define SAMHS_HST_PIPIFR_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPIFR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPIMR : (USBHS Offset: 0x5c0) (R/ 32) Host Pipe Mask Register -------- */

#define SAMHS_HST_PIPIMR_OFFSET              (0x5C0)                                       /**<  (SAMHS_HST_PIPIMR) Host Pipe Mask Register  Offset */

#define SAMHS_HST_PIPIMR_RXINE_Pos           0                                              /**< (SAMHS_HST_PIPIMR) Received IN Data Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_RXINE               (_U_(0x1) << SAMHS_HST_PIPIMR_RXINE_Pos)        /**< (SAMHS_HST_PIPIMR) Received IN Data Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_TXOUTE_Pos          1                                              /**< (SAMHS_HST_PIPIMR) Transmitted OUT Data Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_TXOUTE              (_U_(0x1) << SAMHS_HST_PIPIMR_TXOUTE_Pos)       /**< (SAMHS_HST_PIPIMR) Transmitted OUT Data Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_PERRE_Pos           3                                              /**< (SAMHS_HST_PIPIMR) Pipe Error Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_PERRE               (_U_(0x1) << SAMHS_HST_PIPIMR_PERRE_Pos)        /**< (SAMHS_HST_PIPIMR) Pipe Error Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_NAKEDE_Pos          4                                              /**< (SAMHS_HST_PIPIMR) NAKed Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_NAKEDE              (_U_(0x1) << SAMHS_HST_PIPIMR_NAKEDE_Pos)       /**< (SAMHS_HST_PIPIMR) NAKed Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_OVERFIE_Pos         5                                              /**< (SAMHS_HST_PIPIMR) Overflow Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_OVERFIE             (_U_(0x1) << SAMHS_HST_PIPIMR_OVERFIE_Pos)      /**< (SAMHS_HST_PIPIMR) Overflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_SHORTPACKETIE_Pos   7                                              /**< (SAMHS_HST_PIPIMR) Short Packet Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_SHORTPACKETIE       (_U_(0x1) << SAMHS_HST_PIPIMR_SHORTPACKETIE_Pos)  /**< (SAMHS_HST_PIPIMR) Short Packet Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_NBUSYBKE_Pos        12                                             /**< (SAMHS_HST_PIPIMR) Number of Busy Banks Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_NBUSYBKE            (_U_(0x1) << SAMHS_HST_PIPIMR_NBUSYBKE_Pos)     /**< (SAMHS_HST_PIPIMR) Number of Busy Banks Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_FIFOCON_Pos         14                                             /**< (SAMHS_HST_PIPIMR) FIFO Control Position */
#define SAMHS_HST_PIPIMR_FIFOCON             (_U_(0x1) << SAMHS_HST_PIPIMR_FIFOCON_Pos)      /**< (SAMHS_HST_PIPIMR) FIFO Control Mask */
#define SAMHS_HST_PIPIMR_PDISHDMA_Pos        16                                             /**< (SAMHS_HST_PIPIMR) Pipe Interrupts Disable HDMA Request Enable Position */
#define SAMHS_HST_PIPIMR_PDISHDMA            (_U_(0x1) << SAMHS_HST_PIPIMR_PDISHDMA_Pos)     /**< (SAMHS_HST_PIPIMR) Pipe Interrupts Disable HDMA Request Enable Mask */
#define SAMHS_HST_PIPIMR_PFREEZE_Pos         17                                             /**< (SAMHS_HST_PIPIMR) Pipe Freeze Position */
#define SAMHS_HST_PIPIMR_PFREEZE             (_U_(0x1) << SAMHS_HST_PIPIMR_PFREEZE_Pos)      /**< (SAMHS_HST_PIPIMR) Pipe Freeze Mask */
#define SAMHS_HST_PIPIMR_RSTDT_Pos           18                                             /**< (SAMHS_HST_PIPIMR) Reset Data Toggle Position */
#define SAMHS_HST_PIPIMR_RSTDT               (_U_(0x1) << SAMHS_HST_PIPIMR_RSTDT_Pos)        /**< (SAMHS_HST_PIPIMR) Reset Data Toggle Mask */
#define SAMHS_HST_PIPIMR_Msk                 _U_(0x750BB)                                   /**< (SAMHS_HST_PIPIMR) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPIMR_CTRL_TXSTPE_Pos     2                                              /**< (SAMHS_HST_PIPIMR) Transmitted SETUP Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_CTRL_TXSTPE         (_U_(0x1) << SAMHS_HST_PIPIMR_CTRL_TXSTPE_Pos)  /**< (SAMHS_HST_PIPIMR) Transmitted SETUP Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_CTRL_RXSTALLDE_Pos  6                                              /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_CTRL_RXSTALLDE      (_U_(0x1) << SAMHS_HST_PIPIMR_CTRL_RXSTALLDE_Pos)  /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPIMR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPIMR_ISO_UNDERFIE_Pos    2                                              /**< (SAMHS_HST_PIPIMR) Underflow Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_ISO_UNDERFIE        (_U_(0x1) << SAMHS_HST_PIPIMR_ISO_UNDERFIE_Pos)  /**< (SAMHS_HST_PIPIMR) Underflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_ISO_CRCERRE_Pos     6                                              /**< (SAMHS_HST_PIPIMR) CRC Error Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_ISO_CRCERRE         (_U_(0x1) << SAMHS_HST_PIPIMR_ISO_CRCERRE_Pos)  /**< (SAMHS_HST_PIPIMR) CRC Error Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIMR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPIMR_BLK_TXSTPE_Pos      2                                              /**< (SAMHS_HST_PIPIMR) Transmitted SETUP Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_BLK_TXSTPE          (_U_(0x1) << SAMHS_HST_PIPIMR_BLK_TXSTPE_Pos)   /**< (SAMHS_HST_PIPIMR) Transmitted SETUP Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_BLK_RXSTALLDE_Pos   6                                              /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_BLK_RXSTALLDE       (_U_(0x1) << SAMHS_HST_PIPIMR_BLK_RXSTALLDE_Pos)  /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIMR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPIMR_INTRPT_UNDERFIE_Pos 2                                              /**< (SAMHS_HST_PIPIMR) Underflow Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_INTRPT_UNDERFIE     (_U_(0x1) << SAMHS_HST_PIPIMR_INTRPT_UNDERFIE_Pos)  /**< (SAMHS_HST_PIPIMR) Underflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_INTRPT_RXSTALLDE_Pos 6                                              /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIMR_INTRPT_RXSTALLDE     (_U_(0x1) << SAMHS_HST_PIPIMR_INTRPT_RXSTALLDE_Pos)  /**< (SAMHS_HST_PIPIMR) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIMR_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPIMR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPIER : (USBHS Offset: 0x5f0) (/W 32) Host Pipe Enable Register -------- */

#define SAMHS_HST_PIPIER_OFFSET              (0x5F0)                                       /**<  (SAMHS_HST_PIPIER) Host Pipe Enable Register  Offset */

#define SAMHS_HST_PIPIER_RXINES_Pos          0                                              /**< (SAMHS_HST_PIPIER) Received IN Data Interrupt Enable Position */
#define SAMHS_HST_PIPIER_RXINES              (_U_(0x1) << SAMHS_HST_PIPIER_RXINES_Pos)       /**< (SAMHS_HST_PIPIER) Received IN Data Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_TXOUTES_Pos         1                                              /**< (SAMHS_HST_PIPIER) Transmitted OUT Data Interrupt Enable Position */
#define SAMHS_HST_PIPIER_TXOUTES             (_U_(0x1) << SAMHS_HST_PIPIER_TXOUTES_Pos)      /**< (SAMHS_HST_PIPIER) Transmitted OUT Data Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_PERRES_Pos          3                                              /**< (SAMHS_HST_PIPIER) Pipe Error Interrupt Enable Position */
#define SAMHS_HST_PIPIER_PERRES              (_U_(0x1) << SAMHS_HST_PIPIER_PERRES_Pos)       /**< (SAMHS_HST_PIPIER) Pipe Error Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_NAKEDES_Pos         4                                              /**< (SAMHS_HST_PIPIER) NAKed Interrupt Enable Position */
#define SAMHS_HST_PIPIER_NAKEDES             (_U_(0x1) << SAMHS_HST_PIPIER_NAKEDES_Pos)      /**< (SAMHS_HST_PIPIER) NAKed Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_OVERFIES_Pos        5                                              /**< (SAMHS_HST_PIPIER) Overflow Interrupt Enable Position */
#define SAMHS_HST_PIPIER_OVERFIES            (_U_(0x1) << SAMHS_HST_PIPIER_OVERFIES_Pos)     /**< (SAMHS_HST_PIPIER) Overflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_SHORTPACKETIES_Pos  7                                              /**< (SAMHS_HST_PIPIER) Short Packet Interrupt Enable Position */
#define SAMHS_HST_PIPIER_SHORTPACKETIES      (_U_(0x1) << SAMHS_HST_PIPIER_SHORTPACKETIES_Pos)  /**< (SAMHS_HST_PIPIER) Short Packet Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_NBUSYBKES_Pos       12                                             /**< (SAMHS_HST_PIPIER) Number of Busy Banks Enable Position */
#define SAMHS_HST_PIPIER_NBUSYBKES           (_U_(0x1) << SAMHS_HST_PIPIER_NBUSYBKES_Pos)    /**< (SAMHS_HST_PIPIER) Number of Busy Banks Enable Mask */
#define SAMHS_HST_PIPIER_PDISHDMAS_Pos       16                                             /**< (SAMHS_HST_PIPIER) Pipe Interrupts Disable HDMA Request Enable Position */
#define SAMHS_HST_PIPIER_PDISHDMAS           (_U_(0x1) << SAMHS_HST_PIPIER_PDISHDMAS_Pos)    /**< (SAMHS_HST_PIPIER) Pipe Interrupts Disable HDMA Request Enable Mask */
#define SAMHS_HST_PIPIER_PFREEZES_Pos        17                                             /**< (SAMHS_HST_PIPIER) Pipe Freeze Enable Position */
#define SAMHS_HST_PIPIER_PFREEZES            (_U_(0x1) << SAMHS_HST_PIPIER_PFREEZES_Pos)     /**< (SAMHS_HST_PIPIER) Pipe Freeze Enable Mask */
#define SAMHS_HST_PIPIER_RSTDTS_Pos          18                                             /**< (SAMHS_HST_PIPIER) Reset Data Toggle Enable Position */
#define SAMHS_HST_PIPIER_RSTDTS              (_U_(0x1) << SAMHS_HST_PIPIER_RSTDTS_Pos)       /**< (SAMHS_HST_PIPIER) Reset Data Toggle Enable Mask */
#define SAMHS_HST_PIPIER_Msk                 _U_(0x710BB)                                   /**< (SAMHS_HST_PIPIER) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPIER_CTRL_TXSTPES_Pos    2                                              /**< (SAMHS_HST_PIPIER) Transmitted SETUP Interrupt Enable Position */
#define SAMHS_HST_PIPIER_CTRL_TXSTPES        (_U_(0x1) << SAMHS_HST_PIPIER_CTRL_TXSTPES_Pos)  /**< (SAMHS_HST_PIPIER) Transmitted SETUP Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_CTRL_RXSTALLDES_Pos 6                                              /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIER_CTRL_RXSTALLDES     (_U_(0x1) << SAMHS_HST_PIPIER_CTRL_RXSTALLDES_Pos)  /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPIER_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPIER_ISO_UNDERFIES_Pos   2                                              /**< (SAMHS_HST_PIPIER) Underflow Interrupt Enable Position */
#define SAMHS_HST_PIPIER_ISO_UNDERFIES       (_U_(0x1) << SAMHS_HST_PIPIER_ISO_UNDERFIES_Pos)  /**< (SAMHS_HST_PIPIER) Underflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_ISO_CRCERRES_Pos    6                                              /**< (SAMHS_HST_PIPIER) CRC Error Interrupt Enable Position */
#define SAMHS_HST_PIPIER_ISO_CRCERRES        (_U_(0x1) << SAMHS_HST_PIPIER_ISO_CRCERRES_Pos)  /**< (SAMHS_HST_PIPIER) CRC Error Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIER_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPIER_BLK_TXSTPES_Pos     2                                              /**< (SAMHS_HST_PIPIER) Transmitted SETUP Interrupt Enable Position */
#define SAMHS_HST_PIPIER_BLK_TXSTPES         (_U_(0x1) << SAMHS_HST_PIPIER_BLK_TXSTPES_Pos)  /**< (SAMHS_HST_PIPIER) Transmitted SETUP Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_BLK_RXSTALLDES_Pos  6                                              /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIER_BLK_RXSTALLDES      (_U_(0x1) << SAMHS_HST_PIPIER_BLK_RXSTALLDES_Pos)  /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIER_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPIER_INTRPT_UNDERFIES_Pos 2                                              /**< (SAMHS_HST_PIPIER) Underflow Interrupt Enable Position */
#define SAMHS_HST_PIPIER_INTRPT_UNDERFIES     (_U_(0x1) << SAMHS_HST_PIPIER_INTRPT_UNDERFIES_Pos)  /**< (SAMHS_HST_PIPIER) Underflow Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_INTRPT_RXSTALLDES_Pos 6                                              /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Position */
#define SAMHS_HST_PIPIER_INTRPT_RXSTALLDES     (_U_(0x1) << SAMHS_HST_PIPIER_INTRPT_RXSTALLDES_Pos)  /**< (SAMHS_HST_PIPIER) Received STALLed Interrupt Enable Mask */
#define SAMHS_HST_PIPIER_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPIER_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPIDR : (USBHS Offset: 0x620) (/W 32) Host Pipe Disable Register -------- */

#define SAMHS_HST_PIPIDR_OFFSET              (0x620)                                       /**<  (SAMHS_HST_PIPIDR) Host Pipe Disable Register  Offset */

#define SAMHS_HST_PIPIDR_RXINEC_Pos          0                                              /**< (SAMHS_HST_PIPIDR) Received IN Data Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_RXINEC              (_U_(0x1) << SAMHS_HST_PIPIDR_RXINEC_Pos)       /**< (SAMHS_HST_PIPIDR) Received IN Data Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_TXOUTEC_Pos         1                                              /**< (SAMHS_HST_PIPIDR) Transmitted OUT Data Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_TXOUTEC             (_U_(0x1) << SAMHS_HST_PIPIDR_TXOUTEC_Pos)      /**< (SAMHS_HST_PIPIDR) Transmitted OUT Data Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_PERREC_Pos          3                                              /**< (SAMHS_HST_PIPIDR) Pipe Error Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_PERREC              (_U_(0x1) << SAMHS_HST_PIPIDR_PERREC_Pos)       /**< (SAMHS_HST_PIPIDR) Pipe Error Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_NAKEDEC_Pos         4                                              /**< (SAMHS_HST_PIPIDR) NAKed Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_NAKEDEC             (_U_(0x1) << SAMHS_HST_PIPIDR_NAKEDEC_Pos)      /**< (SAMHS_HST_PIPIDR) NAKed Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_OVERFIEC_Pos        5                                              /**< (SAMHS_HST_PIPIDR) Overflow Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_OVERFIEC            (_U_(0x1) << SAMHS_HST_PIPIDR_OVERFIEC_Pos)     /**< (SAMHS_HST_PIPIDR) Overflow Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_SHORTPACKETIEC_Pos  7                                              /**< (SAMHS_HST_PIPIDR) Short Packet Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_SHORTPACKETIEC      (_U_(0x1) << SAMHS_HST_PIPIDR_SHORTPACKETIEC_Pos)  /**< (SAMHS_HST_PIPIDR) Short Packet Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_NBUSYBKEC_Pos       12                                             /**< (SAMHS_HST_PIPIDR) Number of Busy Banks Disable Position */
#define SAMHS_HST_PIPIDR_NBUSYBKEC           (_U_(0x1) << SAMHS_HST_PIPIDR_NBUSYBKEC_Pos)    /**< (SAMHS_HST_PIPIDR) Number of Busy Banks Disable Mask */
#define SAMHS_HST_PIPIDR_FIFOCONC_Pos        14                                             /**< (SAMHS_HST_PIPIDR) FIFO Control Disable Position */
#define SAMHS_HST_PIPIDR_FIFOCONC            (_U_(0x1) << SAMHS_HST_PIPIDR_FIFOCONC_Pos)     /**< (SAMHS_HST_PIPIDR) FIFO Control Disable Mask */
#define SAMHS_HST_PIPIDR_PDISHDMAC_Pos       16                                             /**< (SAMHS_HST_PIPIDR) Pipe Interrupts Disable HDMA Request Disable Position */
#define SAMHS_HST_PIPIDR_PDISHDMAC           (_U_(0x1) << SAMHS_HST_PIPIDR_PDISHDMAC_Pos)    /**< (SAMHS_HST_PIPIDR) Pipe Interrupts Disable HDMA Request Disable Mask */
#define SAMHS_HST_PIPIDR_PFREEZEC_Pos        17                                             /**< (SAMHS_HST_PIPIDR) Pipe Freeze Disable Position */
#define SAMHS_HST_PIPIDR_PFREEZEC            (_U_(0x1) << SAMHS_HST_PIPIDR_PFREEZEC_Pos)     /**< (SAMHS_HST_PIPIDR) Pipe Freeze Disable Mask */
#define SAMHS_HST_PIPIDR_Msk                 _U_(0x350BB)                                   /**< (SAMHS_HST_PIPIDR) Register Mask  */

/* CTRL mode */
#define SAMHS_HST_PIPIDR_CTRL_TXSTPEC_Pos    2                                              /**< (SAMHS_HST_PIPIDR) Transmitted SETUP Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_CTRL_TXSTPEC        (_U_(0x1) << SAMHS_HST_PIPIDR_CTRL_TXSTPEC_Pos)  /**< (SAMHS_HST_PIPIDR) Transmitted SETUP Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_CTRL_RXSTALLDEC_Pos 6                                              /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_CTRL_RXSTALLDEC     (_U_(0x1) << SAMHS_HST_PIPIDR_CTRL_RXSTALLDEC_Pos)  /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_CTRL_Msk            _U_(0x44)                                      /**< (SAMHS_HST_PIPIDR_CTRL) Register Mask  */

/* ISO mode */
#define SAMHS_HST_PIPIDR_ISO_UNDERFIEC_Pos   2                                              /**< (SAMHS_HST_PIPIDR) Underflow Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_ISO_UNDERFIEC       (_U_(0x1) << SAMHS_HST_PIPIDR_ISO_UNDERFIEC_Pos)  /**< (SAMHS_HST_PIPIDR) Underflow Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_ISO_CRCERREC_Pos    6                                              /**< (SAMHS_HST_PIPIDR) CRC Error Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_ISO_CRCERREC        (_U_(0x1) << SAMHS_HST_PIPIDR_ISO_CRCERREC_Pos)  /**< (SAMHS_HST_PIPIDR) CRC Error Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_ISO_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIDR_ISO) Register Mask  */

/* BLK mode */
#define SAMHS_HST_PIPIDR_BLK_TXSTPEC_Pos     2                                              /**< (SAMHS_HST_PIPIDR) Transmitted SETUP Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_BLK_TXSTPEC         (_U_(0x1) << SAMHS_HST_PIPIDR_BLK_TXSTPEC_Pos)  /**< (SAMHS_HST_PIPIDR) Transmitted SETUP Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_BLK_RXSTALLDEC_Pos  6                                              /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_BLK_RXSTALLDEC      (_U_(0x1) << SAMHS_HST_PIPIDR_BLK_RXSTALLDEC_Pos)  /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_BLK_Msk             _U_(0x44)                                      /**< (SAMHS_HST_PIPIDR_BLK) Register Mask  */

/* INTRPT mode */
#define SAMHS_HST_PIPIDR_INTRPT_UNDERFIEC_Pos 2                                              /**< (SAMHS_HST_PIPIDR) Underflow Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_INTRPT_UNDERFIEC     (_U_(0x1) << SAMHS_HST_PIPIDR_INTRPT_UNDERFIEC_Pos)  /**< (SAMHS_HST_PIPIDR) Underflow Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_INTRPT_RXSTALLDEC_Pos 6                                              /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Position */
#define SAMHS_HST_PIPIDR_INTRPT_RXSTALLDEC     (_U_(0x1) << SAMHS_HST_PIPIDR_INTRPT_RXSTALLDEC_Pos)  /**< (SAMHS_HST_PIPIDR) Received STALLed Interrupt Disable Mask */
#define SAMHS_HST_PIPIDR_INTRPT_Msk          _U_(0x44)                                      /**< (SAMHS_HST_PIPIDR_INTRPT) Register Mask  */


/* -------- SAMHS_HST_PIPINRQ : (USBHS Offset: 0x650) (R/W 32) Host Pipe IN Request Register -------- */

#define SAMHS_HST_PIPINRQ_OFFSET             (0x650)                                       /**<  (SAMHS_HST_PIPINRQ) Host Pipe IN Request Register  Offset */

#define SAMHS_HST_PIPINRQ_INRQ_Pos           0                                              /**< (SAMHS_HST_PIPINRQ) IN Request Number before Freeze Position */
#define SAMHS_HST_PIPINRQ_INRQ               (_U_(0xFF) << SAMHS_HST_PIPINRQ_INRQ_Pos)       /**< (SAMHS_HST_PIPINRQ) IN Request Number before Freeze Mask */
#define SAMHS_HST_PIPINRQ_INMODE_Pos         8                                              /**< (SAMHS_HST_PIPINRQ) IN Request Mode Position */
#define SAMHS_HST_PIPINRQ_INMODE             (_U_(0x1) << SAMHS_HST_PIPINRQ_INMODE_Pos)      /**< (SAMHS_HST_PIPINRQ) IN Request Mode Mask */
#define SAMHS_HST_PIPINRQ_Msk                _U_(0x1FF)                                     /**< (SAMHS_HST_PIPINRQ) Register Mask  */


/* -------- SAMHS_HST_PIPERR : (USBHS Offset: 0x680) (R/W 32) Host Pipe Error Register -------- */

#define SAMHS_HST_PIPERR_OFFSET              (0x680)                                       /**<  (SAMHS_HST_PIPERR) Host Pipe Error Register  Offset */

#define SAMHS_HST_PIPERR_DATATGL_Pos         0                                              /**< (SAMHS_HST_PIPERR) Data Toggle Error Position */
#define SAMHS_HST_PIPERR_DATATGL             (_U_(0x1) << SAMHS_HST_PIPERR_DATATGL_Pos)      /**< (SAMHS_HST_PIPERR) Data Toggle Error Mask */
#define SAMHS_HST_PIPERR_DATAPID_Pos         1                                              /**< (SAMHS_HST_PIPERR) Data PID Error Position */
#define SAMHS_HST_PIPERR_DATAPID             (_U_(0x1) << SAMHS_HST_PIPERR_DATAPID_Pos)      /**< (SAMHS_HST_PIPERR) Data PID Error Mask */
#define SAMHS_HST_PIPERR_PID_Pos             2                                              /**< (SAMHS_HST_PIPERR) Data PID Error Position */
#define SAMHS_HST_PIPERR_PID                 (_U_(0x1) << SAMHS_HST_PIPERR_PID_Pos)          /**< (SAMHS_HST_PIPERR) Data PID Error Mask */
#define SAMHS_HST_PIPERR_TIMEOUT_Pos         3                                              /**< (SAMHS_HST_PIPERR) Time-Out Error Position */
#define SAMHS_HST_PIPERR_TIMEOUT             (_U_(0x1) << SAMHS_HST_PIPERR_TIMEOUT_Pos)      /**< (SAMHS_HST_PIPERR) Time-Out Error Mask */
#define SAMHS_HST_PIPERR_CRC16_Pos           4                                              /**< (SAMHS_HST_PIPERR) CRC16 Error Position */
#define SAMHS_HST_PIPERR_CRC16               (_U_(0x1) << SAMHS_HST_PIPERR_CRC16_Pos)        /**< (SAMHS_HST_PIPERR) CRC16 Error Mask */
#define SAMHS_HST_PIPERR_COUNTER_Pos         5                                              /**< (SAMHS_HST_PIPERR) Error Counter Position */
#define SAMHS_HST_PIPERR_COUNTER             (_U_(0x3) << SAMHS_HST_PIPERR_COUNTER_Pos)      /**< (SAMHS_HST_PIPERR) Error Counter Mask */
#define SAMHS_HST_PIPERR_Msk                 _U_(0x7F)                                      /**< (SAMHS_HST_PIPERR) Register Mask  */

#define SAMHS_HST_PIPERR_CRC_Pos             4                                              /**< (SAMHS_HST_PIPERR Position) CRCx6 Error */
#define SAMHS_HST_PIPERR_CRC                 (_U_(0x1) << SAMHS_HST_PIPERR_CRC_Pos)          /**< (SAMHS_HST_PIPERR Mask) CRC */

/* -------- CTRL : (USBHS Offset: 0x800) (R/W 32) General Control Register -------- */

#define CTRL_OFFSET                   (0x800)                                       /**<  (CTRL) General Control Register  Offset */

#define CTRL_RDERRE_Pos               4                                              /**< (CTRL) Remote Device Connection Error Interrupt Enable Position */
#define CTRL_RDERRE                   (_U_(0x1) << CTRL_RDERRE_Pos)            /**< (CTRL) Remote Device Connection Error Interrupt Enable Mask */
#define CTRL_VBUSHWC_Pos              8                                              /**< (CTRL) VBUS Hardware Control Position */
#define CTRL_VBUSHWC                  (_U_(0x1) << CTRL_VBUSHWC_Pos)           /**< (CTRL) VBUS Hardware Control Mask */
#define CTRL_FRZCLK_Pos               14                                             /**< (CTRL) Freeze USB Clock Position */
#define CTRL_FRZCLK                   (_U_(0x1) << CTRL_FRZCLK_Pos)            /**< (CTRL) Freeze USB Clock Mask */
#define CTRL_USBE_Pos                 15                                             /**< (CTRL) USBHS Enable Position */
#define CTRL_USBE                     (_U_(0x1) << CTRL_USBE_Pos)              /**< (CTRL) USBHS Enable Mask */
#define CTRL_UID_Pos                  24                                             /**< (CTRL) UID Pin Enable Position */
#define CTRL_UID                      (_U_(0x1) << CTRL_UID_Pos)               /**< (CTRL) UID Pin Enable Mask */
#define CTRL_UIMOD_Pos                25                                             /**< (CTRL) USBHS Mode Position */
#define CTRL_UIMOD                    (_U_(0x1) << CTRL_UIMOD_Pos)             /**< (CTRL) USBHS Mode Mask */
#define   CTRL_UIMOD_HOST_Val         _U_(0x0)                                       /**< (CTRL) The module is in USB Host mode.  */
#define   CTRL_UIMOD_SAMHS_DEV_ICE_Val       _U_(0x1)                                       /**< (CTRL) The module is in USB Device mode.  */
#define CTRL_UIMOD_HOST               (CTRL_UIMOD_HOST_Val << CTRL_UIMOD_Pos)  /**< (CTRL) The module is in USB Host mode. Position  */
#define CTRL_UIMOD_SAMHS_DEV_ICE             (CTRL_UIMOD_SAMHS_DEV_ICE_Val << CTRL_UIMOD_Pos)  /**< (CTRL) The module is in USB Device mode. Position  */
#define CTRL_Msk                      _U_(0x300C110)                                 /**< (CTRL) Register Mask  */


/* -------- SR : (USBHS Offset: 0x804) (R/ 32) General Status Register -------- */

#define SR_OFFSET                     (0x804)                                       /**<  (SR) General Status Register  Offset */

#define SR_RDERRI_Pos                 4                                              /**< (SR) Remote Device Connection Error Interrupt (Host mode only) Position */
#define SR_RDERRI                     (_U_(0x1) << SR_RDERRI_Pos)              /**< (SR) Remote Device Connection Error Interrupt (Host mode only) Mask */
#define SR_SPEED_Pos                  12                                             /**< (SR) Speed Status (Device mode only) Position */
#define SR_SPEED                      (_U_(0x3) << SR_SPEED_Pos)               /**< (SR) Speed Status (Device mode only) Mask */
#define   SR_SPEED_FULL_SPEED_Val     _U_(0x0)                                       /**< (SR) Full-Speed mode  */
#define   SR_SPEED_HIGH_SPEED_Val     _U_(0x1)                                       /**< (SR) High-Speed mode  */
#define   SR_SPEED_LOW_SPEED_Val      _U_(0x2)                                       /**< (SR) Low-Speed mode  */
#define SR_SPEED_FULL_SPEED           (SR_SPEED_FULL_SPEED_Val << SR_SPEED_Pos)  /**< (SR) Full-Speed mode Position  */
#define SR_SPEED_HIGH_SPEED           (SR_SPEED_HIGH_SPEED_Val << SR_SPEED_Pos)  /**< (SR) High-Speed mode Position  */
#define SR_SPEED_LOW_SPEED            (SR_SPEED_LOW_SPEED_Val << SR_SPEED_Pos)  /**< (SR) Low-Speed mode Position  */
#define SR_CLKUSABLE_Pos              14                                             /**< (SR) UTMI Clock Usable Position */
#define SR_CLKUSABLE                  (_U_(0x1) << SR_CLKUSABLE_Pos)           /**< (SR) UTMI Clock Usable Mask */
#define SR_Msk                        _U_(0x7010)                                    /**< (SR) Register Mask  */


/* -------- SCR : (USBHS Offset: 0x808) (/W 32) General Status Clear Register -------- */

#define SCR_OFFSET                    (0x808)                                       /**<  (SCR) General Status Clear Register  Offset */

#define SCR_RDERRIC_Pos               4                                              /**< (SCR) Remote Device Connection Error Interrupt Clear Position */
#define SCR_RDERRIC                   (_U_(0x1) << SCR_RDERRIC_Pos)            /**< (SCR) Remote Device Connection Error Interrupt Clear Mask */
#define SCR_Msk                       _U_(0x10)                                      /**< (SCR) Register Mask  */


/* -------- SFR : (USBHS Offset: 0x80c) (/W 32) General Status Set Register -------- */

#define SFR_OFFSET                    (0x80C)                                       /**<  (SFR) General Status Set Register  Offset */

#define SFR_RDERRIS_Pos               4                                              /**< (SFR) Remote Device Connection Error Interrupt Set Position */
#define SFR_RDERRIS                   (_U_(0x1) << SFR_RDERRIS_Pos)            /**< (SFR) Remote Device Connection Error Interrupt Set Mask */
#define SFR_VBUSRQS_Pos               9                                              /**< (SFR) VBUS Request Set Position */
#define SFR_VBUSRQS                   (_U_(0x1) << SFR_VBUSRQS_Pos)            /**< (SFR) VBUS Request Set Mask */
#define SFR_Msk                       _U_(0x210)                                     /**< (SFR) Register Mask  */


/** \brief SAMHS_DEV_DMA hardware registers */
typedef struct
{
  __IO uint32_t SAMHS_DEV_DMANXTDSC; /**< (SAMHS_DEV_DMA Offset: 0x00) Device DMA Channel Next Descriptor Address Register */
  __IO uint32_t SAMHS_DEV_DMAADDRESS; /**< (SAMHS_DEV_DMA Offset: 0x04) Device DMA Channel Address Register */
  __IO uint32_t SAMHS_DEV_DMACONTROL; /**< (SAMHS_DEV_DMA Offset: 0x08) Device DMA Channel Control Register */
  __IO uint32_t SAMHS_DEV_DMASTATUS; /**< (SAMHS_DEV_DMA Offset: 0x0C) Device DMA Channel Status Register */
} samhs_dev_dma_t;

/** \brief SAMHS_HST_DMA hardware registers */
typedef struct
{
  __IO uint32_t SAMHS_HST_DMANXTDSC; /**< (SAMHS_HST_DMA Offset: 0x00) Host DMA Channel Next Descriptor Address Register */
  __IO uint32_t SAMHS_HST_DMAADDRESS; /**< (SAMHS_HST_DMA Offset: 0x04) Host DMA Channel Address Register */
  __IO uint32_t SAMHS_HST_DMACONTROL; /**< (SAMHS_HST_DMA Offset: 0x08) Host DMA Channel Control Register */
  __IO uint32_t SAMHS_HST_DMASTATUS; /**< (SAMHS_HST_DMA Offset: 0x0C) Host DMA Channel Status Register */
} samhs_hst_dma_t;

/** \brief USBHS hardware registers */
typedef struct
{
  __IO uint32_t SAMHS_DEV_CTRL;  /**< (USBHS Offset: 0x00) Device General Control Register */
  __I  uint32_t SAMHS_DEV_ISR;   /**< (USBHS Offset: 0x04) Device Global Interrupt Status Register */
  __O  uint32_t SAMHS_DEV_ICR;   /**< (USBHS Offset: 0x08) Device Global Interrupt Clear Register */
  __O  uint32_t SAMHS_DEV_IFR;   /**< (USBHS Offset: 0x0C) Device Global Interrupt Set Register */
  __I  uint32_t SAMHS_DEV_IMR;   /**< (USBHS Offset: 0x10) Device Global Interrupt Mask Register */
  __O  uint32_t SAMHS_DEV_IDR;   /**< (USBHS Offset: 0x14) Device Global Interrupt Disable Register */
  __O  uint32_t SAMHS_DEV_IER;   /**< (USBHS Offset: 0x18) Device Global Interrupt Enable Register */
  __IO uint32_t SAMHS_DEV_EPT;   /**< (USBHS Offset: 0x1C) Device Endpoint Register */
  __I  uint32_t SAMHS_DEV_FNUM;  /**< (USBHS Offset: 0x20) Device Frame Number Register */
  __I  uint8_t                        Reserved1[220];
  __IO uint32_t SAMHS_DEV_EPTCFG[10]; /**< (USBHS Offset: 0x100) Device Endpoint Configuration Register */
  __I  uint8_t                        Reserved2[8];
  __I  uint32_t SAMHS_DEV_EPTISR[10]; /**< (USBHS Offset: 0x130) Device Endpoint Interrupt Status Register */
  __I  uint8_t                        Reserved3[8];
  __O  uint32_t SAMHS_DEV_EPTICR[10]; /**< (USBHS Offset: 0x160) Device Endpoint Interrupt Clear Register */
  __I  uint8_t                        Reserved4[8];
  __O  uint32_t SAMHS_DEV_EPTIFR[10]; /**< (USBHS Offset: 0x190) Device Endpoint Interrupt Set Register */
  __I  uint8_t                        Reserved5[8];
  __I  uint32_t SAMHS_DEV_EPTIMR[10]; /**< (USBHS Offset: 0x1C0) Device Endpoint Interrupt Mask Register */
  __I  uint8_t                        Reserved6[8];
  __O  uint32_t SAMHS_DEV_EPTIER[10]; /**< (USBHS Offset: 0x1F0) Device Endpoint Interrupt Enable Register */
  __I  uint8_t                        Reserved7[8];
  __O  uint32_t SAMHS_DEV_EPTIDR[10]; /**< (USBHS Offset: 0x220) Device Endpoint Interrupt Disable Register */
  __I  uint8_t                        Reserved8[200];
       samhs_dev_dma_t SAMHS_DEV_DMA[7]; /**< Offset: 0x310 Device DMA Channel Next Descriptor Address Register */
  __I  uint8_t                        Reserved9[128];
  __IO uint32_t SAMHS_HST_CTRL;  /**< (USBHS Offset: 0x400) Host General Control Register */
  __I  uint32_t SAMHS_HST_ISR;   /**< (USBHS Offset: 0x404) Host Global Interrupt Status Register */
  __O  uint32_t SAMHS_HST_ICR;   /**< (USBHS Offset: 0x408) Host Global Interrupt Clear Register */
  __O  uint32_t SAMHS_HST_IFR;   /**< (USBHS Offset: 0x40C) Host Global Interrupt Set Register */
  __I  uint32_t SAMHS_HST_IMR;   /**< (USBHS Offset: 0x410) Host Global Interrupt Mask Register */
  __O  uint32_t SAMHS_HST_IDR;   /**< (USBHS Offset: 0x414) Host Global Interrupt Disable Register */
  __O  uint32_t SAMHS_HST_IER;   /**< (USBHS Offset: 0x418) Host Global Interrupt Enable Register */
  __IO uint32_t SAMHS_HST_PIP;   /**< (USBHS Offset: 0x41C) Host Pipe Register */
  __IO uint32_t SAMHS_HST_FNUM;  /**< (USBHS Offset: 0x420) Host Frame Number Register */
  __IO uint32_t SAMHS_HST_ADDR1; /**< (USBHS Offset: 0x424) Host Address 1 Register */
  __IO uint32_t SAMHS_HST_ADDR2; /**< (USBHS Offset: 0x428) Host Address 2 Register */
  __IO uint32_t SAMHS_HST_ADDR3; /**< (USBHS Offset: 0x42C) Host Address 3 Register */
  __I  uint8_t                        Reserved10[208];
  __IO uint32_t SAMHS_HST_PIPCFG[10]; /**< (USBHS Offset: 0x500) Host Pipe Configuration Register */
  __I  uint8_t                        Reserved11[8];
  __I  uint32_t SAMHS_HST_PIPISR[10]; /**< (USBHS Offset: 0x530) Host Pipe Status Register */
  __I  uint8_t                        Reserved12[8];
  __O  uint32_t SAMHS_HST_PIPICR[10]; /**< (USBHS Offset: 0x560) Host Pipe Clear Register */
  __I  uint8_t                        Reserved13[8];
  __O  uint32_t SAMHS_HST_PIPIFR[10]; /**< (USBHS Offset: 0x590) Host Pipe Set Register */
  __I  uint8_t                        Reserved14[8];
  __I  uint32_t SAMHS_HST_PIPIMR[10]; /**< (USBHS Offset: 0x5C0) Host Pipe Mask Register */
  __I  uint8_t                        Reserved15[8];
  __O  uint32_t SAMHS_HST_PIPIER[10]; /**< (USBHS Offset: 0x5F0) Host Pipe Enable Register */
  __I  uint8_t                        Reserved16[8];
  __O  uint32_t SAMHS_HST_PIPIDR[10]; /**< (USBHS Offset: 0x620) Host Pipe Disable Register */
  __I  uint8_t                        Reserved17[8];
  __IO uint32_t SAMHS_HST_PIPINRQ[10]; /**< (USBHS Offset: 0x650) Host Pipe IN Request Register */
  __I  uint8_t                        Reserved18[8];
  __IO uint32_t SAMHS_HST_PIPERR[10]; /**< (USBHS Offset: 0x680) Host Pipe Error Register */
  __I  uint8_t                        Reserved19[104];
       samhs_hst_dma_t SAMHS_HST_DMA[7]; /**< Offset: 0x710 Host DMA Channel Next Descriptor Address Register */
  __I  uint8_t                        Reserved20[128];
  __IO uint32_t SAMHS_CTRL;     /**< (USBHS Offset: 0x800) General Control Register */
  __I  uint32_t SAMHS_SR;       /**< (USBHS Offset: 0x804) General Status Register */
  __O  uint32_t SAMHS_SCR;      /**< (USBHS Offset: 0x808) General Status Clear Register */
  __O  uint32_t SAMHS_SFR;      /**< (USBHS Offset: 0x80C) General Status Set Register */
} samhs_reg_t;

#define SAMHS_BASE_REG		(0x40038000U)         /**< \brief (USBHS) Base Address */

#define EP_MAX				10

#define FIFO_RAM_ADDR		(0xA0100000U)

// Errata: The DMA feature is not available for Pipe/Endpoint 7
#define EP_DMA_SUPPORT(epnum) (epnum >= 1 && epnum <= 6)

#endif /* _SAMHS_SAMX7X_H_ */
