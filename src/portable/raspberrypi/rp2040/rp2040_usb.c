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

#if CFG_TUSB_MCU == OPT_MCU_RP2040

#include <stdlib.h>
#include "rp2040_usb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPE
//--------------------------------------------------------------------+
  #if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
static bool e15_is_critical_frame_period(struct hw_endpoint *ep);
  #else
    #define e15_is_critical_frame_period(x) (false)
  #endif

//--------------------------------------------------------------------+
// Implementation
//--------------------------------------------------------------------+
// Provide own byte by byte memcpy as not all copies are aligned
static void unaligned_memcpy(uint8_t *dst, const uint8_t *src, size_t n) {
  while (n--) {
    *dst++ = *src++;
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

  TU_LOG2_INT(sizeof(hw_endpoint_t));
}

void __tusb_irq_path_func(hw_endpoint_reset_transfer)(struct hw_endpoint* ep) {
  ep->active = false;
  ep->remaining_len = 0;
  ep->xferred_len = 0;
  ep->user_buf = 0;
  ep->is_xfer_fifo  = false;
}

void __tusb_irq_path_func(hwbuf_ctrl_update)(io_rw_32 *buf_ctrl_reg, uint32_t and_mask, uint32_t or_mask) {
  const bool is_host = rp2usb_is_host_mode();
  uint32_t   value    = 0;
  uint32_t   buf_ctrl = *buf_ctrl_reg;

  if (and_mask) {
    value = buf_ctrl & and_mask;
  }

  if (or_mask) {
    value |= or_mask;
    if (or_mask & USB_BUF_CTRL_AVAIL) {
      if (buf_ctrl & USB_BUF_CTRL_AVAIL) {
        if (is_host) {
#if defined(PICO_RP2040) && PICO_RP2040 == 1
          // RP2040-E4: host buffer selector toggles in single-buffered mode, causing status
          // to be written to BUF1 half and leaving stale AVAILABLE in BUF0 half. Clear it.
          *buf_ctrl_reg = 0;
#else
          panic("buf_ctrl @ 0x%lX already available (host)", (uintptr_t)buf_ctrl_reg);
#endif
        } else {
          panic("buf_ctrl @ 0x%lX already available", (uintptr_t)buf_ctrl_reg);
        }
      }
      *buf_ctrl_reg = value & ~USB_BUF_CTRL_AVAIL;

      // Section 4.1.2.7.1 (rp2040) / 12.7.3.7.1 (rp2350) Concurrent access: after write to buffer control,
      // wait for USB controller to see the update before setting AVAILABLE.
      // Don't need delay in host mode as host is in charge of when to start the transaction.
      if (!is_host) {
        busy_wait_at_least_cycles(12);
      }
    }
  }

  *buf_ctrl_reg = value;
}

// prepare buffer, move data if tx, return buffer control
static uint32_t __tusb_irq_path_func(hwbuf_prepare)(struct hw_endpoint *ep, uint8_t *dpram_buf, bool is_rx) {
  const uint16_t buflen = tu_min16(ep->remaining_len, ep->max_packet_size);
  ep->remaining_len -= buflen;

  uint32_t buf_ctrl = buflen | USB_BUF_CTRL_AVAIL;
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

  // Is this the last buffer? Only really matters for host mode. Will trigger
  // the trans complete irq but also stop it polling. We only really care about
  // trans complete for setup packets being sent
  if (ep->remaining_len == 0) {
    buf_ctrl |= USB_BUF_CTRL_LAST;
  }

  return buf_ctrl;
}

// Start transaction on hw buffer
void __tusb_irq_path_func(hw_endpoint_buffer_xact)(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg) {
  const tusb_dir_t dir = tu_edpt_dir(ep->ep_addr);
  const bool       is_host = rp2usb_is_host_mode();

  bool is_rx;
  if (is_host) {
    is_rx = (dir == TUSB_DIR_IN);
  } else {
    is_rx = (dir == TUSB_DIR_OUT);
  }

  // In case short packet on buf0 in double-buffered RX, buf1 may already contain data from the
  // NEXT transfer (host sent it before CPU processed this IRQ). Cannot safely recover. Avoid by not using double
  // buffering for rx transfer

  // RP2040-E4 (host only): in single-buffered multi-packet transfers, the controller may write completion status to
  //   BUF1 half instead of BUF0. The side effect that controller can execute an extra packet after writing to BUF1
  //   since it leave BUF0 intact, which can be polled before buf_status interrupt is trigger.
  // Workaround for the side effect, we will enable double-buffered for rx but only prepare 1 buf at a time.
  #if CFG_TUSB_RP2040_ERRATA_E4_FIX

  #endif

  // always compute and start with buffer 0
  uint32_t buf_ctrl = hwbuf_prepare(ep, ep->dpram_buf, is_rx) | USB_BUF_CTRL_SEL;

  // Device mode EP0 has no endpoint control register
  if (ep_reg != NULL) {
    // Each buffer completion triggers its own IRQ.
    // If both complete simultaneously, buf_status re-sets on next clock (datasheet Table 406).
    uint32_t ep_ctrl = *ep_reg | EP_CTRL_INTERRUPT_PER_BUFFER;
#if 1
    const bool force_single = (!is_host && is_rx) || (is_host && tu_edpt_number(ep->ep_addr) != 0);
#else
    bool force_single = false; // is_rx;
    #if CFG_TUH_ENABLED
    if (is_host && ep->interrupt_num != 0) {
      force_single = true;
    }
    #endif
#endif

    if (ep->remaining_len && !force_single) {
      // Use buffer 1 (double buffered) if there is still data
      buf_ctrl |= hwbuf_prepare(ep, ep->dpram_buf+64, is_rx) << 16;
      ep_ctrl |= EP_CTRL_DOUBLE_BUFFERED_BITS;
    } else {
      // Single buffered since 1 is enough
      ep_ctrl &= ~EP_CTRL_DOUBLE_BUFFERED_BITS;
    }

    *ep_reg = ep_ctrl;
  }

  // TU_LOG(1, "xact: buf_ctrl = 0x%08lx\r\n", buf_ctrl);

  // Finally, write to buffer_control which will trigger the transfer the next time the controller polls this endpoint
  hwbuf_ctrl_set(buf_reg, buf_ctrl);
}

void hw_endpoint_xfer_start(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t *buffer, tu_fifo_t *ff,
                            uint16_t total_len) {
  (void) ff;
  hw_endpoint_lock_update(ep, 1);

  if (ep->active) {
    // TODO: Is this acceptable for interrupt packets?
    TU_LOG(1, "WARN: starting new transfer on already active ep %02X\r\n", ep->ep_addr);
    hw_endpoint_reset_transfer(ep);
  }

  // Fill in info now that we're kicking off the hw
  ep->remaining_len = total_len;
  ep->xferred_len = 0;
  ep->active = true;

#if CFG_TUD_EDPT_DEDICATED_HWFIFO
  if (ff != NULL) {
    ep->user_fifo    = ff;
    ep->is_xfer_fifo = true;
  } else
#endif
  {
    ep->user_buf     = buffer;
    ep->is_xfer_fifo = false;
  }

  #if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
  if (ep->e15_bulk_in) {
    usb_hw_set->inte = USB_INTS_DEV_SOF_BITS;
  }

  if (e15_is_critical_frame_period(ep)) {
    ep->pending = 1; // skip transfer if we are in critical frame period
  } else
  #endif
  {
    hw_endpoint_buffer_xact(ep, ep_reg, buf_reg);
  }

  hw_endpoint_lock_update(ep, -1);
}

// sync endpoint buffer and return transferred bytes
static uint16_t __tusb_irq_path_func(hwbuf_sync)(hw_endpoint_t *ep, bool is_rx, uint32_t buf_ctrl, uint8_t *dpram_buf) {
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
bool __tusb_irq_path_func(hw_endpoint_xfer_continue)(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t buf_id) {
  hw_endpoint_lock_update(ep, 1);

  if (!ep->active) {
    panic("Can't continue xfer on inactive ep %02X", ep->ep_addr);
  }

  const tusb_dir_t dir   = tu_edpt_dir(ep->ep_addr);
  const bool is_host     = rp2usb_is_host_mode();
  const bool is_rx       = is_host ? (dir == TUSB_DIR_IN) : (dir == TUSB_DIR_OUT);
  const bool is_double   = ep_reg != NULL && ((*ep_reg) & EP_CTRL_DOUBLE_BUFFERED_BITS);

  #if CFG_TUSB_RP2040_ERRATA_E4_FIX
  const bool need_e4_fix = (is_host && !is_double);
  #endif

  // Double-buffered: buf_id from BUFF_CPU_SHOULD_HANDLE indicates which buffer completed.

  // RP2040-E4 (host only): in single-buffered multi-packet transfers, the controller may write completion status to
  //   BUF1 half instead of BUF0. The side effect that controller can execute an extra packet after writing to BUF1
  //   since it leave BUF0 intact, which can be poll before buf_status interrupt is trigger.
  // Workaround for the side effect, we will enable double-buffered for rx but only prepare 1 buf at a time.
  uint32_t buf_ctrl = *buf_reg;
  // TU_LOG(1, "sync: buf_ctrl = 0x%08lx, buf id = %u\r\n", buf_ctrl, buf_id);

  uint8_t* dpram_buf = ep->dpram_buf;
  if (buf_id) {
    buf_ctrl = buf_ctrl >> 16;
  #if CFG_TUSB_RP2040_ERRATA_E4_FIX
    if (!need_e4_fix) // incorrect buf_id, buffer pointer is still buf0
  #endif
    {
      dpram_buf += 64; // buf1 offset
    }
  }

  hwbuf_sync(ep, is_rx, buf_ctrl, dpram_buf);
  const bool is_done = (ep->remaining_len == 0);

  if (is_double) {
    if (buf_id == 0) {
      // buf0 done: wait for buf1, don't start new buffers
      hw_endpoint_lock_update(ep, -1);
      return false;
    }
    // buf1 done: is_done determined by remaining_len above
  }

  if (!is_done) {
  #if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
    if (e15_is_critical_frame_period(ep)) {
      ep->pending = 1;
    } else
  #endif
    {
      hw_endpoint_buffer_xact(ep, ep_reg, buf_reg);
    }
  }

  hw_endpoint_lock_update(ep, -1);
  return is_done;
}

//--------------------------------------------------------------------+
// Errata 15
//--------------------------------------------------------------------+

#if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
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

// check if we need to apply Errata 15 workaround : i.e
// Endpoint is BULK IN and is currently in critical frame period i.e 20% of last usb frame
static bool __tusb_irq_path_func(e15_is_critical_frame_period)(struct hw_endpoint* ep) {
  if (!ep->e15_bulk_in) {
    return false;
  }

  /* Avoid the last 200us (uframe 6.5-7) of a frame, up to the EOF2 point.
   * The device state machine cannot recover from receiving an incorrect PID
   * when it is expecting an ACK.
   */
  uint32_t delta = time_us_32() - e15_last_sof;
  if (delta < 800 || delta > 998) {
    return false;
  }
  TU_LOG(3, "Avoiding sof %lu now %lu last %lu\r\n", (usb_hw->sof_rd + 1) & USB_SOF_RD_BITS, time_us_32(),
         e15_last_sof);
  return true;
}

#endif // TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
#endif
