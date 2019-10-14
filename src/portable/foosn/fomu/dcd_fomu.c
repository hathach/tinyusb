/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

// #if TUSB_OPT_DEVICE_ENABLED && (CFG_TUSB_MCU == OPT_MCU_FOMU_EPTRI)
#if 1

#include "device/dcd.h"
#include "dcd_fomu.h"
#include "csr.h"
#include "irq.h"
void fomu_error(uint32_t line);
void mputs(const char *str);
void mputln(const char *str);

//--------------------------------------------------------------------+
// SIE Command
//--------------------------------------------------------------------+

#define EP_SIZE 64

uint16_t volatile rx_buffer_offset[16];
uint8_t volatile * rx_buffer[16];
uint16_t volatile rx_buffer_max[16];

volatile uint8_t tx_ep;
volatile uint16_t tx_len;
uint8_t volatile * tx_buffer;
volatile uint16_t tx_offset;
volatile uint8_t reset_count;

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+

static void finish_tx(void) {
  // Ignore "ACK" packets where there was no data to send.
  if (!tx_buffer) {
      return;
  }

  // If the system isn't idle, then something is very wrong.
  uint8_t in_status = usb_in_status_read();
  if (!(in_status & (1 << CSR_USB_IN_STATUS_IDLE_OFFSET)))
    fomu_error(__LINE__);

  tx_offset += EP_SIZE;
  if (tx_offset >= tx_len) {
    dcd_event_xfer_complete(0, tu_edpt_addr(tx_ep, TUSB_DIR_IN), tx_len, XFER_RESULT_SUCCESS, true);
    tx_buffer = NULL;
    return;
  }

  // Send more data
  uint8_t added_bytes;
  for (added_bytes = 0; (added_bytes < EP_SIZE) && (added_bytes + tx_offset < tx_len); added_bytes++) {
    usb_in_data_write(tx_buffer[added_bytes + tx_offset]);
  }

  // Updating the epno queues the data
  usb_in_ctrl_write(tx_ep & 0xf);
  return;
}

static void process_rx(bool in_isr) {
    // If the OUT handler is still waiting to send, don't do anything.
    uint8_t out_status = usb_out_status_read();
    if (!(out_status & (1 << CSR_USB_OUT_STATUS_IDLE_OFFSET)))
      return;

    uint8_t rx_ep = (out_status >> CSR_USB_OUT_STATUS_EPNO_OFFSET) & 0xf;

    // If the destination buffer doesn't exist, don't drain the hardware
    // fifo.  Note that this can cause deadlocks if the host is waiting
    // on some other endpoint's data!
    if (rx_buffer[rx_ep] == NULL)
      return;

    uint32_t total_read = 0;
    uint32_t current_offset = rx_buffer_offset[rx_ep];
    if (current_offset > rx_buffer_max[rx_ep])
      fomu_error(__LINE__);
    while (usb_out_status_read() & (1 << CSR_USB_OUT_STATUS_HAVE_OFFSET)) {
      uint8_t c = usb_out_data_read();
      total_read++;
      if ((rx_buffer_offset[rx_ep] + current_offset) < rx_buffer_max[rx_ep])
        rx_buffer[rx_ep][current_offset++] = c;
    }

    // Strip off the CRC16
    rx_buffer_offset[rx_ep] += (total_read - 2);
    if (rx_buffer_offset[rx_ep] > rx_buffer_max[rx_ep])
      rx_buffer_offset[rx_ep] = rx_buffer_max[rx_ep];

    if (rx_buffer_max[rx_ep] == rx_buffer_offset[rx_ep]) {
      rx_buffer[rx_ep] = NULL;
      uint16_t len = rx_buffer_offset[rx_ep];
      dcd_event_xfer_complete(0, tu_edpt_addr(rx_ep, TUSB_DIR_OUT), len, XFER_RESULT_SUCCESS, in_isr);
    }

    // Acknowledge having received the data, and re-enable data reception
    usb_out_ctrl_write(1 << CSR_USB_OUT_CTRL_ENABLE_OFFSET);
}

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+

