/*
 * The MIT License (MIT)
 *
 * Copyright(c) 2016 STMicroelectronics
 * Copyright(c) N Conrad
 * Copyright (c) 2024, hathach (tinyusb.org)
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
 */

#ifndef TUSB_FSDEV_COMMON_H
#define TUSB_FSDEV_COMMON_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdint.h"

// If sharing with CAN, one can set this to be non-zero to give CAN space where it wants it
// Both of these MUST be a multiple of 2, and are in byte units.
#ifndef FSDEV_BTABLE_BASE
#define FSDEV_BTABLE_BASE 0U
#endif

TU_VERIFY_STATIC(FSDEV_BTABLE_BASE % 8 == 0, "BTABLE base must be aligned to 8 bytes");

// FSDEV_PMA_SIZE is PMA buffer size in bytes.
// - 512-byte devices, access with a stride of two words (use every other 16-bit address)
// - 1024-byte devices, access with a stride of one word (use every 16-bit address)
// - 2048-byte devices, access with 32-bit address

// For purposes of accessing the packet
#if FSDEV_PMA_SIZE == 512
  #define FSDEV_PMA_STRIDE  (2u) // 1x16 bit access scheme
  #define pma_aligned TU_ATTR_ALIGNED(4)
#elif FSDEV_PMA_SIZE == 1024
  #define FSDEV_PMA_STRIDE  (1u) // 2x16 bit access scheme
  #define pma_aligned
#elif FSDEV_PMA_SIZE == 2048
  #ifndef FSDEV_BUS_32BIT
    #warning "FSDEV_PMA_SIZE is 2048, but FSDEV_BUS_32BIT is not defined"
  #endif
  #define FSDEV_PMA_STRIDE  (1u) // 32 bit access scheme
  #define pma_aligned
#endif

enum {
  BTABLE_BUF_TX = 0,
  BTABLE_BUF_RX = 1
};

// hardware limit endpoint
#define FSDEV_EP_COUNT 8

// Buffer Table is located in Packet Memory Area (PMA) and therefore its address access is forced to either
// 16-bit or 32-bit depending on FSDEV_BUS_32BIT.
typedef struct {
  union {
    // 0: TX (IN), 1: RX (OUT)

    // strictly 16-bit access (could be 32-bit aligned)
    struct {
      volatile pma_aligned uint16_t addr;
      volatile pma_aligned uint16_t count;
    } ep16[FSDEV_EP_COUNT][2];

    // strictly 32-bit access
    struct {
      volatile uint32_t count_addr;
    } ep32[FSDEV_EP_COUNT][2];
  };
} fsdev_btable_t;

TU_VERIFY_STATIC(sizeof(fsdev_btable_t) == FSDEV_EP_COUNT*8*FSDEV_PMA_STRIDE, "size is not correct");
TU_VERIFY_STATIC(FSDEV_BTABLE_BASE + FSDEV_EP_COUNT*8 <= FSDEV_PMA_SIZE, "BTABLE does not fit in PMA RAM");


#define FSDEV_BTABLE ((volatile fsdev_btable_t*) (USB_PMAADDR+FSDEV_BTABLE_BASE))

// volatile 32-bit aligned
#define _va32     volatile TU_ATTR_ALIGNED(4)

// The fsdev_bus_t type can be used for both register and PMA access necessities
// For type-safety create a new macro for the volatile address of PMAADDR
// The compiler should warn us if we cast it to a non-volatile type?
#ifdef FSDEV_BUS_32BIT
typedef uint32_t fsdev_bus_t;
static volatile uint32_t * const pma32 = (volatile uint32_t*)USB_PMAADDR;

#else
typedef uint16_t fsdev_bus_t;
// Volatile is also needed to prevent the optimizer from changing access to 32-bit (as 32-bit access is forbidden)
static volatile uint16_t * const pma = (volatile uint16_t*)USB_PMAADDR;
#endif

