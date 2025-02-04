/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021 Ha Thach (tinyusb.org) for Double Buffered
 * Copyright (c) 2024-2025 rppicomidi for EPx-only operation (fix Data Sequence error chip bug)
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

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && !CFG_TUH_RPI_PIO_USB && !CFG_TUH_MAX3421

#include "pico.h"
#include "rp2040_usb.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"

// port 0 is native USB port, other is counted as software PIO
#define RHPORT_NATIVE 0

//--------------------------------------------------------------------+
// Low level rp2040 controller functions
//--------------------------------------------------------------------+

#ifndef PICO_USB_HOST_INTERRUPT_ENDPOINTS
#define PICO_USB_HOST_INTERRUPT_ENDPOINTS (USB_MAX_ENDPOINTS - 1)
#endif
static_assert(PICO_USB_HOST_INTERRUPT_ENDPOINTS <= USB_MAX_ENDPOINTS, "");

#define EP_POOL_NELEMENTS (1 + PICO_USB_HOST_INTERRUPT_ENDPOINTS)
static struct hw_endpoint ep_pool[EP_POOL_NELEMENTS];
#define epx (ep_pool[0])

// If the root hub is connected, this device defines
// a variable pre such that pre is true if the device
// speed does not match the root hub speed (a low speed
// device on a full speed hub). If the root hub is disconnected,
// then this macro causes the current function to return.
// The macro replaces the need_pre() function from the previous
// version of the code that panics if the root hub is disconnected.
#define COMPUTE_PRE(daddr) \
  uint8_t speed = dev_speed(); \
  tusb_speed_t speed_val; \
  switch(speed) \
  { \
    case 1: \
      speed_val = TUSB_SPEED_LOW; \
      break; \
    case 2: \
      speed_val = TUSB_SPEED_FULL; \
      break; \
    default: \
      return; /* device disconnected*/ \
  } \
  bool pre = speed_val != tuh_speed_get(daddr)

// The driver keeps track of pending transfers in simple arrays
// The scheduler chooses which transfer next gets access to the EPx hardware
// based on the position in the FIFO, what % of the bus bandwidth has
// been used by control transfers, and whether scheduled transfers are
// higher priority.
typedef struct
{
  volatile uint8_t pending[EP_POOL_NELEMENTS]; // pending[i] is an index into the ep_pool of pending transfers
  volatile uint8_t n_pending; // number of pending items
  volatile int8_t idx; // the index into the pending list of the last item checked for pending
} pending_host_transfer_t;

typedef struct
{
  volatile pending_host_transfer_t* pht;
  volatile int8_t idx;
} active_host_transfert_t;

typedef struct
{
  uint8_t setup[8];
  uint8_t* user_buffer;
  uint16_t total_len;
  uint8_t daddr; // the device address
  uint8_t edpt; // the endpoint number (0 or 0x80)
  uint16_t wMaxPacketSize;
  volatile bool setup_pending;  // true if this is a pending transfer
  volatile bool data_pending;
} _control_xfer_t;

static _control_xfer_t control_xfers[CFG_TUH_DEVICE_MAX+2]; // one for each device + the hub + dev adr 0
static pending_host_transfer_t interrupt_xfers;
static pending_host_transfer_t bulk_xfers;
static active_host_transfert_t active_xfer = {NULL, -1};

static struct hw_endpoint *_hw_endpoint_allocate(uint8_t transfer_type);
static void _hw_endpoint_init(struct hw_endpoint *ep, uint8_t dev_addr, uint8_t ep_addr, uint16_t wMaxPacketSize, uint8_t transfer_type, uint8_t bmInterval);

// The highest dev_addr is CFG_TUH_DEVICE_MAX; hub address is CFG_TUH_DEVICE_MAX+1.
// Let i be 0-15 for OUT endpoint and 16-31 for IN endpoints
// Bit i in nak_mask[dev_addr - 1] is set if endpoint with index i
// in device with dev_addr has sent NAK since the last SOF interrupt.
static uint32_t nak_mask[CFG_TUH_DEVICE_MAX+1]; // +1 for hubs

static uint32_t get_nak_mask_val(uint8_t edpt)
{
  return 1 << (tu_edpt_number(edpt) + (tu_edpt_dir(edpt)*16));
}

static bool is_nak_mask_set(uint8_t daddr, uint8_t edpt)
{
  return (nak_mask[daddr-1] & get_nak_mask_val(edpt)) != 0;
}
static bool __tusb_irq_path_func(hw_add_pending_xfer)(pending_host_transfer_t* pht, uint8_t ep_pool_idx)
{
  // make sure args are valid and there is space in the list
  if (pht == NULL || ep_pool_idx > EP_POOL_NELEMENTS || pht->n_pending >=  EP_POOL_NELEMENTS)
    return false;
  // Disable the USB Host interrupt sources
  struct hw_endpoint* ep = ep_pool+ep_pool_idx;
  hw_endpoint_lock_update(ep, 1);
  // add to the list
  pht->pending[pht->n_pending++] = ep_pool_idx;
  // Enable the USB Host interrupt
  hw_endpoint_lock_update(ep, -1);
  return true;
}

