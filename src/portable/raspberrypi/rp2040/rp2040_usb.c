/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021 Ha Thach (tinyusb.org) for Double Buffered
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

#if CFG_TUSB_MCU == OPT_MCU_RP2040 && (CFG_TUD_ENABLED || CFG_TUH_ENABLED)

  #include <stdlib.h>
  #include "rp2040_usb.h"

  #include "device/dcd.h"
  #include "host/hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPE
//--------------------------------------------------------------------+
  #if CFG_TUSB_RP2_ERRATA_E15
static bool e15_is_critical_frame_period(void);
  #endif

  #if CFG_TUSB_RP2_ERRATA_E2
static uint8_t rp2040_chipversion = 2;
  #endif

critical_section_t rp2usb_lock;

//--------------------------------------------------------------------+
// Implementation
//--------------------------------------------------------------------+
// Provide own byte by byte memcpy as not all copies are aligned.
// Use volatile to prevent compiler from widening to 16/32-bit accesses
// which cause hard fault on RP2350 when dst/src point to USB DPRAM.
static void unaligned_memcpy(uint8_t *dst, const uint8_t *src, size_t n) {
  volatile uint8_t *vdst = dst;
  const volatile uint8_t *vsrc = src;
  while (n--) {
    *vdst++ = *vsrc++;
  }
}

  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
void tu_hwfifo_write(volatile void *hwfifo, const uint8_t *src, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void)access_mode;
  unaligned_memcpy((uint8_t *)(uintptr_t)hwfifo, src, len);
}

void tu_hwfifo_read(const volatile void *hwfifo, uint8_t *dest, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void)access_mode;
  unaligned_memcpy(dest, (const uint8_t *)(uintptr_t)hwfifo, len);
}
  #endif

void rp2usb_init(void) {
  // Reset usb controller
  reset_block(RESETS_RESET_USBCTRL_BITS);
  unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

  #ifdef __GNUC__
    // Clear any previous state just in case
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Warray-bounds"
    #if __GNUC__ > 6
      #pragma GCC diagnostic ignored "-Wstringop-overflow"
    #endif
  #endif
  memset(usb_dpram, 0, sizeof(*usb_dpram));
  #ifdef __GNUC__
    #pragma GCC diagnostic pop
  #endif

  // Mux the controller to the onboard usb phy
  usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;

  #if CFG_TUSB_RP2_ERRATA_E2
  rp2040_chipversion = rp2040_chip_version();
  #endif

  TU_LOG2_INT(sizeof(hw_endpoint_t));

  critical_section_init(&rp2usb_lock);
}

void __tusb_irq_path_func(rp2usb_reset_transfer)(hw_endpoint_t *ep) {
  ep->state         = EPSTATE_IDLE;
  ep->remaining_len = 0;
  ep->xferred_len   = 0;
  ep->user_buf      = 0;
#if CFG_TUD_EDPT_DEDICATED_HWFIFO
  ep->is_xfer_fifo  = false;
#endif
}

void __tusb_irq_path_func(bufctrl_write32)(io_rw_32 *buf_reg, uint32_t value) {
  const uint32_t current    = *buf_reg;
  const uint32_t avail_mask = USB_BUF_CTRL_AVAIL | (USB_BUF_CTRL_AVAIL << 16);
  if (current & value & avail_mask) {
    panic("buf_ctrl @ 0x%lX already available", (uintptr_t)buf_reg);
  }
  *buf_reg = value & ~(USB_BUF_CTRL_AVAIL | (USB_BUF_CTRL_AVAIL << 16)); // write other bits first

  // Section 4.1.2.7.1 (rp2040) / 12.7.3.7.1 (rp2350) Concurrent access: after write to buffer control,
  // wait for USB controller to see the update before setting AVAILABLE.
  // Don't need delay in host mode as host is in charge of when to start the transaction.
  if (value & (USB_BUF_CTRL_AVAIL | (USB_BUF_CTRL_AVAIL << 16))) {
    if (!rp2usb_is_host_mode()) {
      busy_wait_at_least_cycles(12);
    }
    *buf_reg = value; // then set AVAILABLE bit last
  }
}

void __tusb_irq_path_func(bufctrl_write16)(io_rw_16 *buf_reg16, uint16_t value) {
  const uint16_t current = *buf_reg16;
  if (current & value & USB_BUF_CTRL_AVAIL) {
    panic("buf_ctrl @ 0x%lX already available", (uintptr_t)buf_reg16);
  }
  *buf_reg16 = value & (uint16_t)~USB_BUF_CTRL_AVAIL; // write other bits first

  // Section 4.1.2.7.1 (rp2040) / 12.7.3.7.1 (rp2350) Concurrent access
  if (value & USB_BUF_CTRL_AVAIL) {
    if (!rp2usb_is_host_mode()) {
      busy_wait_at_least_cycles(12);
    }
    *buf_reg16 = value; // then set AVAILABLE bit last
  }
}

