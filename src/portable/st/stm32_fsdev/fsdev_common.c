/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2025, HiFiPhile (Zixun LI)
 * SPDX-License-Identifier: MIT
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
  FSDEV_REG->CNTR = U_CNTR_FRES | U_CNTR_PDWN;
  for (volatile uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }

  FSDEV_REG->CNTR &= ~U_CNTR_PDWN;

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
  FSDEV_REG->CNTR = U_CNTR_FRES;

  // Clear pending interrupts
  FSDEV_REG->ISTR = 0;

  // Put USB peripheral in power down mode
  FSDEV_REG->CNTR = U_CNTR_FRES | U_CNTR_PDWN;
  for (volatile uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }
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
    *num_block = (uint8_t)tu_div_ceil(size, 32);
  } else {
    block_in_bytes = 2;
    *blsize = 0;
    *num_block = (uint8_t)tu_div_ceil(size, 2);
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

#ifdef  CFG_TUSB_FSDEV_32BIT
  uint32_t count_addr = FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr;
  count_addr = (bl_nb << 16) | (count_addr & 0x0000FFFFu);
  FSDEV_BTABLE->ep32[ep_id][buf_id].count_addr = count_addr;
#else
  FSDEV_BTABLE->ep16[ep_id][buf_id].count = bl_nb;
#endif
}

#endif