static bool __tusb_irq_path_func(hw_del_pending_xfer)(volatile pending_host_transfer_t* pht, uint8_t ep_pool_idx)
{
  struct hw_endpoint* ep = ep_pool+ep_pool_idx;
  hw_endpoint_lock_update(ep, 1);
  // make sure args are valid and there is space in the list
  assert(pht != NULL && ep_pool_idx < EP_POOL_NELEMENTS && pht->n_pending <= EP_POOL_NELEMENTS && pht->n_pending > 0);
  for (int8_t idx = 0; idx < pht->n_pending; idx++)
  {
    if (pht->pending[idx] == ep_pool_idx)
    {
      pht->pending[idx] = pht->pending[pht->n_pending - 1];
      pht->n_pending--;
      if (pht->idx >= pht->n_pending)
      {
        pht->idx = 0;
      }
      hw_endpoint_lock_update(ep, -1);
      return true;
    }
  }
  hw_endpoint_lock_update(ep, -1);
  return false;
}

// Flags we set by default in sie_ctrl (we add other bits on top)
enum {
  SIE_CTRL_BASE = USB_SIE_CTRL_SOF_EN_BITS      | USB_SIE_CTRL_KEEP_ALIVE_EN_BITS |
                  USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS
};

static struct
{
  absolute_time_t timestamp;
  uint32_t frame_num;
} frame_time_info;

int8_t get_dev_ep_idx(uint8_t dev_addr, uint8_t ep_addr)
{
  uint8_t num = tu_edpt_number(ep_addr);
  if ( num == 0 ) return 0;

  for ( int8_t idx = 1; idx < (int8_t)TU_ARRAY_SIZE(ep_pool); idx++ )
  {
    struct hw_endpoint *ep = &ep_pool[idx];
    if ( ep->configured && (ep->dev_addr == dev_addr) && (ep->ep_addr == ep_addr) )
      return idx;
  }

  return -1;
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t dev_speed(void)
{
  return (usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS) >> USB_SIE_STATUS_SPEED_LSB;
}

static void __tusb_irq_path_func(hw_xfer_complete)(struct hw_endpoint *ep, xfer_result_t xfer_result)
{
  // Mark transfer as done before we tell the tinyusb stack
  uint8_t dev_addr = ep->dev_addr;
  uint8_t ep_addr = ep->ep_addr;
  uint xferred_len = ep->xferred_len;

  hw_endpoint_reset_transfer(ep);
  hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, xfer_result, true);
}

static void __tusb_irq_path_func(_handle_buff_status_bit)(uint bit, struct hw_endpoint *ep)
{
  usb_hw_clear->buf_status = bit;
  // EP may have been stalled?
  assert(ep->active);
  bool done = hw_endpoint_xfer_continue(ep);
  if ( done )
  {
    hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
  }
}

static void __tusb_irq_path_func(hw_handle_buff_status)(void)
{
  uint32_t remaining_buffers = usb_hw->buf_status;
  pico_trace("buf_status 0x%08lx\n", remaining_buffers);

  // Check EPX first
  uint bit = 0b1;
  if ( remaining_buffers & bit )
  {
    remaining_buffers &= ~bit;
    struct hw_endpoint * ep = &epx;

    uint32_t ep_ctrl = *ep->endpoint_control;
    if ( ep_ctrl & EP_CTRL_DOUBLE_BUFFERED_BITS )
    {
      TU_LOG(3, "Double Buffered: ");
    }
    else
    {
      TU_LOG(3, "Single Buffered: ");
    }
    TU_LOG_HEX(3, ep_ctrl);

    _handle_buff_status_bit(bit, ep);
  }

  if ( remaining_buffers )
  {
    panic("Unhandled buffer %d\n", remaining_buffers);
  }
}

static void __tusb_irq_path_func(hw_trans_complete)(void)
{
  if (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS)
  {
    pico_trace("Sent setup packet\n");
    struct hw_endpoint *ep = &epx;
    assert(ep->active);
    // Set transferred length to 8 for a setup packet
    ep->xferred_len = 8;
    hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
  }
  else
  {
    // Don't care. Will handle this in buff status
    return;
  }
}