// prepare buffer, move data if tx, return buffer control
uint16_t __tusb_irq_path_func(bufctrl_prepare16)(hw_endpoint_t *ep, uint8_t *dpram_buf, bool is_rx) {
  const uint16_t buflen = tu_min16(ep->remaining_len, ep->max_packet_size);
  ep->remaining_len -= buflen;

  uint16_t buf_ctrl = buflen | USB_BUF_CTRL_AVAIL;
  if (ep->next_pid) {
    buf_ctrl |= USB_BUF_CTRL_DATA1_PID;
  }
  ep->next_pid ^= 1u;

  if (!is_rx) {
    if (buflen) {
  // Copy data from user buffer/fifo to hw buffer
  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
      if (ep->is_xfer_fifo) {
        // not in sram, may mess up timing with E15 workaround
        tu_hwfifo_write_from_fifo(dpram_buf, ep->user_fifo, buflen, NULL);
      } else
  #endif
      {
        unaligned_memcpy(dpram_buf, ep->user_buf, buflen);
        ep->user_buf += buflen;
      }
    }

    buf_ctrl |= USB_BUF_CTRL_FULL;
  }

  // Is this the last buffer? Will trigger the trans complete irq but also stop it polling.
  // This is used to detect setup packets being sent in host mode
  if (ep->remaining_len == 0) {
    buf_ctrl |= USB_BUF_CTRL_LAST;
  }

  return buf_ctrl;
}

// Start transaction on hw buffer
void __tusb_irq_path_func(rp2usb_buffer_start)(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, bool is_rx) {
  // always compute and start with buffer 0
  uint32_t buf_ctrl = bufctrl_prepare16(ep, ep->dpram_buf, is_rx) | USB_BUF_CTRL_SEL;

  // Note: device EP0 does not have an endpoint control register
  if (ep_reg != NULL) {
    uint32_t ep_ctrl = *ep_reg;
  #if CFG_TUH_ENABLED
    const bool force_single = (rp2usb_is_host_mode() && ep->interrupt_num > 0);
  #else
    const bool force_single = false;
  #endif

    if (ep->remaining_len && !force_single) {
      // Use buffer 1 (double buffered) if there is still data
      buf_ctrl |= (uint32_t)bufctrl_prepare16(ep, ep->dpram_buf + 64, is_rx) << 16;
      ep_ctrl |= EP_CTRL_DOUBLE_BUFFERED_BITS;
    } else {
      // Only buf0 used: clear DOUBLE_BUFFERED so controller doesn't toggle buffer selector
      ep_ctrl &= ~(uint32_t)EP_CTRL_DOUBLE_BUFFERED_BITS;
    }
    *ep_reg = ep_ctrl;
  }

  // Finally, write to buffer control which will trigger the transfer the next time the controller polls this endpoint
  bufctrl_write32(buf_reg, buf_ctrl);
}

void rp2usb_xfer_start(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t *buffer, tu_fifo_t *ff,
                       uint16_t total_len) {
  (void)ff;
  hw_endpoint_lock_update(ep, 1);

  if (ep->state == EPSTATE_ACTIVE) {
    TU_LOG(1, "WARN: starting new transfer on already active ep %02X\r\n", ep->ep_addr);
    rp2usb_reset_transfer(ep);
  }

  // Fill in info now that we're kicking off the hw
  ep->remaining_len = total_len;
  ep->xferred_len   = 0;
  ep->state         = EPSTATE_ACTIVE;

  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
  if (ff != NULL) {
    ep->user_fifo    = ff;
    ep->is_xfer_fifo = true;
  } else
  #endif
  {
    ep->user_buf = buffer;
  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    ep->is_xfer_fifo = false;
  #endif
  }

  const bool is_host = rp2usb_is_host_mode();
  const bool is_rx   = (is_host == (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN));

  #if CFG_TUD_ENABLED
  if (!is_host && ep->future_len > 0) {
    // Device only: previous short-packet abort saved data from the other buffer
    const uint8_t future_len = ep->future_len;
    memcpy(ep->user_buf, ep->dpram_buf + (ep->future_bufid << 6), future_len);
    ep->xferred_len += future_len;
    ep->remaining_len -= future_len;
    ep->user_buf += future_len;
    ep->future_len   = 0;
    ep->future_bufid = 0;

    if (ep->remaining_len == 0) {
      const uint16_t xferred_len = ep->xferred_len;
      rp2usb_reset_transfer(ep);
      dcd_event_xfer_complete(0, ep->ep_addr, xferred_len, XFER_RESULT_SUCCESS, false);
      hw_endpoint_lock_update(ep, -1);
      return;
    }
  }

    #if CFG_TUSB_RP2_ERRATA_E15
  if (ep->e15_bulk_in) {
    usb_hw_set->inte = USB_INTS_DEV_SOF_BITS;

    // skip transfer if we are in critical frame period
    if (e15_is_critical_frame_period()) {
      ep->state = EPSTATE_PENDING;
      hw_endpoint_lock_update(ep, -1);
      return;
    }
  }
    #endif // CFG_TUSB_RP2_ERRATA_E15
  #endif   // CFG_TUD_ENABLED

  rp2usb_buffer_start(ep, ep_reg, buf_reg, is_rx);
  hw_endpoint_lock_update(ep, -1);
}

