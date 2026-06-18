/*
 * The MIT License (MIT)
 *
 * Copyright (c) N Conrad
 * Copyright (c) 2024, hathach (tinyusb.org)
 * Copyright (c) 2025, HiFiPhile (Zixun LI)
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

#ifndef TUSB_FSDEV_COMMON_H
#define TUSB_FSDEV_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common/tusb_common.h"

#if CFG_TUD_ENABLED
  #include "device/dcd.h"
#endif

#if CFG_TUH_ENABLED
  #include "host/hcd.h"
#endif

//--------------------------------------------------------------------+
// FSDEV Register Bit Definitions
// Vendor-independent definitions with U_ prefix to avoid conflicts.
// Based on the common USB FSDEV IP block register layout.
// Lower 16 bits are shared across all variants (STM32, CH32, AT32).
// Upper 16 bits (DRD extensions) only exist on 32-bit DRD MCUs.
//--------------------------------------------------------------------+

// EPnR / CHEPnR - Endpoint/Channel Register
// DTOG and STAT bits are toggle-on-write-1. CTR bits are clear-on-write-0.
//
//   15       14        13    12     11      10     9      8       7        6         5     4      3    2    1    0
//   CTR_RX   DTOG_RX   STAT_RX[1:0]  SETUP   EP_TYPE[1:0]  KIND    CTR_TX   DTOG_TX   STAT_TX[1:0]  EA[3:0]
//
// DRD 32-bit only (C0, G0, H5, U0, U5):
//   31:27    26       25       24      23     22   21   20   19   18   17   16
//   Rsvd     ERR_RX   ERR_TX   LSEP    NAK    DEVADDR[6:0]
#define U_EP_CTR_RX        0x8000u
#define U_EP_DTOG_RX       0x4000u
#define U_EPRX_STAT        0x3000u
#define U_EP_SETUP         0x0800u
#define U_EP_T_FIELD       0x0600u
#define U_EP_KIND          0x0100u
#define U_EP_CTR_TX        0x0080u
#define U_EP_DTOG_TX       0x0040u
#define U_EPTX_STAT        0x0030u
#define U_EPADDR_FIELD     0x000Fu

// DRD 32-bit upper bits
#define U_EP_ERRRX         0x04000000u
#define U_EP_ERRTX         0x02000000u
#define U_EP_LSEP          0x01000000u
#define U_EP_NAK           0x00800000u
#define U_EP_DEVADDR       0x007F0000u
#define U_EP_DEVADDR_Pos   16u

// Endpoint types (EP_TYPE field values)
#define U_EP_BULK          0x0000u
#define U_EP_CONTROL       0x0200u
#define U_EP_ISOCHRONOUS   0x0400u
#define U_EP_INTERRUPT     0x0600u
#define U_EP_TYPE_MASK     (U_EP_T_FIELD)

// EP register mask components (non-toggle bits preserved during read-modify-write)
// Excludes DTOG_RX, STAT_RX, DTOG_TX, STAT_TX (toggle-on-write-1)
#define U_EPREG_MASK_16    (U_EP_CTR_RX | U_EP_SETUP | U_EP_T_FIELD | U_EP_KIND | U_EP_CTR_TX | U_EPADDR_FIELD)
#define U_EPREG_MASK_32    (U_EP_ERRRX | U_EP_ERRTX | U_EP_LSEP | U_EP_NAK | U_EP_DEVADDR | U_EPREG_MASK_16)

// EP register mask selection based on bus width
#ifdef  CFG_TUSB_FSDEV_32BIT
  #define U_EPREG_MASK     U_EPREG_MASK_32
#else
  #define U_EPREG_MASK     U_EPREG_MASK_16
#endif

#define U_EPKIND_MASK      ((uint32_t)(~U_EP_KIND) & U_EPREG_MASK)
#define U_EPTX_DTOGMASK    (U_EPTX_STAT | U_EPREG_MASK)
#define U_EPRX_DTOGMASK    (U_EPRX_STAT | U_EPREG_MASK)

// Bit positions
#define U_EPTX_STAT_Pos    4u
#define U_EP_DTOG_TX_Pos   6u
#define U_EP_CTR_TX_Pos    7u

// Data toggle helpers
#define U_EPTX_DTOG1       0x0010u
#define U_EPTX_DTOG2       0x0020u
#define U_EPRX_DTOG1       0x1000u
#define U_EPRX_DTOG2       0x2000u

// CNTR - Control Register
//   15      14        13    12     11     10       9     8      7    6    5    4        3      2       1     0
//   CTRM    PMAOVRM   ERRM  WKUPM  SUSPM  RESETM   SOFM  ESOFM  Rsvd Rsvd Rsvd RESUME   FSUSP  LPMODE  PDWN  FRES
//
// DRD 32-bit only:
//   31     30:16
//   HOST   Rsvd
#define U_CNTR_CTRM       0x8000u
#define U_CNTR_PMAOVRM    0x4000u
#define U_CNTR_ERRM       0x2000u
#define U_CNTR_WKUPM      0x1000u
#define U_CNTR_SUSPM      0x0800u
#define U_CNTR_RESETM     0x0400u
#define U_CNTR_SOFM       0x0200u
#define U_CNTR_ESOFM      0x0100u
#define U_CNTR_RESUME     0x0010u
#define U_CNTR_FSUSP      0x0008u
#define U_CNTR_LPMODE     0x0004u
#define U_CNTR_PDWN       0x0002u
#define U_CNTR_FRES       0x0001u

#define U_CNTR_HOST       0x80000000u   // DRD: enable host mode
#define U_CNTR_DCON       0x0400u       // DRD host: same bit as RESETM

// ISTR - Interrupt Status Register
//   15    14      13   12    11    10     9    8      7    6    5    4     3    2    1    0
//   CTR   PMAOVR  ERR  WKUP  SUSP  RESET  SOF  ESOF   Rsvd Rsvd Rsvd DIR   EP_ID[3:0]
//
// DRD 32-bit only:
//   31   30         29          28:16
//   Rsvd LS_DCONN   DCON_STAT   Rsvd
#define U_ISTR_CTR        0x8000u
#define U_ISTR_PMAOVR     0x4000u
#define U_ISTR_ERR        0x2000u
#define U_ISTR_WKUP       0x1000u
#define U_ISTR_SUSP       0x0800u
#define U_ISTR_RESET      0x0400u
#define U_ISTR_SOF        0x0200u
#define U_ISTR_ESOF       0x0100u
#define U_ISTR_DIR        0x0010u
#define U_ISTR_EP_ID      0x000Fu

#define U_ISTR_LS_DCONN   0x40000000u   // DRD: low-speed device connected
#define U_ISTR_DCON_STAT  0x20000000u   // DRD: device connection status
#define U_ISTR_DCON       0x0400u       // DRD host: same bit as RESET

// FNR - Frame Number Register (read-only)
//   15    14    13   12   11   10   9    8    7    6    5    4    3    2    1    0
//   RXDP  RXDM  LCK[2:0]       FN[10:0]
#define U_FNR_RXDP        0x8000u
#define U_FNR_RXDM        0x4000u
#define U_FNR_FN          0x07FFu

// DADDR - Device Address Register
//   15:8   7    6    5    4    3    2    1    0
//   Rsvd   EF   ADD[6:0]
#define U_DADDR_EF        0x80u

// LPMCSR - LPM Control and Status Register
// Supported: STM32 F0, L0, L4, G0, G4, C0, H5, U0, WB. Not on: F1, F3, AT32, CH32.
//   15:8           7    6    5    4    3    2    1        0
//   Rsvd           BESL[3:0]      Rsvd REMWAKE  Rsvd LPMACK   LMPEN
#define U_LPMCSR_LMPEN     0x0001u
#define U_LPMCSR_LPMACK    0x0002u
#define U_LPMCSR_REMWAKE   0x0008u
#define U_LPMCSR_BESL      0x00F0u

// BCDR - Battery Charging Detector Register
// Supported: STM32 F0, L0, L4, G0, G4, C0, H5, U0, WB. Not on: F1, F3, AT32, CH32.
//   15    14:8   7        6     5     4      3     2     1      0
//   DPPU  Rsvd   PS2DET   SDET  PDET  DCDET  SDEN  PDEN  DCDEN  BCDEN
#define U_BCDR_BCDEN       0x0001u
#define U_BCDR_DCDEN       0x0002u
#define U_BCDR_PDEN        0x0004u
#define U_BCDR_SDEN        0x0008u
#define U_BCDR_DCDET       0x0010u
#define U_BCDR_PDET        0x0020u
#define U_BCDR_SDET        0x0040u
#define U_BCDR_PS2DET      0x0080u
#define U_BCDR_DPPU        0x8000u

// Channel status (DRD host mode, reuses STAT_TX/STAT_RX bit positions)
#define U_CH_TX_STTX       0x0030u
#define U_CH_TX_ACK_SBUF   0x0000u
#define U_CH_TX_STALL      0x0010u
#define U_CH_TX_NAK        0x0020u

#define U_CH_RX_STRX       0x3000u
#define U_CH_RX_ACK_SBUF   0x0000u
#define U_CH_RX_STALL      0x1000u
#define U_CH_RX_NAK        0x2000u
#define U_CH_RX_VALID      0x3000u

//--------------------------------------------------------------------+
// Registers Typedef
//--------------------------------------------------------------------+
// hardware limit endpoint
#define FSDEV_EP_COUNT 8

// The fsdev_bus_t type can be used for both register and PMA access necessities
#ifdef CFG_TUSB_FSDEV_32BIT
typedef uint32_t fsdev_bus_t;
#else
typedef uint16_t fsdev_bus_t;
#endif

// volatile 32-bit aligned
#define _va32 volatile TU_ATTR_ALIGNED(4)

typedef struct {
  struct {
    _va32 fsdev_bus_t reg;
  } ep[FSDEV_EP_COUNT];

  _va32 uint32_t    RESERVED7[8]; // Reserved
  _va32 fsdev_bus_t CNTR;         // 40: Control register
  _va32 fsdev_bus_t ISTR;         // 44: Interrupt status register
  _va32 fsdev_bus_t FNR;          // 48: Frame number register
  _va32 fsdev_bus_t DADDR;        // 4C: Device address register
  _va32 fsdev_bus_t BTABLE;       // 50: Buffer Table address register
  _va32 fsdev_bus_t LPMCSR;       // 54: LPM Control and Status (not on F1, F3, AT32, CH32)
  _va32 fsdev_bus_t BCDR;         // 58: Battery Charging Detector (not on F1, F3, AT32, CH32)
} fsdev_regs_t;

TU_VERIFY_STATIC(offsetof(fsdev_regs_t, CNTR) == 0x40, "Wrong offset");
TU_VERIFY_STATIC(sizeof(fsdev_regs_t) == 0x5C, "Size is not correct");

#define FSDEV_REG ((fsdev_regs_t *)FSDEV_REG_BASE)

//--------------------------------------------------------------------+
// BTable and PMA Access
//--------------------------------------------------------------------+

// If sharing with CAN, one can set this to be non-zero to give CAN space where it wants it
// Both of these MUST be a multiple of 2, and are in byte units.
#ifndef FSDEV_BTABLE_BASE
  #define FSDEV_BTABLE_BASE 0U
#endif
TU_VERIFY_STATIC((FSDEV_BTABLE_BASE & 0x7) == 0, "BTABLE base must be aligned to 8 bytes");

#define FSDEV_ADDR_DATA_RATIO (CFG_TUSB_FIFO_HWFIFO_ADDR_STRIDE/CFG_TUSB_FIFO_HWFIFO_DATA_STRIDE)

// Need alignment when access address is 32 bit but data is only 16-bit
#if FSDEV_ADDR_DATA_RATIO == 2
  #define fsdev_addr_data_align TU_ATTR_ALIGNED(4)
#else
  #define fsdev_addr_data_align
#endif

enum {
  BTABLE_BUF_TX = 0,
  BTABLE_BUF_RX = 1
};

// Buffer Table is located in Packet Memory Area (PMA) and therefore its address access is forced to either
// 16-bit or 32-bit depending on  CFG_TUSB_FSDEV_32BIT.
// 0: TX (IN), 1: RX (OUT)
typedef union {
  // data is strictly 16-bit access (address could be 32-bit aligned)
  struct {
    volatile fsdev_addr_data_align uint16_t addr;
    volatile fsdev_addr_data_align uint16_t count;
  } ep16[FSDEV_EP_COUNT][2];

  // strictly 32-bit access
  struct {
    volatile uint32_t count_addr;
  } ep32[FSDEV_EP_COUNT][2];
} fsdev_btable_t;

TU_VERIFY_STATIC(sizeof(fsdev_btable_t) == FSDEV_EP_COUNT * 8 * FSDEV_ADDR_DATA_RATIO, "size is not correct");
TU_VERIFY_STATIC(FSDEV_BTABLE_BASE + FSDEV_EP_COUNT * 8 <= CFG_TUSB_FSDEV_PMA_SIZE, "BTABLE does not fit in PMA RAM");

#define FSDEV_BTABLE ((volatile fsdev_btable_t *)(FSDEV_PMA_BASE + FSDEV_ADDR_DATA_RATIO * FSDEV_BTABLE_BASE))

typedef struct {
  volatile fsdev_addr_data_align fsdev_bus_t value;
} fsdev_pma_buf_t;

#define PMA_BUF_AT(_addr) ((fsdev_pma_buf_t *)(FSDEV_PMA_BASE + FSDEV_ADDR_DATA_RATIO * (_addr)))

//--------------------------------------------------------------------+
// Vendor-specific includes
//--------------------------------------------------------------------+
#if defined(TUP_USBIP_FSDEV_STM32)
  #include "fsdev_stm32.h"
#elif defined(TUP_USBIP_FSDEV_CH32)
  #include "fsdev_ch32.h"
#elif defined(TUP_USBIP_FSDEV_AT32)
  #include "fsdev_at32.h"
#else
  #error "Unknown USB IP"
#endif

//--------------------------------------------------------------------+
// Endpoint Helper
// - CTR is write 0 to clear
// - DTOG and STAT are write 1 to toggle
//--------------------------------------------------------------------+
typedef enum {
  EP_STAT_DISABLED = 0,
  EP_STAT_STALL    = 1,
  EP_STAT_NAK      = 2,
  EP_STAT_VALID    = 3
} ep_stat_t;

#define EP_STAT_MASK(_dir) (3u << (U_EPTX_STAT_Pos + ((_dir) == TUSB_DIR_IN ? 0 : 8)))
#define EP_DTOG_MASK(_dir) (1u << (U_EP_DTOG_TX_Pos + ((_dir) == TUSB_DIR_IN ? 0 : 8)))

#define CH_STAT_MASK(_dir) (3u << (U_EPTX_STAT_Pos + ((_dir) == TUSB_DIR_IN ? 8 : 0)))
#define CH_DTOG_MASK(_dir) (1u << (U_EP_DTOG_TX_Pos + ((_dir) == TUSB_DIR_IN ? 8 : 0)))

TU_ATTR_ALWAYS_INLINE static inline uint32_t ep_read(uint32_t ep_id) {
  return FSDEV_REG->ep[ep_id].reg;
}

TU_ATTR_ALWAYS_INLINE static inline void ep_write(uint32_t ep_id, uint32_t value, bool need_exclusive) {
  if (need_exclusive) {
    fsdev_int_disable(0);
  }

  FSDEV_REG->ep[ep_id].reg = (fsdev_bus_t)value;

  if (need_exclusive) {
    fsdev_int_enable(0);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void ep_write_clear_ctr(uint32_t ep_id, tusb_dir_t dir) {
  uint32_t reg = FSDEV_REG->ep[ep_id].reg;
  reg |= U_EP_CTR_TX | U_EP_CTR_RX;
  reg &= U_EPREG_MASK;
  reg &= ~(1u << (U_EP_CTR_TX_Pos + (dir == TUSB_DIR_IN ? 0u : 8u)));
  ep_write(ep_id, reg, false);
}

TU_ATTR_ALWAYS_INLINE static inline void ep_change_status(uint32_t *reg, tusb_dir_t dir, ep_stat_t state) {
  *reg ^= (state << (U_EPTX_STAT_Pos + (dir == TUSB_DIR_IN ? 0 : 8)));
}

TU_ATTR_ALWAYS_INLINE static inline void ep_change_dtog(uint32_t *reg, tusb_dir_t dir, uint8_t state) {
  *reg ^= (state << (U_EP_DTOG_TX_Pos + (dir == TUSB_DIR_IN ? 0 : 8)));
}

TU_ATTR_ALWAYS_INLINE static inline bool ep_is_iso(uint32_t reg) {
  return (reg & U_EP_TYPE_MASK) == U_EP_ISOCHRONOUS;
}

//--------------------------------------------------------------------+
// Channel Helper
// - Direction is opposite to endpoint direction
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint32_t ch_read(uint32_t ch_id) {
  return ep_read(ch_id);
}

TU_ATTR_ALWAYS_INLINE static inline void ch_write(uint32_t ch_id, uint32_t value, bool need_exclusive) {
  ep_write(ch_id, value, need_exclusive);
}

TU_ATTR_ALWAYS_INLINE static inline void ch_write_clear_ctr(uint32_t ch_id, tusb_dir_t dir) {
  uint32_t reg = FSDEV_REG->ep[ch_id].reg;
  reg |= U_EP_CTR_TX | U_EP_CTR_RX;
  reg &= U_EPREG_MASK;
  reg &= ~(1u << (U_EP_CTR_TX_Pos + (dir == TUSB_DIR_IN ? 8u : 0u)));
  ep_write(ch_id, reg, false);
}

TU_ATTR_ALWAYS_INLINE static inline void ch_change_status(uint32_t *reg, tusb_dir_t dir, ep_stat_t state) {
  *reg ^= (state << (U_EPTX_STAT_Pos + (dir == TUSB_DIR_IN ? 8 : 0)));
}

TU_ATTR_ALWAYS_INLINE static inline void ch_change_dtog(uint32_t *reg, tusb_dir_t dir, uint8_t state) {
  *reg ^= (state << (U_EP_DTOG_TX_Pos + (dir == TUSB_DIR_IN ? 8 : 0)));
}

//--------------------------------------------------------------------+
// BTable Helper
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint32_t btable_get_addr(uint32_t ep_id, uint8_t buf_id) {
#ifdef  CFG_TUSB_FSDEV_32BIT
  return FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr & 0x0000FFFFu;
#else
  return FSDEV_BTABLE->ep16[ep_id][buf_id].addr;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void btable_set_addr(uint32_t ep_id, uint8_t buf_id, uint16_t addr) {
#ifdef  CFG_TUSB_FSDEV_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr          = (count_addr & 0xFFFF0000u) | (addr & 0x0000FFFCu);

  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  FSDEV_BTABLE->ep16[ep_id][buf_id].addr = addr;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t btable_get_count(uint32_t ep_id, uint8_t buf_id) {
  uint16_t count;
#ifdef  CFG_TUSB_FSDEV_32BIT
  count = (FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr >> 16);
#else
  count = FSDEV_BTABLE->ep16[ep_id][buf_id].count;
#endif
  return count & 0x3FFU;
}

TU_ATTR_ALWAYS_INLINE static inline void btable_set_count(uint32_t ep_id, uint8_t buf_id, uint16_t byte_count) {
#ifdef  CFG_TUSB_FSDEV_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr          = (count_addr & ~0x03FF0000u) | ((byte_count & 0x3FFu) << 16);

  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  uint16_t cnt = FSDEV_BTABLE->ep16[ep_id][buf_id].count;
  cnt          = (cnt & ~0x3FFU) | (byte_count & 0x3FFU);

  FSDEV_BTABLE->ep16[ep_id][buf_id].count = cnt;
#endif
}

// Reset the USB Core
void fsdev_core_reset(void);

// De-initialize the USB Core
void fsdev_deinit(void);

// Aligned buffer size according to hardware
uint16_t pma_align_buffer_size(uint16_t size, uint8_t *blsize, uint8_t *num_block);

// Set RX buffer size
void btable_set_rx_bufsize(uint32_t ep_id, uint8_t buf_id, uint16_t wCount);

#ifdef __cplusplus
}
#endif

#endif /* TUSB_FSDEV_COMMON_H */