typedef struct {
  struct {
    _va32 fsdev_bus_t reg;
  }ep[FSDEV_EP_COUNT];

  _va32 uint32_t RESERVED7[8];       // Reserved
  _va32 fsdev_bus_t CNTR;            // 40: Control register
  _va32 fsdev_bus_t ISTR;            // 44: Interrupt status register
  _va32 fsdev_bus_t FNR;             // 48: Frame number register
  _va32 fsdev_bus_t DADDR;           // 4C: Device address register
  _va32 fsdev_bus_t BTABLE;          // 50: Buffer Table address register (16-bit only)
  _va32 fsdev_bus_t LPMCSR;          // 54: LPM Control and Status Register (32-bit only)
  _va32 fsdev_bus_t BCDR;            // 58: Battery Charging Detector Register (32-bit only)
} fsdev_regs_t;

TU_VERIFY_STATIC(offsetof(fsdev_regs_t, CNTR) == 0x40, "Wrong offset");
TU_VERIFY_STATIC(sizeof(fsdev_regs_t) == 0x5C, "Size is not correct");

#define FSDEV_REG ((fsdev_regs_t*) FSDEV_REG_BASE)


#ifndef USB_EPTX_STAT
#define USB_EPTX_STAT 0x0030U
#endif

#ifndef USB_EPRX_STAT
#define USB_EPRX_STAT 0x3000U
#endif

#ifndef USB_EP_DTOG_TX_Pos
#define USB_EP_DTOG_TX_Pos                      (6U)
#endif

#ifndef USB_EP_DTOG_RX_Pos
#define USB_EP_DTOG_RX_Pos                      (14U)
#endif