static void __tusb_irq_path_func(_hw_setup_epx_from_ep)(struct hw_endpoint* ep) {
  // Fill in endpoint control register with buffer offset
  uint dpram_offset = hw_data_offset(ep->hw_data_buf);
  // Bits 0-5 should be 0
  assert(!(dpram_offset & 0b111111));
  // set up epx to do an interrupt transfer; the polling interval does not matter
  uint32_t ep_reg = EP_CTRL_ENABLE_BITS
              | EP_CTRL_DOUBLE_BUFFERED_BITS | EP_CTRL_INTERRUPT_PER_DOUBLE_BUFFER
              | (TUSB_XFER_INTERRUPT << EP_CTRL_BUFFER_TYPE_LSB)
              | dpram_offset;
  *epx.endpoint_control = ep_reg;
  *epx.buffer_control = *ep->buffer_control;
  epx.remaining_len = ep->remaining_len;
  epx.user_buf = ep->user_buf;
  epx.wMaxPacketSize = ep->wMaxPacketSize;
  epx.hw_data_buf = ep->hw_data_buf;
  epx.next_pid = ep->next_pid;
  epx.rx = ep->rx;
  epx.xferred_len = 0;
  epx.dev_addr = ep->dev_addr;
  epx.ep_addr = ep->ep_addr;
  epx.transfer_type = ep->transfer_type;
}

void __tusb_irq_path_func(_hw_epx_xfer_start)(uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  tusb_dir_t const ep_dir = tu_edpt_dir(ep_addr);
  // Control endpoint can change direction
  if ( ep_num == 0  && ep_addr != epx.ep_addr )
  {
    // Direction has flipped on endpoint so re-init it
    _hw_endpoint_init(&epx, dev_addr, ep_addr, control_xfers[dev_addr].wMaxPacketSize, TUSB_XFER_CONTROL, 0);
  }
  hw_endpoint_xfer_start(&epx, buffer, buflen);

  // That has set up buffer control, endpoint control etc
  // for host we have to initiate the transfer
  usb_hw->dev_addr_ctrl = (uint32_t) (dev_addr | (ep_num << USB_ADDR_ENDP_ENDPOINT_LSB));

  COMPUTE_PRE(dev_addr);
  uint32_t flags = USB_SIE_CTRL_START_TRANS_BITS | SIE_CTRL_BASE |
                     (ep_dir ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS) |
                     (pre ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);
  // START_TRANS bit on SIE_CTRL seems to exhibit the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = flags & ~USB_SIE_CTRL_START_TRANS_BITS;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = flags;
  epx.active = true;
}