static void dcd_reset(void)
{
  reset_count++;
  usb_setup_ev_enable_write(0);
  usb_in_ev_enable_write(0);
  usb_out_ev_enable_write(0);

  usb_address_write(0);

  // Reset all three FIFO handlers
  usb_setup_ctrl_write(1 << CSR_USB_SETUP_CTRL_RESET_OFFSET);
  usb_in_ctrl_write(1 << CSR_USB_IN_CTRL_RESET_OFFSET);
  usb_out_ctrl_write(1 << CSR_USB_OUT_CTRL_RESET_OFFSET);

  memset((void *)rx_buffer, 0, sizeof(rx_buffer));
  memset((void *)rx_buffer_max, 0, sizeof(rx_buffer_max));
  memset((void *)rx_buffer_offset, 0, sizeof(rx_buffer_offset));
  tx_len = 0;
  tx_buffer = NULL;
  tx_offset = 0;
  tx_ep = 0;

  // Accept incoming data by default.
  usb_out_ctrl_write(1 << CSR_USB_OUT_CTRL_ENABLE_OFFSET);

  // Enable all event handlers and clear their contents
  usb_setup_ev_pending_write(usb_setup_ev_pending_read());
  usb_in_ev_pending_write(usb_in_ev_pending_read());
  usb_out_ev_pending_write(usb_out_ev_pending_read());
  usb_in_ev_enable_write(1);
  usb_out_ev_enable_write(1);
  usb_setup_ev_enable_write(3);

  dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
}

// Initializes the USB peripheral for device mode and enables it.
void dcd_init(uint8_t rhport)
{
  (void) rhport;

  usb_pullup_out_write(0);

  // Enable all event handlers and clear their contents
  usb_setup_ev_pending_write(usb_setup_ev_pending_read());
  usb_in_ev_pending_write(usb_in_ev_pending_read());
  usb_out_ev_pending_write(usb_out_ev_pending_read());
  usb_in_ev_enable_write(1);
  usb_out_ev_enable_write(1);
  usb_setup_ev_enable_write(3);

  // Turn on the external pullup
  usb_pullup_out_write(1);
}

// Enables or disables the USB device interrupt(s). May be used to
// prevent concurrency issues when mutating data structures shared
// between main code and the interrupt handler.
void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
	irq_setmask(irq_getmask() | (1 << USB_INTERRUPT));
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  irq_setmask(irq_getmask() & ~(1 << USB_INTERRUPT));
}

// Called when the device is given a new bus address.
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  // Set address and then acknowledge the SETUP packet
  usb_address_write(dev_addr);

  // ACK the transfer (sets the address)
  usb_setup_ctrl_write(1 << CSR_USB_SETUP_CTRL_HANDLED_OFFSET);
}

// Called when the device received SET_CONFIG request, you can leave this
// empty if your peripheral does not require any specific action.
void dcd_set_config(uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
}