// sync endpoint buffer and return transferred bytes
static uint16_t __tusb_irq_path_func(bufctrl_sync16)(hw_endpoint_t *ep, bool is_rx, uint16_t buf_ctrl,
                                                     uint8_t *dpram_buf) {
  const uint16_t xferred_bytes = buf_ctrl & USB_BUF_CTRL_LEN_MASK;

  if (!is_rx) {
    // We are continuing a transfer here. If we are TX, we have successfully
    // sent some data can increase the length we have sent
    assert(!(buf_ctrl & USB_BUF_CTRL_FULL));
  } else {
    // If we have received some data, so can increase the length
    // we have received AFTER we have copied it to the user buffer at the appropriate offset
    assert(buf_ctrl & USB_BUF_CTRL_FULL);
  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    if (ep->is_xfer_fifo) {
      // not in sram, may mess up timing with E15 workaround
      tu_hwfifo_read_to_fifo(dpram_buf, ep->user_fifo, xferred_bytes, NULL);
    } else
  #endif
    {
      unaligned_memcpy(ep->user_buf, dpram_buf, xferred_bytes);
      ep->user_buf += xferred_bytes;
    }
  }
  ep->xferred_len += xferred_bytes;

  // Short packet
  if (xferred_bytes < ep->max_packet_size) {
    // Reduce total length as this is last packet
    ep->remaining_len = 0;
  }

  return xferred_bytes;
}