// Scheduling algorithm:
// Isochronous transfers are not supported. Encountering one will cause
// an assert.
//
// Both BULK and INTERRUPT transfers are handled by epx. The epx
// endpoint will trigger the irq with transfer complete with NAK
// if epx is set up for interrupt transfer and the device responds
// with NAK. The SIE_STATUS register NAK_REC bit will be set.
//
// If there are any pending control transfers, do them first.
// Control transfers operate until completion. NAK on the bus
// causes auto-retry.
//
// Every SOF interrupt, all endpoints will be polled. INTERRUPT
// endpoints that respond with NAK will not be polled again until the
// next SOF at the soonest. If there is not enough time in the frame to
// poll all endpoints, then INTERRUPT endpoints have priority over BULK
// endpoints. If there is extra time at the end of the frame, BULK
// endpoints that have responded with NAK will be polled again until
// there is not enough time left in the frame.
//
// If the epx endpoint is in the middle of a transfer, then scheduling
// is postponed
static void __tusb_irq_path_func(hcd_schedule_next_transfer)()
{
  if (epx.active)
    return; // The last transfer is still ongoing

  absolute_time_t now = get_absolute_time();
  // Must be at least 100us left in the 1000us frame
  if (absolute_time_diff_us(frame_time_info.timestamp, now) > 900)
    return; // not enough time left in the frame to do any more transfers
  // First check for setup transfers
  // TODO: This loop grants time starting with the lowest number device address
  for (size_t idx = 0; idx < TU_ARRAY_SIZE(control_xfers); idx++)
  {
    if (control_xfers[idx].setup_pending)
    {
      // set up epx to do a control transfer
      // Copy data into setup packet buffer
      for ( uint8_t jdx = 0; jdx < 8; jdx++ )
      {
        usbh_dpram->setup_packet[jdx] = control_xfers[idx].setup[jdx];
      }
      // Configure EP0 struct with setup info for the trans complete
      struct hw_endpoint * ep = _hw_endpoint_allocate(0);
      assert(ep);

      // EPX should be inactive
      assert(!ep->active);
      ep->wMaxPacketSize = control_xfers[idx].wMaxPacketSize;
      // EP0 out
      _hw_endpoint_init(ep, control_xfers[idx].daddr, 0x00, ep->wMaxPacketSize, 0, 0);
      assert(ep->configured);

      // Set device address
      usb_hw->dev_addr_ctrl = control_xfers[idx].daddr;
      ep->dev_addr = control_xfers[idx].daddr;
      ep->ep_addr = 0;
      // Set pre if we are a low speed device on full speed hub
      COMPUTE_PRE(ep->dev_addr);
      ep->remaining_len = 8;
      ep->active = true;
      uint32_t const flags = SIE_CTRL_BASE | USB_SIE_CTRL_SEND_SETUP_BITS | USB_SIE_CTRL_START_TRANS_BITS |
                         (pre ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);

      // START_TRANS bit on SIE_CTRL seems to exhibit the same behavior as the AVAILABLE bit
      // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
      // We write everything except the START_TRANS bit first, then wait some cycles.
      usb_hw->sie_ctrl = flags & ~USB_SIE_CTRL_START_TRANS_BITS;
      busy_wait_at_least_cycles(12);
      usb_hw->sie_ctrl = flags;
      control_xfers[idx].setup_pending = false;
      active_xfer.pht = NULL;
      return;
    }
    else if (control_xfers[idx].data_pending)
    {
      epx.dev_addr = control_xfers[idx].daddr;
      _hw_epx_xfer_start(control_xfers[idx].daddr, control_xfers[idx].edpt, control_xfers[idx].user_buffer, control_xfers[idx].total_len);
      control_xfers[idx].data_pending = false;
      active_xfer.pht = NULL;
      return;
    }
    now = get_absolute_time();
    // Must be at least 100us left in the 1000us frame
    if (absolute_time_diff_us(frame_time_info.timestamp, now) > 900)
    {
      return;
    }
  }
  // If there are any pending Interrupt transactions, start one
  // if its polling interval is exceeded
  if (interrupt_xfers.n_pending > 0)
  {
    // see if it is time to start an interrupt transfer for any in the list
    int8_t old_idx = interrupt_xfers.idx;
    do
    {
      now = get_absolute_time();
      // Must be at least 100us left in the 1000us frame
      if (absolute_time_diff_us(frame_time_info.timestamp, now) > 900)
      {
        return;
      }
      int8_t pool_idx = (int8_t)interrupt_xfers.pending[interrupt_xfers.idx];
      struct hw_endpoint* ep = ep_pool + pool_idx;
      interrupt_xfers.idx = (int8_t)((interrupt_xfers.idx+1) % interrupt_xfers.n_pending);
      if (is_nak_mask_set(ep->dev_addr, ep->ep_addr))
      {
        continue; // you only get to poll it once per frame
      }
      if (absolute_time_diff_us(ep->scheduled_time, now) > 0)
      {
        // This is the endpoint to set up
        ep->scheduled_time = delayed_by_ms(now, ep->polling_interval);
        _hw_setup_epx_from_ep(ep);
        active_xfer.pht = &interrupt_xfers;
        active_xfer.idx = pool_idx;
        _hw_epx_xfer_start(ep->dev_addr, ep->ep_addr, ep->user_buf, ep->remaining_len);
        return;
      }
    } while (interrupt_xfers.idx != old_idx);
  }

  // No control transfers pending. Check for bulk transfers
  if (bulk_xfers.n_pending > 0)
  {
    int old_idx = bulk_xfers.idx;
    do
    {
      now = get_absolute_time();
      // Must be at least 100us left in the 1000us frame
      if (absolute_time_diff_us(frame_time_info.timestamp, now) > 900)
      {
        return;
      }
      int8_t pool_idx = (int8_t)bulk_xfers.pending[bulk_xfers.idx];
      struct hw_endpoint* ep = ep_pool + pool_idx;
      bulk_xfers.idx = (int8_t)((bulk_xfers.idx+1) % bulk_xfers.n_pending);
      _hw_setup_epx_from_ep(ep);
      active_xfer.pht = &bulk_xfers;
      active_xfer.idx = pool_idx;
      // start the transfer
      _hw_epx_xfer_start(ep->dev_addr, ep->ep_addr, ep->user_buf, ep->remaining_len);
      return;
    } while (bulk_xfers.idx != old_idx);
  }
}