// Called to remote wake up host when suspended (e.g hid keyboard)
void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void) rhport;
  uint8_t ep_num = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t ep_dir = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return false; // Not supported

  if (ep_dir == TUSB_DIR_OUT) {
    rx_buffer_offset[ep_num] = 0;
    rx_buffer_max[ep_num] = 0;
    rx_buffer[ep_num] = NULL;
  }

  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT)
    usb_out_stall_write((1 << CSR_USB_OUT_STALL_STALL_OFFSET) | tu_edpt_number(ep_addr));
  else {
    usb_in_ctrl_write((1 << CSR_USB_IN_CTRL_STALL_OFFSET) | tu_edpt_number(ep_addr));
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT)
    usb_out_stall_write((0 << CSR_USB_OUT_STALL_STALL_OFFSET) | tu_edpt_number(ep_addr));
  // IN endpoints will get unstalled when more data is written.
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void)rhport;
  uint8_t ep_num = tu_edpt_number(ep_addr);
  uint8_t ep_dir = tu_edpt_dir(ep_addr);
  TU_ASSERT(tx_ep < 16);

  // These sorts of transfers are handled in hardware automatically, so simply inform
  // the core that the transfer was processed.
  if ((ep_num == 0) && (total_bytes == 0) && (buffer == NULL)) {
    dcd_event_xfer_complete(0, ep_addr, total_bytes, XFER_RESULT_SUCCESS, false);

    // An IN packet is sent to acknowledge an OUT token.  Re-enable OUT after this.
    // if (ep_dir == TUSB_DIR_IN) {
    //   usb_out_ev_enable_write(0);
    //   process_rx(false);
    //   usb_out_ev_enable_write(1);
    // }
    return true;
  }

  TU_ASSERT(((uint32_t)buffer) >= 0x10000000);
  TU_ASSERT(((uint32_t)buffer) <= 0x10020000);

  if (ep_dir == TUSB_DIR_IN) {
    uint32_t offset;

    // Wait for the tx pipe to free up
    uint8_t previous_reset_count = reset_count;
    while (!((tx_buffer == NULL)
          && (usb_in_status_read() & (1 << CSR_USB_IN_STATUS_IDLE_OFFSET))
          && !(usb_in_status_read() & (1 << CSR_USB_IN_STATUS_HAVE_OFFSET))))
      ;
    // If a reset happens while we're waiting, abort the transfer
    if (previous_reset_count != reset_count)
      return true;

    tx_ep = ep_num;
    tx_len = total_bytes;
    tx_offset = 0;
    tx_buffer = buffer;

    for (offset = 0; (offset < EP_SIZE) && (offset < total_bytes); offset++) {
      usb_in_data_write(buffer[offset]);
    }
    // Updating the epno queues the data
    usb_in_ctrl_write(ep_num & 0xf);
  }
  else if (ep_dir == TUSB_DIR_OUT) {
    TU_ASSERT(rx_buffer[ep_num] == NULL);

    rx_buffer_offset[ep_num] = 0;
    rx_buffer_max[ep_num] = total_bytes;
    rx_buffer[ep_num] = buffer;

    // If there's data in the buffer already, we'll try draining it
    // into the current fifo immediately.
    usb_out_ev_enable_write(0);
    if (usb_out_status_read() & (1 << CSR_USB_OUT_STATUS_HAVE_OFFSET))
      process_rx(false);
    usb_out_ev_enable_write(1);
  }
  return true;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

void hal_dcd_isr(uint8_t rhport)
{
  uint8_t setup_pending   = usb_setup_ev_pending_read();
  uint8_t in_pending      = usb_in_ev_pending_read();
  uint8_t out_pending     = usb_out_ev_pending_read();
  usb_setup_ev_pending_write(setup_pending);
  usb_in_ev_pending_write(in_pending);
  usb_out_ev_pending_write(out_pending);

  // This event means a bus reset occurred.  Reset everything, and
  // abandon any further processing.
  if (setup_pending & 2) {
    dcd_reset();
    return;
  }

  // An "OUT" transaction just completed so we have new data.
  // (But only if we can accept the data)
  // if (out_pending) {
  if (usb_out_ev_enable_read() && out_pending) {
    process_rx(true);
  }

  // An "IN" transaction just completed.
  // Note that due to the way tinyusb's callback system is implemented,
  // we must handle IN and OUT packets before we handle SETUP packets.
  // This ensures that any responses to SETUP packets aren't overwritten.
  // For example, oftentimes a host will request part of a descriptor
  // to begin with, then make a subsequent request.  If we don't handle
  // the IN packets first, then the second request will be truncated.
  if (in_pending) {
    finish_tx();
  } 

  // We got a SETUP packet.  Copy it to the setup buffer and clear
  // the "pending" bit.
  if (setup_pending & 1) {
    // Setup packets are always 8 bytes, plus two bytes of crc16.
    uint8_t setup_packet[10];
    uint32_t setup_length = 0;

    if (!(usb_setup_status_read() & 1))
      fomu_error(__LINE__);

    while (usb_setup_status_read() & 1) {
      uint8_t c = usb_setup_data_read();
      if (setup_length < sizeof(setup_packet))
        setup_packet[setup_length] = c;
      setup_length++;
    }

    // If we have 10 bytes, that's a full SETUP packet plus CRC16.
    // Otherwise, it was an RX error.
    if (setup_length == 10) {
      dcd_event_setup_received(rhport, setup_packet, true);
      // Acknowledge the packet, so long as it isn't a SET_ADDRESS
      // packet.  If it is, leave it unacknowledged and we'll do this
      // in the `dcd_set_address` function instead.
      if (!((setup_packet[0] == 0x00) && (setup_packet[1] == 0x05)))
        usb_setup_ctrl_write(1 << CSR_USB_SETUP_CTRL_HANDLED_OFFSET);
    }
    else {
      fomu_error(__LINE__);
    }
  }
}

#endif
