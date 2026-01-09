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
static void sync_xfer(hw_endpoint_t *ep);

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

void tu_hwfifo_write(volatile void *hwfifo, const uint8_t *src, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void)access_mode;
  unaligned_memcpy((uint8_t *)(uintptr_t)hwfifo, src, len);
}

void tu_hwfifo_read(const volatile void *hwfifo, uint8_t *dest, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void)access_mode;
  unaligned_memcpy(dest, (const uint8_t *)(uintptr_t)hwfifo, len);
}

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
        panic("buf_ctrl @ 0x%lX already available", (uintptr_t)buf_ctrl_reg);
      }
      *buf_ctrl_reg = value & ~USB_BUF_CTRL_AVAIL;

      // Section 4.1.2.7.1 (rp2040) / 12.7.3.7.1 (rp2350) Concurrent access:  after write to buffer control, we need to
      // wait at least 1/48 mhz (usb clock), 12 cycles should be good for 48*12Mhz = 576Mhz.
      // Don't need delay in host mode as host is in charge
      if (!is_host) {
        busy_wait_at_least_cycles(12);
      }
    }
  }

  *buf_ctrl_reg = value;
}

// prepare buffer, move data if tx, return buffer control
static uint32_t __tusb_irq_path_func(prepare_ep_buffer)(struct hw_endpoint *ep, uint8_t buf_id, bool is_rx) {
  const uint16_t buflen = tu_min16(ep->remaining_len, ep->wMaxPacketSize);
  ep->remaining_len = (uint16_t) (ep->remaining_len - buflen);

  uint32_t buf_ctrl = buflen | USB_BUF_CTRL_AVAIL;

  // PID
  buf_ctrl |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
  ep->next_pid ^= 1u;

  if (!is_rx) {
    if (buflen) {
      // Copy data from user buffer/fifo to hw buffer
      uint8_t *hw_buf = ep->hw_data_buf + buf_id * 64;
      if (ep->is_xfer_fifo) {
        // not in sram, may mess up timing with E15 workaround
        tu_hwfifo_write_from_fifo(hw_buf, ep->user_fifo, buflen, NULL);
      } else {
        unaligned_memcpy(hw_buf, ep->user_buf, buflen);
        ep->user_buf += buflen;
      }
    }

    // Mark as full
    buf_ctrl |= USB_BUF_CTRL_FULL;
  }

  // Is this the last buffer? Only really matters for host mode. Will trigger
  // the trans complete irq but also stop it polling. We only really care about
  // trans complete for setup packets being sent
  if (ep->remaining_len == 0) {
    buf_ctrl |= USB_BUF_CTRL_LAST;
  }

  if (buf_id) {
    buf_ctrl = buf_ctrl << 16;
  }

  return buf_ctrl;
}