static void __tusb_irq_path_func(hcd_rp2040_irq)(void)
{
  uint32_t status = usb_hw->ints;
  uint32_t handled = 0;
  uint8_t current_edpt = (usb_hw->dev_addr_ctrl & USB_ADDR_ENDP_ENDPOINT_BITS) >> USB_ADDR_ENDP_ENDPOINT_LSB;
  current_edpt |= (uint8_t)((usb_hw->sie_ctrl & USB_SIE_CTRL_RECEIVE_DATA_BITS) ? 0x80 : 0);
  uint8_t current_daddr = (usb_hw->dev_addr_ctrl & USB_ADDR_ENDP_ADDRESS_BITS) >> USB_ADDR_ENDP_ADDRESS_LSB;
  bool is_control_xfer = tu_edpt_number(current_edpt) == 0;
  bool attached = true;

  if ( status & USB_INTS_HOST_CONN_DIS_BITS )
  {
    handled |= USB_INTS_HOST_CONN_DIS_BITS;

    if ( dev_speed() )
    {
      tu_memclr(nak_mask, sizeof(nak_mask));
      hcd_event_device_attach(RHPORT_NATIVE, true);
    }
    else
    {
      attached = false;
      hcd_event_device_remove(RHPORT_NATIVE, true);
    }

    // Clear speed change interrupt
    usb_hw_clear->sie_status = USB_SIE_STATUS_SPEED_BITS;
  }
  if ( status & USB_INTS_HOST_SOF_BITS )
  {
    handled |= USB_INTS_HOST_SOF_BITS;
    // Clear NAK record for this frame
    tu_memclr(nak_mask, sizeof(nak_mask));
    // Record new frame number for scheduler and clear the interrupt
    frame_time_info.frame_num = usb_hw->sof_rd;
    frame_time_info.timestamp = get_absolute_time();
  }

  if ( status & USB_INTS_STALL_BITS )
  {
    if (usb_hw->sie_status & USB_SIE_STATUS_STALL_REC_BITS)
    {
      // We have rx'd a stall from the device
      // NOTE THIS SHOULD HAVE PRIORITY OVER BUFF_STATUS
      // AND TRANS_COMPLETE as the stall is an alternative response
      // to one of those events
      pico_trace("Stall REC\n");
      handled |= USB_INTS_STALL_BITS;
      usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
      hw_xfer_complete(&epx, XFER_RESULT_STALLED);
    }
  }
  bool drop_data = false;
  if ( status & USB_INTS_ERROR_DATA_SEQ_BITS )
  {
    handled |= USB_INTS_ERROR_DATA_SEQ_BITS;
    usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
    TU_LOG(1, "  Seq Error: %08lx, [0] = 0x%04u  [1] = 0x%04x\r\n", *epx.buffer_control,
           tu_u32_low16(*epx.buffer_control),
           tu_u32_high16(*epx.buffer_control));
    // For control transfers, this is bad. For other transfers, just drop the data
    if (is_control_xfer)
      panic("Data Seq Error \n");
    else
    {
      drop_data = true;
      // put the next_pid value back where it should be
      epx.next_pid ^= 1u;
    }
  }

  if ( usb_hw->sie_status & USB_SIE_STATUS_NAK_REC_BITS )
  {
    // control transfers auto-retry on NAK
    // Bulk and Interrupt transfers do not
    if (!is_control_xfer)
    {
      drop_data = true;
      nak_mask[current_daddr-1] = get_nak_mask_val(current_edpt);
    }
    usb_hw_clear->sie_status = USB_SIE_STATUS_NAK_REC_BITS;
  }
  if (usb_hw->sie_status & USB_SIE_STATUS_RX_TIMEOUT_BITS)
  {
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;
    drop_data = true;
  }
  if ( status & USB_INTS_BUFF_STATUS_BITS )
  {
    handled |= USB_INTS_BUFF_STATUS_BITS;
    if (drop_data)
    {
      usb_hw_clear->buf_status = 0x01;
    }
    else
      hw_handle_buff_status();
  }

  if ( status & USB_INTS_TRANS_COMPLETE_BITS )
  {
    handled |= USB_INTS_TRANS_COMPLETE_BITS;
    usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;

    if (drop_data)
    {
      TU_LOG(3, "complete: buffer dropped %02x/%02x\r\n", current_daddr, current_edpt);
      hw_endpoint_reset_transfer(&epx);
      if (active_xfer.pht != NULL)
      {
        ep_pool[active_xfer.idx].active = false;
      }
    }
    else
    {
      TU_LOG(2, "\r\nTransfer complete dev=%02x ep=%02x\r\n", current_daddr, current_edpt);
      // If the transfer completed, remove if from the pending list
      if (active_xfer.pht != NULL)
      {
        *ep_pool[active_xfer.idx].buffer_control = *epx.buffer_control;
        ep_pool[active_xfer.idx].next_pid = epx.next_pid;
        hw_del_pending_xfer(active_xfer.pht, (uint8_t)active_xfer.idx);
      }
      hw_trans_complete();
    }
    active_xfer.pht = NULL;
  }

  if ( status & USB_INTS_ERROR_RX_TIMEOUT_BITS )
  {
    handled |= USB_INTS_ERROR_RX_TIMEOUT_BITS;
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;
  }

  // This is how application space forces the scheduler to run
  if ( status & USB_INTS_DEV_SOF_BITS )
  {
    handled |= USB_INTS_DEV_SOF_BITS;
    usb_hw_clear->intf = USB_INTF_DEV_SOF_BITS;
  }

  if ( status ^ handled )
  {
    panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
  }
  if (attached)
    hcd_schedule_next_transfer();
}