//--------------------------------------------------------------------+
// BTable
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint32_t btable_get_addr(uint32_t ep_id, uint8_t buf_id) {
#ifdef FSDEV_BUS_32BIT
  return FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr & 0x0000FFFFu;
#else
  return FSDEV_BTABLE->ep16[ep_id][buf_id].addr;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void btable_set_addr(uint32_t ep_id, uint8_t buf_id, uint16_t addr) {
#ifdef FSDEV_BUS_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr = (count_addr & 0xFFFF0000u) | (addr & 0x0000FFFCu);
  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  FSDEV_BTABLE->ep16[ep_id][buf_id].addr = addr;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t btable_get_count(uint32_t ep_id, uint8_t buf_id) {
  uint16_t count;
#ifdef FSDEV_BUS_32BIT
  count = (FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr >> 16);
#else
  count = FSDEV_BTABLE->ep16[ep_id][buf_id].count;
#endif
  return count & 0x3FFU;
}

TU_ATTR_ALWAYS_INLINE static inline void btable_set_count(uint32_t ep_id, uint8_t buf_id, uint16_t byte_count) {
#ifdef FSDEV_BUS_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr = (count_addr & ~0x03FF0000u) | ((byte_count & 0x3FFu) << 16);
  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  uint16_t cnt = FSDEV_BTABLE->ep16[ep_id][buf_id].count;
  cnt = (cnt & ~0x3FFU) | (byte_count & 0x3FFU);
  FSDEV_BTABLE->ep16[ep_id][buf_id].count = cnt;
#endif
}

/* Aligned buffer size according to hardware */
TU_ATTR_ALWAYS_INLINE static inline uint16_t pma_align_buffer_size(uint16_t size, uint8_t* blsize, uint8_t* num_block) {
  /* The STM32 full speed USB peripheral supports only a limited set of
   * buffer sizes given by the RX buffer entry format in the USB_BTABLE. */
  uint16_t block_in_bytes;
  if (size > 62) {
    block_in_bytes = 32;
    *blsize = 1;
  } else {
    block_in_bytes = 2;
    *blsize = 0;
  }

  *num_block = tu_div_ceil(size, block_in_bytes);

  return (*num_block) * block_in_bytes;
}

TU_ATTR_ALWAYS_INLINE static inline void btable_set_rx_bufsize(uint32_t ep_id, uint8_t buf_id, uint32_t wCount) {
  uint8_t blsize, num_block;
  (void) pma_align_buffer_size(wCount, &blsize, &num_block);

  /* Encode into register. When BLSIZE==1, we need to subtract 1 block count */
  uint16_t bl_nb = (blsize << 15) | ((num_block - blsize) << 10);

#ifdef FSDEV_BUS_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr = (bl_nb << 16) | (count_addr & 0x0000FFFFu);
  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  FSDEV_BTABLE->ep16[ep_id][buf_id].count = bl_nb;
#endif
}

//--------------------------------------------------------------------+
// Endpoint
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_endpoint(USB_TypeDef * USBx, uint32_t bEpIdx, uint32_t wRegValue) {
  (void) USBx;
  FSDEV_REG->ep[bEpIdx].reg = (fsdev_bus_t) wRegValue;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_endpoint(USB_TypeDef * USBx, uint32_t bEpIdx) {
  (void) USBx;
  return FSDEV_REG->ep[bEpIdx].reg;
}

/**
  * @brief  Sets address in an endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  bAddr Address.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_address(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t bAddr) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= bAddr;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_eptype(USB_TypeDef * USBx, uint32_t bEpIdx, uint32_t wType) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= (uint32_t)USB_EP_T_MASK;
  regVal |= wType;
  regVal |= USB_EP_CTR_RX | USB_EP_CTR_TX; // These clear on write0, so must set high
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_eptype(USB_TypeDef * USBx, uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EP_T_FIELD;
  return regVal;
}

/**
  * @brief  Clears bit CTR_RX / CTR_TX in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_rx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal &= ~USB_EP_CTR_RX;
  regVal |= USB_EP_CTR_TX; // preserve CTR_TX (clears on writing 0)
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_tx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal &= ~USB_EP_CTR_TX;
  regVal |= USB_EP_CTR_RX; // preserve CTR_RX (clears on writing 0)
  pcd_set_endpoint(USBx, bEpIdx,regVal);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t ep_add_tx_status(uint32_t reg, uint32_t state) {
  return reg ^ state;
}
TU_ATTR_ALWAYS_INLINE static inline uint32_t ep_add_rx_status(uint32_t reg, uint32_t state) {
  return reg ^ state;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t ep_add_tx_dtog(uint32_t reg, uint32_t state) {
  return reg ^ (state << USB_EP_DTOG_TX_Pos);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t ep_add_rx_dtog(uint32_t reg, uint32_t state) {
  return reg ^ (state << USB_EP_DTOG_RX_Pos);
}

/**
  * @brief  sets the status for tx transfer (bits STAT_TX[1:0]).
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  wState new state
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_tx_status(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wState) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPTX_DTOGMASK;
  regVal ^= wState;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

/**
  * @brief  sets the status for rx transfer (bits STAT_TX[1:0])
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  wState new state
  * @retval None
  */

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wState) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPRX_DTOGMASK;
  regVal ^= wState;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

//TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpIdx) {
//  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
//  return (regVal & USB_EPRX_STAT) >> (12u);
//}

TU_ATTR_ALWAYS_INLINE static inline void pcd_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_RX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  if((regVal & USB_EP_DTOG_RX) != 0) {
    pcd_rx_dtog(USBx,bEpIdx);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx) {
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  if((regVal & USB_EP_DTOG_TX) != 0) {
    pcd_tx_dtog(USBx,bEpIdx);
  }
}

//TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_kind(USB_TypeDef * USBx,  uint32_t bEpIdx) {
//  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
//  regVal |= USB_EP_KIND;
//  regVal &= USB_EPREG_MASK;
//  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
//  pcd_set_endpoint(USBx, bEpIdx, regVal);
//}
//
//TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_ep_kind(USB_TypeDef * USBx, uint32_t bEpIdx) {
//  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
//  regVal &= USB_EPKIND_MASK;
//  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
//  pcd_set_endpoint(USBx, bEpIdx, regVal);
//}

#ifdef __cplusplus
 }
#endif

#endif