// Prepare buffer control register value
void __tusb_irq_path_func(hw_endpoint_start_next_buffer)(struct hw_endpoint* ep) {
  const tusb_dir_t dir = tu_edpt_dir(ep->ep_addr);
  bool      is_rx;
  bool      is_host = false;
  io_rw_32 *ep_ctrl_reg;
  io_rw_32 *buf_ctrl_reg;

  #if CFG_TUH_ENABLED
  is_host = rp2usb_is_host_mode();
  if (is_host) {
    buf_ctrl_reg = hwbuf_ctrl_reg_host(ep);
    ep_ctrl_reg  = hwep_ctrl_reg_host(ep);
    is_rx        = (dir == TUSB_DIR_IN);
  } else
  #endif
  {
    buf_ctrl_reg = hwbuf_ctrl_reg_device(ep);
    ep_ctrl_reg  = hwep_ctrl_reg_device(ep);
    is_rx        = (dir == TUSB_DIR_OUT);
  }

  // always compute and start with buffer 0
  uint32_t buf_ctrl = prepare_ep_buffer(ep, 0, is_rx) | USB_BUF_CTRL_SEL;

  // EP0 has no endpoint control register, also usbd only schedule 1 packet at a time (single buffer)
  if (ep_ctrl_reg != NULL) {
    uint32_t ep_ctrl = *ep_ctrl_reg;

    // For now: skip double buffered for RX e.g OUT endpoint in Device mode, since host could send < 64 bytes and cause
    // short packet on buffer0
    // NOTE: this could happen to Host mode IN endpoint Also, Host mode "interrupt" endpoint hardware is only single
    // buffered,
    // NOTE2: Currently Host bulk is implemented using "interrupt" endpoint
    const bool force_single = (!is_host && is_rx) || (is_host && tu_edpt_number(ep->ep_addr) != 0);

    if (ep->remaining_len && !force_single) {
      // Use buffer 1 (double buffered) if there is still data
      // TODO: Isochronous for buffer1 bit-field is different than CBI (control bulk, interrupt)

      buf_ctrl |= prepare_ep_buffer(ep, 1, is_rx);

      // Set endpoint control double buffered bit if needed
      ep_ctrl &= ~EP_CTRL_INTERRUPT_PER_BUFFER;
      ep_ctrl |= EP_CTRL_DOUBLE_BUFFERED_BITS | EP_CTRL_INTERRUPT_PER_DOUBLE_BUFFER;
    } else {
      // Single buffered since 1 is enough
      ep_ctrl &= ~(EP_CTRL_DOUBLE_BUFFERED_BITS | EP_CTRL_INTERRUPT_PER_DOUBLE_BUFFER);
      ep_ctrl |= EP_CTRL_INTERRUPT_PER_BUFFER;
    }

    *ep_ctrl_reg = ep_ctrl;
  }

  TU_LOG(3, "  Prepare BufCtrl: [0] = 0x%04x  [1] = 0x%04x\r\n", tu_u32_low16(buf_ctrl), tu_u32_high16(buf_ctrl));

  // Finally, write to buffer_control which will trigger the transfer
  // the next time the controller polls this dpram address
  hwbuf_ctrl_set(buf_ctrl_reg, buf_ctrl);
}

void hw_endpoint_xfer_start(struct hw_endpoint *ep, uint8_t *buffer, tu_fifo_t *ff, uint16_t total_len) {
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

  if (ff != NULL) {
    ep->user_fifo    = ff;
    ep->is_xfer_fifo = true;
  } else {
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
    hw_endpoint_start_next_buffer(ep);
  }

  hw_endpoint_lock_update(ep, -1);
}

// sync endpoint buffer and return transferred bytes
static uint16_t __tusb_irq_path_func(sync_ep_buffer)(hw_endpoint_t *ep, io_rw_32 *buf_ctrl_reg, uint8_t buf_id,
                                                     bool is_rx) {
  uint32_t buf_ctrl = *buf_ctrl_reg;
  if (buf_id) {
    buf_ctrl = buf_ctrl >> 16;
  }

  const uint16_t xferred_bytes = buf_ctrl & USB_BUF_CTRL_LEN_MASK;

  if (!is_rx) {
    // We are continuing a transfer here. If we are TX, we have successfully
    // sent some data can increase the length we have sent
    assert(!(buf_ctrl & USB_BUF_CTRL_FULL));
  } else {
    // If we have received some data, so can increase the length
    // we have received AFTER we have copied it to the user buffer at the appropriate offset
    assert(buf_ctrl & USB_BUF_CTRL_FULL);

    uint8_t *hw_buf = ep->hw_data_buf + buf_id * 64;
    if (ep->is_xfer_fifo) {
      // not in sram, may mess up timing with E15 workaround
      tu_hwfifo_read_to_fifo(hw_buf, ep->user_fifo, xferred_bytes, NULL);
    } else {
      unaligned_memcpy(ep->user_buf, hw_buf, xferred_bytes);
      ep->user_buf += xferred_bytes;
    }
  }
  ep->xferred_len += xferred_bytes;

  // Short packet
  if (xferred_bytes < ep->wMaxPacketSize) {
    // Reduce total length as this is last packet
    ep->remaining_len = 0;
  }

  return xferred_bytes;
}