void __tusb_irq_path_func(hcd_int_handler)(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;
  hcd_rp2040_irq();
}

static struct hw_endpoint *_next_free_interrupt_ep(void)
{
  struct hw_endpoint * ep = NULL;
  for ( uint i = 1; i < TU_ARRAY_SIZE(ep_pool); i++ )
  {
    ep = &ep_pool[i];
    if ( !ep->configured )
    {
      // Will be configured by _hw_endpoint_init / _hw_endpoint_allocate
      ep->interrupt_num = (uint8_t) (i - 1);
      return ep;
    }
  }
  return ep;
}

static struct hw_endpoint *_hw_endpoint_allocate(uint8_t transfer_type)
{
  struct hw_endpoint * ep = NULL;

  if ( transfer_type != TUSB_XFER_CONTROL )
  {
    // Note: even though datasheet name these "Interrupt" endpoints. These are actually
    // "Asynchronous" endpoints and can be used for other type such as: Bulk  (ISO need confirmation)
    ep = _next_free_interrupt_ep();
    pico_info("Allocate %s ep %d\n", tu_edpt_type_str(transfer_type), ep->interrupt_num);
    assert(ep);
    ep->buffer_control = &usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num].ctrl;
    ep->endpoint_control = &usbh_dpram->int_ep_ctrl[ep->interrupt_num].ctrl;
    // 0 for epx (double buffered): TODO increase to 1024 for ISO
    // 2x64 for intep0
    // 3x64 for intep1
    // etc
    ep->hw_data_buf = &usbh_dpram->epx_data[64 * (ep->interrupt_num + 2)];
  }
  else
  {
    ep = &epx;
    ep->buffer_control = &usbh_dpram->epx_buf_ctrl;
    ep->endpoint_control = &usbh_dpram->epx_ctrl;
    ep->hw_data_buf = &usbh_dpram->epx_data[0];
  }

  return ep;
}

static void _hw_endpoint_init(struct hw_endpoint *ep, uint8_t dev_addr, uint8_t ep_addr, uint16_t wMaxPacketSize, uint8_t transfer_type, uint8_t bInterval)
{
  // Already has data buffer, endpoint control, and buffer control allocated at this point
  assert(ep->endpoint_control);
  assert(ep->buffer_control);
  assert(ep->hw_data_buf);

  uint8_t const num = tu_edpt_number(ep_addr);
  tusb_dir_t const dir = tu_edpt_dir(ep_addr);

  ep->ep_addr = ep_addr;
  ep->dev_addr = dev_addr;

  // For host, IN to host == RX, anything else rx == false
  ep->rx = (dir == TUSB_DIR_IN);

  // Response to a setup packet on EP0 starts with pid of 1
  ep->next_pid = (num == 0 ? 1u : 0u);
  ep->wMaxPacketSize = wMaxPacketSize;
  ep->transfer_type = transfer_type;
  if (num == 0)
    control_xfers[dev_addr].wMaxPacketSize = wMaxPacketSize;

  pico_trace("hw_endpoint_init dev %d ep %02X xfer %d\n", ep->dev_addr, ep->ep_addr, ep->transfer_type);
  pico_trace("dev %d ep %02X setup buffer @ 0x%p\n", ep->dev_addr, ep->ep_addr, ep->hw_data_buf);
  uint dpram_offset = hw_data_offset(ep->hw_data_buf);
  // Bits 0-5 should be 0
  assert(!(dpram_offset & 0b111111));

  // Fill in endpoint control register with buffer offset
  uint32_t ep_reg = EP_CTRL_ENABLE_BITS
                    | EP_CTRL_INTERRUPT_PER_BUFFER
                    | (ep->transfer_type << EP_CTRL_BUFFER_TYPE_LSB)
                    | dpram_offset;
  if ( bInterval )
  {
    ep_reg |= (uint32_t) ((bInterval - 1) << EP_CTRL_HOST_INTERRUPT_INTERVAL_LSB);
    ep->polling_interval = bInterval;
  }
  *ep->endpoint_control = ep_reg;
  pico_trace("endpoint control (0x%p) <- 0x%lx\n", ep->endpoint_control, ep_reg);
  ep->configured = true;

  if ( ep != &epx )
  {
    // Endpoint has its own addr_endp and interrupt bits to be setup!
    // This is an interrupt/async endpoint. so need to set up ADDR_ENDP register with:
    // - device address
    // - endpoint number / direction
    // - preamble
    uint32_t reg = (uint32_t) (dev_addr | (num << USB_ADDR_ENDP1_ENDPOINT_LSB));

    if ( dir == TUSB_DIR_OUT )
    {
      reg |= USB_ADDR_ENDP1_INTEP_DIR_BITS;
    }
    COMPUTE_PRE(dev_addr);
    if ( pre )
    {
      reg |= USB_ADDR_ENDP1_INTEP_PREAMBLE_BITS;
    }
    usb_hw->int_ep_addr_ctrl[ep->interrupt_num] = reg;

    // We are just setting up the registers; going to poll the endpoint ourselves,
    // so no need to enable the endpoint now.
  }
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;
  pico_trace("hcd_init %d\n", rhport);
  assert(rhport == 0);

  // Reset any previous state
  rp2040_usb_init();

  // Force VBUS detect to always present, for now we assume vbus is always provided (without using VBUS En)
  usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

  // Remove shared irq if it was previously added so as not to fill up shared irq slots
  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);

  irq_add_shared_handler(USBCTRL_IRQ, hcd_rp2040_irq, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);

  // clear epx and interrupt eps
  memset(&ep_pool, 0, sizeof(ep_pool));

  // Enable in host mode with SOF / Keep alive on
  usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS | USB_MAIN_CTRL_HOST_NDEVICE_BITS;
  usb_hw->sie_ctrl = SIE_CTRL_BASE;
  usb_hw->inte = USB_INTE_BUFF_STATUS_BITS      |
                 USB_INTE_HOST_CONN_DIS_BITS    |
                 USB_INTE_HOST_RESUME_BITS      |
                 USB_INTE_STALL_BITS            |
                 USB_INTE_TRANS_COMPLETE_BITS   |
                 USB_INTE_ERROR_RX_TIMEOUT_BITS |
                 USB_INTE_HOST_SOF_BITS         |
                 USB_INTE_DEV_SOF_BITS          |  // <= used by hcd_setup_send()
                 USB_INTE_ERROR_DATA_SEQ_BITS   ;
  tu_memclr(nak_mask, sizeof(nak_mask));
  control_xfers[0].daddr = 0;
  control_xfers[0].wMaxPacketSize = 8;
  control_xfers[0].data_pending = false;
  control_xfers[0].setup_pending = false;
  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;

  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);
  reset_block(RESETS_RESET_USBCTRL_BITS);
  unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

  return true;
}

