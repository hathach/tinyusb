/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Ha Thach (tinyusb.org)
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

#include "tusb_option.h"

#if defined(TUP_USBIP_FSDEV) && (CFG_TUH_ENABLED || CFG_TUD_ENABLED)

#include "fsdev_common.h"

//--------------------------------------------------------------------+
// Global
//--------------------------------------------------------------------+

// Reset the USB Core
void fsdev_core_reset(void) {
  // Perform USB peripheral reset
  FSDEV_REG->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for (volatile uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }

  FSDEV_REG->CNTR &= ~USB_CNTR_PDWN;

  // Wait startup time, for F042 and F070, this is <= 1 us.
  for (volatile uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }

  // Clear pending interrupts
  FSDEV_REG->ISTR = 0;
}

// De-initialize the USB Core
void fsdev_deinit(void) {
  // Disable all interrupts and force USB reset
  FSDEV_REG->CNTR = USB_CNTR_FRES;

  // Clear pending interrupts
  FSDEV_REG->ISTR = 0;

  // Put USB peripheral in power down mode
  FSDEV_REG->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for (volatile uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }
}


//--------------------------------------------------------------------+
// PMA read/write
//--------------------------------------------------------------------+

// Write to packet memory area (PMA) from user memory
// - Packet memory must be either strictly 16-bit or 32-bit depending on FSDEV_BUS_32BIT
// - Uses unaligned for RAM (since M0 cannot access unaligned address)
bool fsdev_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t nbytes) {
  if (nbytes == 0) {
    return true;
  }
  uint32_t n_write = nbytes / FSDEV_BUS_SIZE;

  fsdev_pma_buf_t* pma_buf = PMA_BUF_AT(dst);
  const uint8_t *src8 = src;

  while (n_write--) {
    pma_buf->value = fsdevbus_unaligned_read(src8);
    src8 += FSDEV_BUS_SIZE;
    pma_buf++;
  }

  // odd bytes e.g 1 for 16-bit or 1-3 for 32-bit
  uint16_t odd = nbytes & (FSDEV_BUS_SIZE - 1);
  if (odd) {
    fsdev_bus_t temp = 0;
    for(uint16_t i = 0; i < odd; i++) {
      temp |= *src8++ << (i * 8);
    }
    pma_buf->value = temp;
  }

  return true;
}

// Read from packet memory area (PMA) to user memory.
// - Packet memory must be either strictly 16-bit or 32-bit depending on FSDEV_BUS_32BIT
// - Uses unaligned for RAM (since M0 cannot access unaligned address)
bool fsdev_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t nbytes) {
  if (nbytes == 0) {
    return true;
  }
  uint32_t n_read = nbytes / FSDEV_BUS_SIZE;

  fsdev_pma_buf_t* pma_buf = PMA_BUF_AT(src);
  uint8_t *dst8 = (uint8_t *)dst;

  while (n_read--) {
    fsdevbus_unaligned_write(dst8, (fsdev_bus_t ) pma_buf->value);
    dst8 += FSDEV_BUS_SIZE;
    pma_buf++;
  }

  // odd bytes e.g 1 for 16-bit or 1-3 for 32-bit
  uint16_t odd = nbytes & (FSDEV_BUS_SIZE - 1);
  if (odd) {
    fsdev_bus_t temp = pma_buf->value;
    while (odd--) {
      *dst8++ = (uint8_t) (temp & 0xfful);
      temp >>= 8;
    }
  }

  return true;
}

//--------------------------------------------------------------------+
// BTable Helper
//--------------------------------------------------------------------+

// Aligned buffer size according to hardware
uint16_t pma_align_buffer_size(uint16_t size, uint8_t* blsize, uint8_t* num_block) {
  /* The STM32 full speed USB peripheral supports only a limited set of
   * buffer sizes given by the RX buffer entry format in the USB_BTABLE. */
  uint16_t block_in_bytes;
  if (size > 62) {
    block_in_bytes = 32;
    *blsize = 1;
    *num_block = tu_div_ceil(size, 32);
  } else {
    block_in_bytes = 2;
    *blsize = 0;
    *num_block = tu_div_ceil(size, 2);
  }

  return (*num_block) * block_in_bytes;
}

// Set RX buffer size
void btable_set_rx_bufsize(uint32_t ep_id, uint8_t buf_id, uint16_t wCount) {
  uint8_t blsize, num_block;
  (void) pma_align_buffer_size(wCount, &blsize, &num_block);

  /* Encode into register. When BLSIZE==1, we need to subtract 1 block count */
  uint16_t bl_nb = (blsize << 15) | ((num_block - blsize) << 10);
  if (bl_nb == 0) {
    // zlp but 0 is invalid value, set blsize to 1 (32 bytes)
    // Note: lower value can cause PMAOVR on setup with ch32v203
    bl_nb = 1 << 15;
  }

#ifdef FSDEV_BUS_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr = (bl_nb << 16) | (count_addr & 0x0000FFFFu);
  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  FSDEV_BTABLE->ep16[ep_id][buf_id].count = bl_nb;
#endif
}

#endif