// Update hw endpoint struct with info from hardware after a buff status interrupt
static void __tusb_irq_path_func(sync_xfer)(hw_endpoint_t *ep) {
  // const uint8_t    ep_num  = tu_edpt_number(ep->ep_addr);
  const tusb_dir_t dir     = tu_edpt_dir(ep->ep_addr);

  io_rw_32 *buf_ctrl_reg;
  io_rw_32 *ep_ctrl_reg;
  bool      is_rx;

  #if CFG_TUH_ENABLED
  const bool is_host = rp2usb_is_host_mode();
  if (is_host) {
    buf_ctrl_reg = hwbuf_ctrl_reg_host(ep);
    ep_ctrl_reg  = hwep_ctrl_reg_host(ep);
    is_rx        = (dir == TUSB_DIR_IN);
  } else
  #endif
  {
    buf_ctrl_reg = hwbuf_ctrl_reg_device(ep);
    ep_ctrl_reg  = hwep_ctrl_reg_device(ep);
    is_rx        = (dir == TUSB_DIR_OUT);
  }

  TU_LOG(3, "  Sync BufCtrl: [0] = 0x%04x  [1] = 0x%04x\r\n", tu_u32_low16(*buf_ctrl_reg),
         tu_u32_high16(*buf_ctrl_reg));
  uint16_t buf0_bytes = sync_ep_buffer(ep, buf_ctrl_reg, 0, is_rx); // always sync buffer 0

  // sync buffer 1 if double buffered
  if (ep_ctrl_reg != NULL && (*ep_ctrl_reg) & EP_CTRL_DOUBLE_BUFFERED_BITS) {
    if (buf0_bytes == ep->wMaxPacketSize) {
      // sync buffer 1 if not short packet
      sync_ep_buffer(ep, buf_ctrl_reg, 1, is_rx);
    } else {
      // short packet on buffer 0
      // TODO couldn't figure out how to handle this case which happen with net_lwip_webserver example
      // At this time (currently trigger per 2 buffer), the buffer1 is probably filled with data from
      // the next transfer (not current one). For now we disable double buffered for device OUT
      // NOTE this could happen to Host IN
#if 0
      uint8_t const ep_num = tu_edpt_number(ep->ep_addr);
      uint8_t const dir =  (uint8_t) tu_edpt_dir(ep->ep_addr);
      uint8_t const ep_id = 2*ep_num + (dir ? 0 : 1);

      // abort queued transfer on buffer 1
      usb_hw->abort |= TU_BIT(ep_id);

      while ( !(usb_hw->abort_done & TU_BIT(ep_id)) ) {}

      uint32_t ep_ctrl = *ep->endpoint_control;
      ep_ctrl &= ~(EP_CTRL_DOUBLE_BUFFERED_BITS | EP_CTRL_INTERRUPT_PER_DOUBLE_BUFFER);
      ep_ctrl |= EP_CTRL_INTERRUPT_PER_BUFFER;

      io_rw_32 *buf_ctrl_reg = is_host ? hwbuf_ctrl_reg_host(ep) : hwbuf_ctrl_reg_device(ep);
      hwbuf_ctrl_set(buf_ctrl_reg, 0);

      usb_hw->abort &= ~TU_BIT(ep_id);

      TU_LOG(3, "----SHORT PACKET buffer0 on EP %02X:\r\n", ep->ep_addr);
      TU_LOG(3, "  BufCtrl: [0] = 0x%04x  [1] = 0x%04x\r\n", tu_u32_low16(buf_ctrl), tu_u32_high16(buf_ctrl));
#endif
    }
  }
}

// Returns true if transfer is complete
bool __tusb_irq_path_func(hw_endpoint_xfer_continue)(struct hw_endpoint* ep) {
  hw_endpoint_lock_update(ep, 1);

  // Part way through a transfer
  if (!ep->active) {
    panic("Can't continue xfer on inactive ep %02X", ep->ep_addr);
  }

  sync_xfer(ep); // Update EP struct from hardware state

  // Now we have synced our state with the hardware. Is there more data to transfer?
  // If we are done then notify tinyusb
  if (ep->remaining_len == 0) {
    pico_trace("Completed transfer of %d bytes on ep %02X\r\n", ep->xferred_len, ep->ep_addr);
    // Notify caller we are done so it can notify the tinyusb stack
    hw_endpoint_lock_update(ep, -1);
    return true;
  } else {
  #if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
    if (e15_is_critical_frame_period(ep)) {
      ep->pending = 1;
    } else
  #endif
    {
      hw_endpoint_start_next_buffer(ep);
    }
  }

  hw_endpoint_lock_update(ep, -1);
  // More work to do
  return false;
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
