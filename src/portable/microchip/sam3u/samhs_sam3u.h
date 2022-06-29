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

#ifndef _SAMHS_SAM3U_H_
#define _SAMHS_SAM3U_H_

/* -------- SAMHS_DEV_CTRL : (SAMHS Offset: 0x00) SAMHS Control Register -------- */
#define SAMHS_DEV_CTRL_DEV_ADDR_Pos 0
#define SAMHS_DEV_CTRL_DEV_ADDR_Msk (0x7fu << SAMHS_DEV_CTRL_DEV_ADDR_Pos) /**< \brief (SAMHS_DEV_CTRL) SAMHS Address */
#define SAMHS_DEV_CTRL_DEV_ADDR(value) ((SAMHS_DEV_CTRL_DEV_ADDR_Msk & ((value) << SAMHS_DEV_CTRL_DEV_ADDR_Pos)))
#define SAMHS_DEV_CTRL_FADDR_EN (0x1u << 7) /**< \brief (SAMHS_DEV_CTRL) Function Address Enable */
#define SAMHS_DEV_CTRL_EN_SAMHS (0x1u << 8) /**< \brief (SAMHS_DEV_CTRL) SAMHS Enable */
#define SAMHS_DEV_CTRL_DETACH (0x1u << 9) /**< \brief (SAMHS_DEV_CTRL) Detach Command */
#define SAMHS_DEV_CTRL_REWAKEUP (0x1u << 10) /**< \brief (SAMHS_DEV_CTRL) Send Remote Wake Up */
#define SAMHS_DEV_CTRL_PULLD_DIS (0x1u << 11) /**< \brief (SAMHS_DEV_CTRL) Pull-Down Disable */
/* -------- SAMHS_DEV_FNUM : (SAMHS Offset: 0x04) SAMHS Frame Number Register -------- */
#define SAMHS_DEV_FNUM_MICRO_FRAME_NUM_Pos 0
#define SAMHS_DEV_FNUM_MICRO_FRAME_NUM_Msk (0x7u << SAMHS_DEV_FNUM_MICRO_FRAME_NUM_Pos) /**< \brief (SAMHS_DEV_FNUM) Microframe Number */
#define SAMHS_DEV_FNUM_FRAME_NUMBER_Pos 3
#define SAMHS_DEV_FNUM_FRAME_NUMBER_Msk (0x7ffu << SAMHS_DEV_FNUM_FRAME_NUMBER_Pos) /**< \brief (SAMHS_DEV_FNUM) Frame Number as defined in the Packet Field Formats */
#define SAMHS_DEV_FNUM_FNUM_ERR (0x1u << 31) /**< \brief (SAMHS_DEV_FNUM) Frame Number CRC Error */
/* -------- SAMHS_DEV_IEN : (SAMHS Offset: 0x10) SAMHS Interrupt Enable Register -------- */
#define SAMHS_DEV_IEN_DET_SUSPD (0x1u << 1) /**< \brief (SAMHS_DEV_IEN) Suspend Interrupt Enable */
#define SAMHS_DEV_IEN_MICRO_SOF (0x1u << 2) /**< \brief (SAMHS_DEV_IEN) Micro-SOF Interrupt Enable */
#define SAMHS_DEV_IEN_INT_SOF (0x1u << 3) /**< \brief (SAMHS_DEV_IEN) SOF Interrupt Enable */
#define SAMHS_DEV_IEN_ENDRESET (0x1u << 4) /**< \brief (SAMHS_DEV_IEN) End Of Reset Interrupt Enable */
#define SAMHS_DEV_IEN_WAKE_UP (0x1u << 5) /**< \brief (SAMHS_DEV_IEN) Wake Up CPU Interrupt Enable */
#define SAMHS_DEV_IEN_ENDOFRSM (0x1u << 6) /**< \brief (SAMHS_DEV_IEN) End Of Resume Interrupt Enable */
#define SAMHS_DEV_IEN_UPSTR_RES (0x1u << 7) /**< \brief (SAMHS_DEV_IEN) Upstream Resume Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_0 (0x1u << 8) /**< \brief (SAMHS_DEV_IEN) Endpoint 0 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_1 (0x1u << 9) /**< \brief (SAMHS_DEV_IEN) Endpoint 1 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_2 (0x1u << 10) /**< \brief (SAMHS_DEV_IEN) Endpoint 2 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_3 (0x1u << 11) /**< \brief (SAMHS_DEV_IEN) Endpoint 3 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_4 (0x1u << 12) /**< \brief (SAMHS_DEV_IEN) Endpoint 4 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_5 (0x1u << 13) /**< \brief (SAMHS_DEV_IEN) Endpoint 5 Interrupt Enable */
#define SAMHS_DEV_IEN_EPT_6 (0x1u << 14) /**< \brief (SAMHS_DEV_IEN) Endpoint 6 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_1 (0x1u << 25) /**< \brief (SAMHS_DEV_IEN) DMA Channel 1 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_2 (0x1u << 26) /**< \brief (SAMHS_DEV_IEN) DMA Channel 2 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_3 (0x1u << 27) /**< \brief (SAMHS_DEV_IEN) DMA Channel 3 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_4 (0x1u << 28) /**< \brief (SAMHS_DEV_IEN) DMA Channel 4 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_5 (0x1u << 29) /**< \brief (SAMHS_DEV_IEN) DMA Channel 5 Interrupt Enable */
#define SAMHS_DEV_IEN_DMA_6 (0x1u << 30) /**< \brief (SAMHS_DEV_IEN) DMA Channel 6 Interrupt Enable */
/* -------- SAMHS_DEV_INTSTA : (SAMHS Offset: 0x14) SAMHS Interrupt Status Register -------- */
#define SAMHS_DEV_INTSTA_SPEED (0x1u << 0) /**< \brief (SAMHS_DEV_INTSTA) Speed Status */
#define SAMHS_DEV_INTSTA_DET_SUSPD (0x1u << 1) /**< \brief (SAMHS_DEV_INTSTA) Suspend Interrupt */
#define SAMHS_DEV_INTSTA_MICRO_SOF (0x1u << 2) /**< \brief (SAMHS_DEV_INTSTA) Micro Start Of Frame Interrupt */
#define SAMHS_DEV_INTSTA_INT_SOF (0x1u << 3) /**< \brief (SAMHS_DEV_INTSTA) Start Of Frame Interrupt */
#define SAMHS_DEV_INTSTA_ENDRESET (0x1u << 4) /**< \brief (SAMHS_DEV_INTSTA) End Of Reset Interrupt */
#define SAMHS_DEV_INTSTA_WAKE_UP (0x1u << 5) /**< \brief (SAMHS_DEV_INTSTA) Wake Up CPU Interrupt */
#define SAMHS_DEV_INTSTA_ENDOFRSM (0x1u << 6) /**< \brief (SAMHS_DEV_INTSTA) End Of Resume Interrupt */
#define SAMHS_DEV_INTSTA_UPSTR_RES (0x1u << 7) /**< \brief (SAMHS_DEV_INTSTA) Upstream Resume Interrupt */
#define SAMHS_DEV_INTSTA_EPT_0 (0x1u << 8) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 0 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_1 (0x1u << 9) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 1 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_2 (0x1u << 10) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 2 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_3 (0x1u << 11) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 3 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_4 (0x1u << 12) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 4 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_5 (0x1u << 13) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 5 Interrupt */
#define SAMHS_DEV_INTSTA_EPT_6 (0x1u << 14) /**< \brief (SAMHS_DEV_INTSTA) Endpoint 6 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_1 (0x1u << 25) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 1 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_2 (0x1u << 26) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 2 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_3 (0x1u << 27) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 3 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_4 (0x1u << 28) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 4 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_5 (0x1u << 29) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 5 Interrupt */
#define SAMHS_DEV_INTSTA_DMA_6 (0x1u << 30) /**< \brief (SAMHS_DEV_INTSTA) DMA Channel 6 Interrupt */
/* -------- SAMHS_DEV_CLRINT : (SAMHS Offset: 0x18) SAMHS Clear Interrupt Register -------- */
#define SAMHS_DEV_CLRINT_DET_SUSPD (0x1u << 1) /**< \brief (SAMHS_DEV_CLRINT) Suspend Interrupt Clear */
#define SAMHS_DEV_CLRINT_MICRO_SOF (0x1u << 2) /**< \brief (SAMHS_DEV_CLRINT) Micro Start Of Frame Interrupt Clear */
#define SAMHS_DEV_CLRINT_INT_SOF (0x1u << 3) /**< \brief (SAMHS_DEV_CLRINT) Start Of Frame Interrupt Clear */
#define SAMHS_DEV_CLRINT_ENDRESET (0x1u << 4) /**< \brief (SAMHS_DEV_CLRINT) End Of Reset Interrupt Clear */
#define SAMHS_DEV_CLRINT_WAKE_UP (0x1u << 5) /**< \brief (SAMHS_DEV_CLRINT) Wake Up CPU Interrupt Clear */
#define SAMHS_DEV_CLRINT_ENDOFRSM (0x1u << 6) /**< \brief (SAMHS_DEV_CLRINT) End Of Resume Interrupt Clear */
#define SAMHS_DEV_CLRINT_UPSTR_RES (0x1u << 7) /**< \brief (SAMHS_DEV_CLRINT) Upstream Resume Interrupt Clear */
/* -------- SAMHS_DEV_EPTRST : (SAMHS Offset: 0x1C) SAMHS Endpoints Reset Register -------- */
#define SAMHS_DEV_EPTRST_EPT_0 (0x1u << 0) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 0 Reset */
#define SAMHS_DEV_EPTRST_EPT_1 (0x1u << 1) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 1 Reset */
#define SAMHS_DEV_EPTRST_EPT_2 (0x1u << 2) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 2 Reset */
#define SAMHS_DEV_EPTRST_EPT_3 (0x1u << 3) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 3 Reset */
#define SAMHS_DEV_EPTRST_EPT_4 (0x1u << 4) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 4 Reset */
#define SAMHS_DEV_EPTRST_EPT_5 (0x1u << 5) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 5 Reset */
#define SAMHS_DEV_EPTRST_EPT_6 (0x1u << 6) /**< \brief (SAMHS_DEV_EPTRST) Endpoint 6 Reset */
/* -------- SAMHS_DEV_TST : (SAMHS Offset: 0xE0) SAMHS Test Register -------- */
#define SAMHS_DEV_TST_SPEED_CFG_Pos 0
#define SAMHS_DEV_TST_SPEED_CFG_Msk (0x3u << SAMHS_DEV_TST_SPEED_CFG_Pos) /**< \brief (SAMHS_DEV_TST) Speed Configuration */
#define SAMHS_DEV_TST_SPEED_CFG(value) ((SAMHS_DEV_TST_SPEED_CFG_Msk & ((value) << SAMHS_DEV_TST_SPEED_CFG_Pos)))
#define   SAMHS_DEV_TST_SPEED_CFG_NORMAL (0x0u << 0) /**< \brief (SAMHS_DEV_TST) Normal Mode: The macro is in Full Speed mode, ready to make a High Speed identification, if the host supports it and then to automatically switch to High Speed mode */
#define   SAMHS_DEV_TST_SPEED_CFG_HIGH_SPEED (0x2u << 0) /**< \brief (SAMHS_DEV_TST) Force High Speed: Set this value to force the hardware to work in High Speed mode. Only for debug or test purpose. */
#define   SAMHS_DEV_TST_SPEED_CFG_FULL_SPEED (0x3u << 0) /**< \brief (SAMHS_DEV_TST) Force Full Speed: Set this value to force the hardware to work only in Full Speed mode. In this configuration, the macro will not respond to a High Speed reset handshake. */
#define SAMHS_DEV_TST_TST_J (0x1u << 2) /**< \brief (SAMHS_DEV_TST) Test J Mode */
#define SAMHS_DEV_TST_TST_K (0x1u << 3) /**< \brief (SAMHS_DEV_TST) Test K Mode */
#define SAMHS_DEV_TST_TST_PKT (0x1u << 4) /**< \brief (SAMHS_DEV_TST) Test Packet Mode */
#define SAMHS_DEV_TST_OPMODE2 (0x1u << 5) /**< \brief (SAMHS_DEV_TST) OpMode2 */
/* -------- SAMHS_DEV_EPTCFG : (SAMHS Offset: N/A) SAMHS Endpoint Configuration Register -------- */
#define SAMHS_DEV_EPTCFG_EPT_SIZE_Pos 0
#define SAMHS_DEV_EPTCFG_EPT_SIZE_Msk (0x7u << SAMHS_DEV_EPTCFG_EPT_SIZE_Pos) /**< \brief (SAMHS_DEV_EPTCFG) Endpoint Size */
#define SAMHS_DEV_EPTCFG_EPT_SIZE(value) ((SAMHS_DEV_EPTCFG_EPT_SIZE_Msk & ((value) << SAMHS_DEV_EPTCFG_EPT_SIZE_Pos)))
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_8 (0x0u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 8 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_16 (0x1u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 16 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_32 (0x2u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 32 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_64 (0x3u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 64 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_128 (0x4u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 128 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_256 (0x5u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 256 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_512 (0x6u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 512 bytes */
#define   SAMHS_DEV_EPTCFG_EPT_SIZE_1024 (0x7u << 0) /**< \brief (SAMHS_DEV_EPTCFG) 1024 bytes */
#define SAMHS_DEV_EPTCFG_EPT_DIR (0x1u << 3) /**< \brief (SAMHS_DEV_EPTCFG) Endpoint Direction */
#define SAMHS_DEV_EPTCFG_EPT_TYPE_Pos 4
#define SAMHS_DEV_EPTCFG_EPT_TYPE_Msk (0x3u << SAMHS_DEV_EPTCFG_EPT_TYPE_Pos) /**< \brief (SAMHS_DEV_EPTCFG) Endpoint Type */
#define SAMHS_DEV_EPTCFG_EPT_TYPE(value) ((SAMHS_DEV_EPTCFG_EPT_TYPE_Msk & ((value) << SAMHS_DEV_EPTCFG_EPT_TYPE_Pos)))
#define   SAMHS_DEV_EPTCFG_EPT_TYPE_CTRL8 (0x0u << 4) /**< \brief (SAMHS_DEV_EPTCFG) Control endpoint */
#define   SAMHS_DEV_EPTCFG_EPT_TYPE_ISO (0x1u << 4) /**< \brief (SAMHS_DEV_EPTCFG) Isochronous endpoint */
#define   SAMHS_DEV_EPTCFG_EPT_TYPE_BULK (0x2u << 4) /**< \brief (SAMHS_DEV_EPTCFG) Bulk endpoint */
#define   SAMHS_DEV_EPTCFG_EPT_TYPE_INT (0x3u << 4) /**< \brief (SAMHS_DEV_EPTCFG) Interrupt endpoint */
#define SAMHS_DEV_EPTCFG_BK_NUMBER_Pos 6
#define SAMHS_DEV_EPTCFG_BK_NUMBER_Msk (0x3u << SAMHS_DEV_EPTCFG_BK_NUMBER_Pos) /**< \brief (SAMHS_DEV_EPTCFG) Number of Banks */
#define SAMHS_DEV_EPTCFG_BK_NUMBER(value) ((SAMHS_DEV_EPTCFG_BK_NUMBER_Msk & ((value) << SAMHS_DEV_EPTCFG_BK_NUMBER_Pos)))
#define   SAMHS_DEV_EPTCFG_BK_NUMBER_0 (0x0u << 6) /**< \brief (SAMHS_DEV_EPTCFG) Zero bank, the endpoint is not mapped in memory */
#define   SAMHS_DEV_EPTCFG_BK_NUMBER_1 (0x1u << 6) /**< \brief (SAMHS_DEV_EPTCFG) One bank (bank 0) */
#define   SAMHS_DEV_EPTCFG_BK_NUMBER_2 (0x2u << 6) /**< \brief (SAMHS_DEV_EPTCFG) Double bank (Ping-Pong: bank0/bank1) */
#define   SAMHS_DEV_EPTCFG_BK_NUMBER_3 (0x3u << 6) /**< \brief (SAMHS_DEV_EPTCFG) Triple bank (bank0/bank1/bank2) */
#define SAMHS_DEV_EPTCFG_NB_TRANS_Pos 8
#define SAMHS_DEV_EPTCFG_NB_TRANS_Msk (0x3u << SAMHS_DEV_EPTCFG_NB_TRANS_Pos) /**< \brief (SAMHS_DEV_EPTCFG) Number Of Transaction per Microframe */
#define SAMHS_DEV_EPTCFG_NB_TRANS(value) ((SAMHS_DEV_EPTCFG_NB_TRANS_Msk & ((value) << SAMHS_DEV_EPTCFG_NB_TRANS_Pos)))
#define SAMHS_DEV_EPTCFG_EPT_MAPD (0x1u << 31) /**< \brief (SAMHS_DEV_EPTCFG) Endpoint Mapped */
/* -------- SAMHS_DEV_EPTCTLENB : (SAMHS Offset: N/A) SAMHS Endpoint Control Enable Register -------- */
#define SAMHS_DEV_EPTCTLENB_EPT_ENABL (0x1u << 0) /**< \brief (SAMHS_DEV_EPTCTLENB) Endpoint Enable */
#define SAMHS_DEV_EPTCTLENB_AUTO_VALID (0x1u << 1) /**< \brief (SAMHS_DEV_EPTCTLENB) Packet Auto-Valid Enable */
#define SAMHS_DEV_EPTCTLENB_INTDIS_DMA (0x1u << 3) /**< \brief (SAMHS_DEV_EPTCTLENB) Interrupts Disable DMA */
#define SAMHS_DEV_EPTCTLENB_NYET_DIS (0x1u << 4) /**< \brief (SAMHS_DEV_EPTCTLENB) NYET Disable (Only for High Speed Bulk OUT endpoints) */
#define SAMHS_DEV_EPTCTLENB_ERR_OVFLW (0x1u << 8) /**< \brief (SAMHS_DEV_EPTCTLENB) Overflow Error Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTCTLENB) Received OUT Data Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_TX_COMPLT (0x1u << 10) /**< \brief (SAMHS_DEV_EPTCTLENB) Transmitted IN Data Complete Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_TXRDY (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTLENB) TX Packet Ready Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_RX_SETUP (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTLENB) Received SETUP */
#define SAMHS_DEV_EPTCTLENB_STALL_SNT (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTLENB) Stall Sent Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_NAK_IN (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTLENB) NAKIN Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_NAK_OUT (0x1u << 15) /**< \brief (SAMHS_DEV_EPTCTLENB) NAKOUT Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_BUSY_BANK (0x1u << 18) /**< \brief (SAMHS_DEV_EPTCTLENB) Busy Bank Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_SHRT_PCKT (0x1u << 31) /**< \brief (SAMHS_DEV_EPTCTLENB) Short Packet Send/Short Packet Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_DATAX_RX (0x1u << 6) /**< \brief (SAMHS_DEV_EPTCTLENB) DATAx Interrupt Enable (Only for high bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTLENB_MDATA_RX (0x1u << 7) /**< \brief (SAMHS_DEV_EPTCTLENB) MDATA Interrupt Enable (Only for high bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTLENB_TXRDY_TRER (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTLENB) TX Packet Ready/Transaction Error Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_ERR_FL_ISO (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTLENB) Error Flow Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_ERR_CRC_NTR (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTLENB) ISO CRC Error/Number of Transaction Error Interrupt Enable */
#define SAMHS_DEV_EPTCTLENB_ERR_FLUSH (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTLENB) Bank Flush Error Interrupt Enable */
/* -------- SAMHS_DEV_EPTCTLDIS : (SAMHS Offset: N/A) SAMHS Endpoint Control Disable Register -------- */
#define SAMHS_DEV_EPTCTLDIS_EPT_DISABL (0x1u << 0) /**< \brief (SAMHS_DEV_EPTCTLDIS) Endpoint Disable */
#define SAMHS_DEV_EPTCTLDIS_AUTO_VALID (0x1u << 1) /**< \brief (SAMHS_DEV_EPTCTLDIS) Packet Auto-Valid Disable */
#define SAMHS_DEV_EPTCTLDIS_INTDIS_DMA (0x1u << 3) /**< \brief (SAMHS_DEV_EPTCTLDIS) Interrupts Disable DMA */
#define SAMHS_DEV_EPTCTLDIS_NYET_DIS (0x1u << 4) /**< \brief (SAMHS_DEV_EPTCTLDIS) NYET Enable (Only for High Speed Bulk OUT endpoints) */
#define SAMHS_DEV_EPTCTLDIS_ERR_OVFLW (0x1u << 8) /**< \brief (SAMHS_DEV_EPTCTLDIS) Overflow Error Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTCTLDIS) Received OUT Data Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_TX_COMPLT (0x1u << 10) /**< \brief (SAMHS_DEV_EPTCTLDIS) Transmitted IN Data Complete Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_TXRDY (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTLDIS) TX Packet Ready Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_RX_SETUP (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTLDIS) Received SETUP Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_STALL_SNT (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTLDIS) Stall Sent Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_NAK_IN (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTLDIS) NAKIN Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_NAK_OUT (0x1u << 15) /**< \brief (SAMHS_DEV_EPTCTLDIS) NAKOUT Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_BUSY_BANK (0x1u << 18) /**< \brief (SAMHS_DEV_EPTCTLDIS) Busy Bank Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_SHRT_PCKT (0x1u << 31) /**< \brief (SAMHS_DEV_EPTCTLDIS) Short Packet Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_DATAX_RX (0x1u << 6) /**< \brief (SAMHS_DEV_EPTCTLDIS) DATAx Interrupt Disable (Only for High Bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTLDIS_MDATA_RX (0x1u << 7) /**< \brief (SAMHS_DEV_EPTCTLDIS) MDATA Interrupt Disable (Only for High Bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTLDIS_TXRDY_TRER (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTLDIS) TX Packet Ready/Transaction Error Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_ERR_FL_ISO (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTLDIS) Error Flow Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_ERR_CRC_NTR (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTLDIS) ISO CRC Error/Number of Transaction Error Interrupt Disable */
#define SAMHS_DEV_EPTCTLDIS_ERR_FLUSH (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTLDIS) bank flush error Interrupt Disable */
/* -------- SAMHS_DEV_EPTCTL : (SAMHS Offset: N/A) SAMHS Endpoint Control Register -------- */
#define SAMHS_DEV_EPTCTL_EPT_ENABL (0x1u << 0) /**< \brief (SAMHS_DEV_EPTCTL) Endpoint Enable */
#define SAMHS_DEV_EPTCTL_AUTO_VALID (0x1u << 1) /**< \brief (SAMHS_DEV_EPTCTL) Packet Auto-Valid Enabled (Not for CONTROL Endpoints) */
#define SAMHS_DEV_EPTCTL_INTDIS_DMA (0x1u << 3) /**< \brief (SAMHS_DEV_EPTCTL) Interrupt Disables DMA */
#define SAMHS_DEV_EPTCTL_NYET_DIS (0x1u << 4) /**< \brief (SAMHS_DEV_EPTCTL) NYET Disable (Only for High Speed Bulk OUT endpoints) */
#define SAMHS_DEV_EPTCTL_ERR_OVFLW (0x1u << 8) /**< \brief (SAMHS_DEV_EPTCTL) Overflow Error Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTCTL) Received OUT Data Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_TX_COMPLT (0x1u << 10) /**< \brief (SAMHS_DEV_EPTCTL) Transmitted IN Data Complete Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_TXRDY (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTL) TX Packet Ready Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_RX_SETUP (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTL) Received SETUP Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_STALL_SNT (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTL) Stall Sent Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_NAK_IN (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTL) NAKIN Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_NAK_OUT (0x1u << 15) /**< \brief (SAMHS_DEV_EPTCTL) NAKOUT Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_BUSY_BANK (0x1u << 18) /**< \brief (SAMHS_DEV_EPTCTL) Busy Bank Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_SHRT_PCKT (0x1u << 31) /**< \brief (SAMHS_DEV_EPTCTL) Short Packet Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_DATAX_RX (0x1u << 6) /**< \brief (SAMHS_DEV_EPTCTL) DATAx Interrupt Enabled (Only for High Bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTL_MDATA_RX (0x1u << 7) /**< \brief (SAMHS_DEV_EPTCTL) MDATA Interrupt Enabled (Only for High Bandwidth Isochronous OUT endpoints) */
#define SAMHS_DEV_EPTCTL_TXRDY_TRER (0x1u << 11) /**< \brief (SAMHS_DEV_EPTCTL) TX Packet Ready/Transaction Error Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_ERR_FL_ISO (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCTL) Error Flow Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_ERR_CRC_NTR (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCTL) ISO CRC Error/Number of Transaction Error Interrupt Enabled */
#define SAMHS_DEV_EPTCTL_ERR_FLUSH (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCTL) Bank Flush Error Interrupt Enabled */
/* -------- SAMHS_DEV_EPTSETSTA : (SAMHS Offset: N/A) SAMHS Endpoint Set Status Register -------- */
#define SAMHS_DEV_EPTSETSTA_FRCESTALL (0x1u << 5) /**< \brief (SAMHS_DEV_EPTSETSTA) Stall Handshake Request Set */
#define SAMHS_DEV_EPTSETSTA_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTSETSTA) KILL Bank Set (for IN Endpoint) */
#define SAMHS_DEV_EPTSETSTA_TXRDY (0x1u << 11) /**< \brief (SAMHS_DEV_EPTSETSTA) TX Packet Ready Set */
#define SAMHS_DEV_EPTSETSTA_TXRDY_TRER (0x1u << 11) /**< \brief (SAMHS_DEV_EPTSETSTA) TX Packet Ready Set */
/* -------- SAMHS_DEV_EPTCLRSTA : (SAMHS Offset: N/A) SAMHS Endpoint Clear Status Register -------- */
#define SAMHS_DEV_EPTCLRSTA_FRCESTALL (0x1u << 5) /**< \brief (SAMHS_DEV_EPTCLRSTA) Stall Handshake Request Clear */
#define SAMHS_DEV_EPTCLRSTA_TOGGLESQ (0x1u << 6) /**< \brief (SAMHS_DEV_EPTCLRSTA) Data Toggle Clear */
#define SAMHS_DEV_EPTCLRSTA_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTCLRSTA) Received OUT Data Clear */
#define SAMHS_DEV_EPTCLRSTA_TX_COMPLT (0x1u << 10) /**< \brief (SAMHS_DEV_EPTCLRSTA) Transmitted IN Data Complete Clear */
#define SAMHS_DEV_EPTCLRSTA_RX_SETUP (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCLRSTA) Received SETUP Clear */
#define SAMHS_DEV_EPTCLRSTA_STALL_SNT (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCLRSTA) Stall Sent Clear */
#define SAMHS_DEV_EPTCLRSTA_NAK_IN (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCLRSTA) NAKIN Clear */
#define SAMHS_DEV_EPTCLRSTA_NAK_OUT (0x1u << 15) /**< \brief (SAMHS_DEV_EPTCLRSTA) NAKOUT Clear */
#define SAMHS_DEV_EPTCLRSTA_ERR_FL_ISO (0x1u << 12) /**< \brief (SAMHS_DEV_EPTCLRSTA) Error Flow Clear */
#define SAMHS_DEV_EPTCLRSTA_ERR_CRC_NTR (0x1u << 13) /**< \brief (SAMHS_DEV_EPTCLRSTA) Number of Transaction Error Clear */
#define SAMHS_DEV_EPTCLRSTA_ERR_FLUSH (0x1u << 14) /**< \brief (SAMHS_DEV_EPTCLRSTA) Bank Flush Error Clear */
/* -------- SAMHS_DEV_EPTSTA : (SAMHS Offset: N/A) SAMHS Endpoint Status Register -------- */
#define SAMHS_DEV_EPTSTA_FRCESTALL (0x1u << 5) /**< \brief (SAMHS_DEV_EPTSTA) Stall Handshake Request */
#define SAMHS_DEV_EPTSTA_TOGGLESQ_STA_Pos 6
#define SAMHS_DEV_EPTSTA_TOGGLESQ_STA_Msk (0x3u << SAMHS_DEV_EPTSTA_TOGGLESQ_STA_Pos) /**< \brief (SAMHS_DEV_EPTSTA) Toggle Sequencing */
#define   SAMHS_DEV_EPTSTA_TOGGLESQ_STA_DATA0 (0x0u << 6) /**< \brief (SAMHS_DEV_EPTSTA) DATA0 */
#define   SAMHS_DEV_EPTSTA_TOGGLESQ_STA_DATA1 (0x1u << 6) /**< \brief (SAMHS_DEV_EPTSTA) DATA1 */
#define   SAMHS_DEV_EPTSTA_TOGGLESQ_STA_DATA2 (0x2u << 6) /**< \brief (SAMHS_DEV_EPTSTA) Reserved for High Bandwidth Isochronous Endpoint */
#define   SAMHS_DEV_EPTSTA_TOGGLESQ_STA_MDATA (0x3u << 6) /**< \brief (SAMHS_DEV_EPTSTA) Reserved for High Bandwidth Isochronous Endpoint */
#define SAMHS_DEV_EPTSTA_ERR_OVFLW (0x1u << 8) /**< \brief (SAMHS_DEV_EPTSTA) Overflow Error */
#define SAMHS_DEV_EPTSTA_RXRDY_TXKL (0x1u << 9) /**< \brief (SAMHS_DEV_EPTSTA) Received OUT Data/KILL Bank */
#define SAMHS_DEV_EPTSTA_TX_COMPLT (0x1u << 10) /**< \brief (SAMHS_DEV_EPTSTA) Transmitted IN Data Complete */
#define SAMHS_DEV_EPTSTA_TXRDY (0x1u << 11) /**< \brief (SAMHS_DEV_EPTSTA) TX Packet Ready */
#define SAMHS_DEV_EPTSTA_RX_SETUP (0x1u << 12) /**< \brief (SAMHS_DEV_EPTSTA) Received SETUP */
#define SAMHS_DEV_EPTSTA_STALL_SNT (0x1u << 13) /**< \brief (SAMHS_DEV_EPTSTA) Stall Sent */
#define SAMHS_DEV_EPTSTA_NAK_IN (0x1u << 14) /**< \brief (SAMHS_DEV_EPTSTA) NAK IN */
#define SAMHS_DEV_EPTSTA_NAK_OUT (0x1u << 15) /**< \brief (SAMHS_DEV_EPTSTA) NAK OUT */
#define SAMHS_DEV_EPTSTA_CURBK_CTLDIR_Pos 16
#define SAMHS_DEV_EPTSTA_CURBK_CTLDIR_Msk (0x3u << SAMHS_DEV_EPTSTA_CURBK_CTLDIR_Pos) /**< \brief (SAMHS_DEV_EPTSTA) Current Bank/Control Direction */
#define SAMHS_DEV_EPTSTA_BUSY_BANK_STA_Pos 18
#define SAMHS_DEV_EPTSTA_BUSY_BANK_STA_Msk (0x3u << SAMHS_DEV_EPTSTA_BUSY_BANK_STA_Pos) /**< \brief (SAMHS_DEV_EPTSTA) Busy Bank Number */
#define   SAMHS_DEV_EPTSTA_BUSY_BANK_STA_1BUSYBANK (0x0u << 18) /**< \brief (SAMHS_DEV_EPTSTA) 1 busy bank */
#define   SAMHS_DEV_EPTSTA_BUSY_BANK_STA_2BUSYBANKS (0x1u << 18) /**< \brief (SAMHS_DEV_EPTSTA) 2 busy banks */
#define   SAMHS_DEV_EPTSTA_BUSY_BANK_STA_3BUSYBANKS (0x2u << 18) /**< \brief (SAMHS_DEV_EPTSTA) 3 busy banks */
#define SAMHS_DEV_EPTSTA_BYTE_COUNT_Pos 20
#define SAMHS_DEV_EPTSTA_BYTE_COUNT_Msk (0x7ffu << SAMHS_DEV_EPTSTA_BYTE_COUNT_Pos) /**< \brief (SAMHS_DEV_EPTSTA) SAMHS Byte Count */
#define SAMHS_DEV_EPTSTA_SHRT_PCKT (0x1u << 31) /**< \brief (SAMHS_DEV_EPTSTA) Short Packet */
#define SAMHS_DEV_EPTSTA_TXRDY_TRER (0x1u << 11) /**< \brief (SAMHS_DEV_EPTSTA) TX Packet Ready/Transaction Error */
#define SAMHS_DEV_EPTSTA_ERR_FL_ISO (0x1u << 12) /**< \brief (SAMHS_DEV_EPTSTA) Error Flow */
#define SAMHS_DEV_EPTSTA_ERR_CRC_NTR (0x1u << 13) /**< \brief (SAMHS_DEV_EPTSTA) CRC ISO Error/Number of Transaction Error */
#define SAMHS_DEV_EPTSTA_ERR_FLUSH (0x1u << 14) /**< \brief (SAMHS_DEV_EPTSTA) Bank Flush Error */
#define SAMHS_DEV_EPTSTA_CURBK_Pos 16
#define SAMHS_DEV_EPTSTA_CURBK_Msk (0x3u << SAMHS_DEV_EPTSTA_CURBK_Pos) /**< \brief (SAMHS_DEV_EPTSTA) Current Bank */
#define   SAMHS_DEV_EPTSTA_CURBK_BANK0 (0x0u << 16) /**< \brief (SAMHS_DEV_EPTSTA) Bank 0 (or single bank) */
#define   SAMHS_DEV_EPTSTA_CURBK_BANK1 (0x1u << 16) /**< \brief (SAMHS_DEV_EPTSTA) Bank 1 */
#define   SAMHS_DEV_EPTSTA_CURBK_BANK2 (0x2u << 16) /**< \brief (SAMHS_DEV_EPTSTA) Bank 2 */
/* -------- SAMHS_DEV_DMANXTDSC : (SAMHS Offset: N/A) SAMHS DMA Next Descriptor Address Register -------- */
#define SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Pos 0
#define SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Msk (0xffffffffu << SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Pos) /**< \brief (SAMHS_DEV_DMANXTDSC) Next Descriptor Address */
#define SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD(value) ((SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Msk & ((value) << SAMHS_DEV_DMANXTDSC_NXT_DSC_ADD_Pos)))
/* -------- SAMHS_DEV_DMAADDRESS : (SAMHS Offset: N/A) SAMHS DMA Channel Address Register -------- */
#define SAMHS_DEV_DMAADDRESS_BUFF_ADD_Pos 0
#define SAMHS_DEV_DMAADDRESS_BUFF_ADD_Msk (0xffffffffu << SAMHS_DEV_DMAADDRESS_BUFF_ADD_Pos) /**< \brief (SAMHS_DEV_DMAADDRESS) Buffer Address */
#define SAMHS_DEV_DMAADDRESS_BUFF_ADD(value) ((SAMHS_DEV_DMAADDRESS_BUFF_ADD_Msk & ((value) << SAMHS_DEV_DMAADDRESS_BUFF_ADD_Pos)))
/* -------- SAMHS_DEV_DMACONTROL : (SAMHS Offset: N/A) SAMHS DMA Channel Control Register -------- */
#define SAMHS_DEV_DMACONTROL_CHANN_ENB (0x1u << 0) /**< \brief (SAMHS_DEV_DMACONTROL) (Channel Enable Command) */
#define SAMHS_DEV_DMACONTROL_LDNXT_DSC (0x1u << 1) /**< \brief (SAMHS_DEV_DMACONTROL) Load Next Channel Transfer Descriptor Enable (Command) */
#define SAMHS_DEV_DMACONTROL_END_TR_EN (0x1u << 2) /**< \brief (SAMHS_DEV_DMACONTROL) End of Transfer Enable (Control) */
#define SAMHS_DEV_DMACONTROL_END_B_EN (0x1u << 3) /**< \brief (SAMHS_DEV_DMACONTROL) End of Buffer Enable (Control) */
#define SAMHS_DEV_DMACONTROL_END_TR_IT (0x1u << 4) /**< \brief (SAMHS_DEV_DMACONTROL) End of Transfer Interrupt Enable */
#define SAMHS_DEV_DMACONTROL_END_BUFFIT (0x1u << 5) /**< \brief (SAMHS_DEV_DMACONTROL) End of Buffer Interrupt Enable */
#define SAMHS_DEV_DMACONTROL_DESC_LD_IT (0x1u << 6) /**< \brief (SAMHS_DEV_DMACONTROL) Descriptor Loaded Interrupt Enable */
#define SAMHS_DEV_DMACONTROL_BURST_LCK (0x1u << 7) /**< \brief (SAMHS_DEV_DMACONTROL) Burst Lock Enable */
#define SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos 16
#define SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Msk (0xffffu << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos) /**< \brief (SAMHS_DEV_DMACONTROL) Buffer Byte Length (Write-only) */
#define SAMHS_DEV_DMACONTROL_BUFF_LENGTH(value) ((SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Msk & ((value) << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos)))
/* -------- SAMHS_DEV_DMASTATUS : (SAMHS Offset: N/A) SAMHS DMA Channel Status Register -------- */
#define SAMHS_DEV_DMASTATUS_CHANN_ENB (0x1u << 0) /**< \brief (SAMHS_DEV_DMASTATUS) Channel Enable Status */
#define SAMHS_DEV_DMASTATUS_CHANN_ACT (0x1u << 1) /**< \brief (SAMHS_DEV_DMASTATUS) Channel Active Status */
#define SAMHS_DEV_DMASTATUS_END_TR_ST (0x1u << 4) /**< \brief (SAMHS_DEV_DMASTATUS) End of Channel Transfer Status */
#define SAMHS_DEV_DMASTATUS_END_BF_ST (0x1u << 5) /**< \brief (SAMHS_DEV_DMASTATUS) End of Channel Buffer Status */
#define SAMHS_DEV_DMASTATUS_DESC_LDST (0x1u << 6) /**< \brief (SAMHS_DEV_DMASTATUS) Descriptor Loaded Status */
#define SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos 16
#define SAMHS_DEV_DMASTATUS_BUFF_COUNT_Msk (0xffffu << SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos) /**< \brief (SAMHS_DEV_DMASTATUS) Buffer Byte Count */
#define SAMHS_DEV_DMASTATUS_BUFF_COUNT(value) ((SAMHS_DEV_DMASTATUS_BUFF_COUNT_Msk & ((value) << SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos)))


/** \brief samhs_dma hardware registers */
typedef struct {
  __IO uint32_t SAMHS_DEV_DMANXTDSC;  /**< \brief (samhs_dma Offset: 0x0) SAMHS DMA Next Descriptor Address Register */
  __IO uint32_t SAMHS_DEV_DMAADDRESS; /**< \brief (samhs_dma Offset: 0x4) SAMHS DMA Channel Address Register */
  __IO uint32_t SAMHS_DEV_DMACONTROL; /**< \brief (samhs_dma Offset: 0x8) SAMHS DMA Channel Control Register */
  __IO uint32_t SAMHS_DEV_DMASTATUS;  /**< \brief (samhs_dma Offset: 0xC) SAMHS DMA Channel Status Register */
} samhs_dma_t;

/** \brief samhs_ept hardware registers */
typedef struct {
  __IO uint32_t SAMHS_DEV_EPTCFG;    /**< \brief (samhs_ept Offset: 0x0) SAMHS Endpoint Configuration Register */
  __O  uint32_t SAMHS_DEV_EPTCTLENB; /**< \brief (samhs_ept Offset: 0x4) SAMHS Endpoint Control Enable Register */
  __O  uint32_t SAMHS_DEV_EPTCTLDIS; /**< \brief (samhs_ept Offset: 0x8) SAMHS Endpoint Control Disable Register */
  __I  uint32_t SAMHS_DEV_EPTCTL;    /**< \brief (samhs_ept Offset: 0xC) SAMHS Endpoint Control Register */
  __I  uint32_t Reserved1[1];
  __O  uint32_t SAMHS_DEV_EPTSETSTA; /**< \brief (samhs_ept Offset: 0x14) SAMHS Endpoint Set Status Register */
  __O  uint32_t SAMHS_DEV_EPTCLRSTA; /**< \brief (samhs_ept Offset: 0x18) SAMHS Endpoint Clear Status Register */
  __I  uint32_t SAMHS_DEV_EPTSTA;    /**< \brief (samhs_ept Offset: 0x1C) SAMHS Endpoint Status Register */
} samhs_ept_t;

/** \brief samhs_reg hardware registers */
#define SAMHS_EPT_NUMBER 7
#define SAMHS_DMA_NUMBER 6

typedef struct {
  __IO uint32_t SAMHS_DEV_CTRL;                 /**< \brief (samhs_reg Offset: 0x00) SAMHS Control Register */
  __I  uint32_t SAMHS_DEV_FNUM;                 /**< \brief (samhs_reg Offset: 0x04) SAMHS Frame Number Register */
  __I  uint32_t Reserved1[2];
  __IO uint32_t SAMHS_DEV_IEN;                  /**< \brief (samhs_reg Offset: 0x10) SAMHS Interrupt Enable Register */
  __I  uint32_t SAMHS_DEV_INTSTA;               /**< \brief (samhs_reg Offset: 0x14) SAMHS Interrupt Status Register */
  __O  uint32_t SAMHS_DEV_CLRINT;               /**< \brief (samhs_reg Offset: 0x18) SAMHS Clear Interrupt Register */
  __O  uint32_t SAMHS_DEV_EPTRST;               /**< \brief (samhs_reg Offset: 0x1C) SAMHS Endpoints Reset Register */
  __I  uint32_t Reserved2[48];
  __IO uint32_t SAMHS_DEV_TST;                  /**< \brief (samhs_reg Offset: 0xE0) SAMHS Test Register */
  __I  uint32_t Reserved3[7];
       samhs_ept_t SAMHS_DEV_EPT[SAMHS_EPT_NUMBER]; /**< \brief (samhs_reg Offset: 0x100) endpoint = 0 .. 6 */
  __I  uint32_t Reserved4[72];
       samhs_dma_t SAMHS_DEV_DMA[SAMHS_DMA_NUMBER]; /**< \brief (samhs_reg Offset: 0x300) channel = 0 .. 5 */
} samhs_reg_t;

#define SAMHS_BASE_REG		(0x400A4000U)         /**< \brief (USBHS) Base Address */

#define EP_MAX           	7

#define FIFO_RAM_ADDR		(0x20180000U)

// Errata: The DMA feature is not available for Pipe/Endpoint 7
#define EP_DMA_SUPPORT(epnum) (epnum >= 1 && epnum <= 6)

#endif /* _SAMHS_SAM3U_H_ */