void hcd_port_reset(uint8_t rhport)
{
  (void) rhport;
  pico_trace("hcd_port_reset\n");
  assert(rhport == 0);
  // TODO: Nothing to do here yet. Perhaps need to reset some state?
}

void hcd_port_reset_end(uint8_t rhport)
{
  (void) rhport;
}

bool hcd_port_connect_status(uint8_t rhport)
{
  (void) rhport;
  pico_trace("hcd_port_connect_status\n");
  assert(rhport == 0);
  return usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void) rhport;
  assert(rhport == 0);

  // TODO: Should enumval this register
  switch ( dev_speed() )
  {
    case 1:
      return TUSB_SPEED_LOW;
    case 2:
      return TUSB_SPEED_FULL;
    default:
      panic("Invalid speed\n");
      // return TUSB_SPEED_INVALID;
  }
}

static void close_pending_host_transfers(pending_host_transfer_t* pht, uint8_t dev_addr)
{
  for (int8_t idx = 0; idx < pht->n_pending;)
  {
    uint8_t pending_idx = pht->pending[idx];
    if (ep_pool[pending_idx].dev_addr == dev_addr)
    {
      hw_del_pending_xfer(pht, pending_idx);
    }
    else
    {
      ++idx;
    }
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
  pico_trace("hcd_device_close %d\n", dev_addr);
  (void) rhport;

  if (dev_addr == 0) return;
  uint32_t saved_irqs = save_and_disable_interrupts();
  // clean up scheduling buffers on close
  if (active_xfer.pht != NULL && ep_pool[active_xfer.idx].dev_addr == dev_addr) {
    if (epx.active)
    {
      epx.active = false;
    }
    active_xfer.pht = NULL;
  }
  close_pending_host_transfers(&interrupt_xfers, dev_addr);
  close_pending_host_transfers(&bulk_xfers, dev_addr);
  control_xfers[dev_addr].setup_pending = false;
  control_xfers[dev_addr].data_pending = false;


  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++)
  {
    hw_endpoint_t* ep = &ep_pool[i];

    if (ep->dev_addr == dev_addr && ep->configured)
    {
      // in case it is an interrupt endpoint, disable it
      usb_hw_clear->int_ep_ctrl = (1 << (ep->interrupt_num + 1));
      usb_hw->int_ep_addr_ctrl[ep->interrupt_num] = 0;

      // unconfigure the endpoint
      ep->configured = false;
      *ep->endpoint_control = 0;
      *ep->buffer_control = 0;
      hw_endpoint_reset_transfer(ep);
    }
  }
  restore_interrupts(saved_irqs);
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void) rhport;
  return frame_time_info.frame_num;
}

void hcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  assert(rhport == 0);
  irq_set_enabled(USBCTRL_IRQ, true);
}

void hcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  // todo we should check this is disabling from the correct core; note currently this is never called
  assert(rhport == 0);
  irq_set_enabled(USBCTRL_IRQ, false);
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;

  pico_trace("hcd_edpt_open dev_addr %d, ep_addr %d\n", dev_addr, ep_desc->bEndpointAddress);

  // Allocated differently based on if it's an interrupt endpoint or not
  struct hw_endpoint *ep = _hw_endpoint_allocate(ep_desc->bmAttributes.xfer);
  TU_ASSERT(ep);

  _hw_endpoint_init(ep,
                    dev_addr,
                    ep_desc->bEndpointAddress,
                    tu_edpt_packet_size(ep_desc),
                    ep_desc->bmAttributes.xfer,
                    ep_desc->bInterval);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  (void) rhport;

  pico_trace("hcd_edpt_xfer dev_addr %d, ep_addr 0x%x, len %d\n", dev_addr, ep_addr, buflen);


  uint8_t const ep_num = tu_edpt_number(ep_addr);

  // Get appropriate ep.
  int8_t ep_idx = get_dev_ep_idx(dev_addr, ep_addr);
  TU_ASSERT(ep_idx != -1);
  struct hw_endpoint *ep = ep_pool+ep_idx;

  TU_ASSERT(ep);

  if (ep_num == 0)
  {
    TU_ASSERT(dev_addr < CFG_TUH_DEVICE_MAX+2);
    TU_ASSERT(control_xfers[dev_addr].data_pending == false);
    TU_ASSERT(control_xfers[dev_addr].setup_pending == false);
    // Disable the USB Host interrupt sources
    hw_endpoint_lock_update(ep, 1);
    control_xfers[dev_addr].daddr = dev_addr;
    control_xfers[dev_addr].edpt = ep_addr;
    control_xfers[dev_addr].total_len = buflen;
    control_xfers[dev_addr].user_buffer = buffer;
    control_xfers[dev_addr].data_pending = true;
    hw_endpoint_lock_update(ep, -1);
  }
  else
  {
    struct hw_endpoint *ep = ep_pool+ep_idx;
    ep->remaining_len = buflen;
    ep->user_buf = buffer;
    TU_ASSERT(ep);
    pending_host_transfer_t* next_list;
    if (ep->polling_interval != 0)
    {
      next_list = &interrupt_xfers;
      ep->scheduled_time = delayed_by_ms(get_absolute_time(), ep->polling_interval); // now + polling interval
      TU_LOG(2,"scheduling new interrupt xfer %d:%02x/%02x\r\n", ep_idx, ep->dev_addr, ep->ep_addr);
    }
    else
    {
      next_list = &bulk_xfers;
      TU_LOG(2,"scheduling new bulk xfer %d:%02x/%02x\r\n", ep_idx, ep->dev_addr, ep->ep_addr);
    }
    // Add the transfer to the appropriate pending transfer list
    if (!hw_add_pending_xfer(next_list, (uint8_t)ep_idx))
      panic("Cannot schedule new xfer\r\n");
  }
  // next call schedule_next_transfer() will start the transfer
  // if no other higher priority transfer is pending
  // this interrupt is never used by the host, but it forces the scheduler now.
  usb_hw_set->intf = USB_INTF_DEV_SOF_BITS;
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;
  // TODO not implemented yet
  return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  (void) rhport;
  TU_ASSERT(dev_addr < CFG_TUH_DEVICE_MAX+2);
  TU_ASSERT(control_xfers[dev_addr].data_pending == false);
  TU_ASSERT(control_xfers[dev_addr].setup_pending == false);
  // Disable the USB Host interrupt sources
  hw_endpoint_lock_update(&epx, 1);
  control_xfers[dev_addr].daddr = dev_addr;
  control_xfers[dev_addr].edpt = 0;
  control_xfers[dev_addr].total_len = 8;
  for (int idx=0; idx < 8; idx++)
  {
    control_xfers[dev_addr].setup[idx] = setup_packet[idx];
  }
  control_xfers[dev_addr].setup_pending = true;
  // Enable the USB Host interrupt
  hw_endpoint_lock_update(&epx, -1);
  // next call schedule_next_transfer() will start the transfer
  // if no other higher priority control transfer is pending

  // this interrupt is never used by the host, but it forces the scheduler now.
  usb_hw_set->intf = USB_INTF_DEV_SOF_BITS;
  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  panic("hcd_clear_stall");
  // return true;
}

#endif //CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && !CFG_TUH_RPI_PIO_USB && !CFG_TUH_MAX3421