// Returns true if transfer is complete.
// buf_id: which buffer completed (from BUFF_CPU_SHOULD_HANDLE, only used for double-buffered).
bool __tusb_irq_path_func(rp2usb_xfer_continue)(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t buf_id,
                                                bool is_rx) {
  hw_endpoint_lock_update(ep, 1);

  if (ep->state == EPSTATE_IDLE) {
    // probably land here due to short packet on rx with double buffered
    hw_endpoint_lock_update(ep, -1);
    return false;
  }

  const bool is_host   = rp2usb_is_host_mode();
  const bool is_double = (ep_reg != NULL && ((*ep_reg) & EP_CTRL_DOUBLE_BUFFERED_BITS));

  // Double-buffered: buf_id from BUFF_CPU_SHOULD_HANDLE indicates which buffer completed.
  // RP2040-E4 (host only): in single-buffered multi-packet transfers, the controller may write completion status to
  // BUF1 half instead of BUF0. The side effect is that controller can execute an extra packet after writing to BUF1
  // since it leaves BUF0 intact, which can be polled before buf_status interrupt is triggered.
  uint8_t *dpram_buf = ep->dpram_buf;
  if (buf_id) {
  #if CFG_TUSB_RP2_ERRATA_E4
    if (!(is_host && !is_double)) // E4 bug: incorrect buf_id, buffer data is still buf0
  #endif
    {
      dpram_buf += 64; // buf1 offset
    }
  }

  io_rw_16 *buf_reg16  = (io_rw_16 *)buf_reg;
  uint16_t  buf_ctrl16 = *(buf_reg16 + buf_id);

  const uint16_t xact_bytes = bufctrl_sync16(ep, is_rx, buf_ctrl16, dpram_buf);
  const bool     is_last    = buf_ctrl16 & USB_BUF_CTRL_LAST;
  const bool     is_short   = xact_bytes < ep->max_packet_size;
  const bool     is_done    = is_short || is_last;

  // Short packet on rx with double buffer: abort the other half (if not last) and reset the buffer control.
  // The other buffer may be: (a) still AVAIL, (b) in-progress (controller receiving), or (c) already completed.
  // We must abort to safely reclaim it. If it has valid data (FULL), save as future for the next transfer.
  // Note: Host mode current does not save next transfer data due to shared epx --> potential issue. However, RP2040-E4
  // causes more or less of the same issue since it write to buf1 and next time it continues to transfer on buf0 (stale)
  if (is_short && is_double && is_rx && !is_last) {
    const uint32_t abort_bit = TU_BIT(tu_edpt_number(ep->ep_addr) << 1); // abort is device only -> IN endpoint

    if (is_host) {
      // host stop current transfer, not safe, can be racing
      const uint32_t sie_ctrl = (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK) | USB_SIE_CTRL_STOP_TRANS_BITS;
      usb_hw->sie_ctrl = sie_ctrl;
      while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}
    } else {
      // device abort current transfer
  #if CFG_TUSB_RP2_ERRATA_E2
      if (rp2040_chipversion >= 2)
  #endif
      {
        usb_hw_set->abort = abort_bit;
        while ((usb_hw->abort_done & abort_bit) != abort_bit) {}
      }
    }

    // After abort, check if the other buffer received valid data
    io_rw_16      *buf_reg16_other  = buf_reg16 + (buf_id ^ 1);
    const uint16_t buf_ctrl16_other = *buf_reg16_other;
    if (buf_ctrl16_other & USB_BUF_CTRL_FULL) {
      // Data already sent into this buffer. Save it for the next transfer.
      // buff_status will be clear by the next run
  #if CFG_TUD_ENABLED
      if (!is_host) {
        ep->future_len   = (uint8_t)(buf_ctrl16_other & USB_BUF_CTRL_LEN_MASK);
        ep->future_bufid = buf_id ^ 1;
      }
  #endif
    } else {
      ep->next_pid ^= 1u; // roll back pid if aborted
    }

    *buf_reg = 0;         // reset buffer control

    if (!is_host) {
  #if CFG_TUSB_RP2_ERRATA_E2
      if (rp2040_chipversion >= 2)
  #endif
      {
        usb_hw_clear->abort_done = abort_bit;
        usb_hw_clear->abort      = abort_bit;
      }
    }

    hw_endpoint_lock_update(ep, -1);
    return true;
  }

  if (!is_done && ep->remaining_len > 0) {
  #if CFG_TUSB_RP2_ERRATA_E15
    const bool need_e15 = ep->e15_bulk_in;
    if (need_e15 && e15_is_critical_frame_period()) {
      // mark as pending if matches E15 condition
      ep->state = EPSTATE_PENDING;
    } else if (need_e15 && ep->state == EPSTATE_PENDING) {
      // if already pending, meaning the other buf completes first, don't arm buffer, let SOF handle it
      // do nothing
    } else
  #endif
    {
      // ping-pong: arm the completed buffer with new data
      const uint16_t buf_ctrl16_new = bufctrl_prepare16(ep, dpram_buf, is_rx);
      bufctrl_write16(buf_reg16 + buf_id, buf_ctrl16_new);
    }
  }

  hw_endpoint_lock_update(ep, -1);
  return is_done;
}

//--------------------------------------------------------------------+
// Errata 15
//--------------------------------------------------------------------+

  #if CFG_TUSB_RP2_ERRATA_E15
// E15 is fixed with RP2350

/* Don't mark IN buffers as available during the last 200us of a full-speed
   frame. This avoids a situation seen with the USB2.0 hub on a Raspberry
   Pi 4 where a late IN token before the next full-speed SOF can cause port
   babble and a corrupt ACK packet. The nature of the data corruption has a
   chance to cause device lockup.

   Use the next SOF to mark delayed buffers as available. This reduces
   available Bulk IN bandwidth by approximately 20%, and requires that the
   SOF interrupt is enabled while these transfers are ongoing.

   Inherit the top-level enable from the corresponding Pico-SDK flag.
   Applications that will not use the device in a situation where it could
   be plugged into a Pi 4 or Pi 400 (for example, when directly connected
   to a commodity hub or other host) can turn off the flag in the SDK.
*/

volatile uint32_t e15_last_sof = 0;

// check if it is currently in critical frame period i.e 20% of last usb frame
static bool __tusb_irq_path_func(e15_is_critical_frame_period)(void) {
  /* Avoid the last 200us (uframe 6.5-7) of a frame, up to the EOF2 point.
   * The device state machine cannot recover from receiving an incorrect PID
   * when it is expecting an ACK. */
  uint32_t delta = time_us_32() - e15_last_sof;
  if (delta < 800 || delta > 998) {
    return false;
  }
  // TU_LOG(3, "Avoiding sof %lu now %lu last %lu\r\n", (usb_hw->sof_rd + 1) & USB_SOF_RD_BITS, time_us_32(),
  // e15_last_sof);
  return true;
}

  #endif
#endif
