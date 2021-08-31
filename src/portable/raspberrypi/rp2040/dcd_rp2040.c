/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_RP2040

#include "pico.h"
#include "rp2040_usb.h"

#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
#include "pico/fix/rp2040_usb_device_enumeration.h"
#endif

#include "device/dcd.h"

// Current implementation force vbus detection as always present, causing device think it is always plugged into host.
// Therefore it cannot detect disconnect event, mistaken it as suspend.
// Note: won't work if change to 0 (for now)
#define FORCE_VBUS_DETECT   1

/*------------------------------------------------------------------*/
/* Low level controller
 *------------------------------------------------------------------*/

#define usb_hw_set hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

// Init these in dcd_init
static uint8_t *next_buffer_ptr;

// USB_MAX_ENDPOINTS Endpoints, direction TUSB_DIR_OUT for out and TUSB_DIR_IN for in.
static struct hw_endpoint hw_endpoints[USB_MAX_ENDPOINTS][2];

static inline struct hw_endpoint *hw_endpoint_get_by_num(uint8_t num, tusb_dir_t dir)
{
  return &hw_endpoints[num][dir];
}

static struct hw_endpoint *hw_endpoint_get_by_addr(uint8_t ep_addr)
{
  uint8_t num = tu_edpt_number(ep_addr);
  tusb_dir_t dir = tu_edpt_dir(ep_addr);
  return hw_endpoint_get_by_num(num, dir);
}

static void _hw_endpoint_alloc(struct hw_endpoint *ep, uint8_t transfer_type)
{
  // size must be multiple of 64
  uint16_t size = tu_div_ceil(ep->wMaxPacketSize, 64) * 64u;

  // double buffered Bulk endpoint
  if ( transfer_type == TUSB_XFER_BULK )
  {
    size *= 2u;
  }

  ep->hw_data_buf = next_buffer_ptr;
  next_buffer_ptr += size;

  assert(((uintptr_t )next_buffer_ptr & 0b111111u) == 0);
  uint dpram_offset = hw_data_offset(ep->hw_data_buf);
  assert(hw_data_offset(next_buffer_ptr) <= USB_DPRAM_MAX);

  pico_info("  Alloced %d bytes at offset 0x%x (0x%p)\r\n", size, dpram_offset, ep->hw_data_buf);

  // Fill in endpoint control register with buffer offset
  uint32_t const reg = EP_CTRL_ENABLE_BITS | (transfer_type << EP_CTRL_BUFFER_TYPE_LSB) | dpram_offset;

  *ep->endpoint_control = reg;
}

#if 0 // todo unused
static void _hw_endpoint_close(struct hw_endpoint *ep)
{
    // Clear hardware registers and then zero the struct
    // Clears endpoint enable
    *ep->endpoint_control = 0;
    // Clears buffer available, etc
    *ep->buffer_control = 0;
    // Clear any endpoint state
    memset(ep, 0, sizeof(struct hw_endpoint));
}

static void hw_endpoint_close(uint8_t ep_addr)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_close(ep);
}
#endif

static void hw_endpoint_init(uint8_t ep_addr, uint16_t wMaxPacketSize, uint8_t transfer_type)
{
  struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);

  const uint8_t num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  ep->ep_addr = ep_addr;

  // For device, IN is a tx transfer and OUT is an rx transfer
  ep->rx = (dir == TUSB_DIR_OUT);

  // Response to a setup packet on EP0 starts with pid of 1
  ep->next_pid = (num == 0 ? 1u : 0u);

  ep->wMaxPacketSize = wMaxPacketSize;
  ep->transfer_type = transfer_type;

  // Every endpoint has a buffer control register in dpram
  if ( dir == TUSB_DIR_IN )
  {
    ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].in;
  }
  else
  {
    ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].out;
  }

  // Clear existing buffer control state
  *ep->buffer_control = 0;

  if ( num == 0 )
  {
    // EP0 has no endpoint control register because
    // the buffer offsets are fixed
    ep->endpoint_control = NULL;

    // Buffer offset is fixed (also double buffered)
    ep->hw_data_buf = (uint8_t*) &usb_dpram->ep0_buf_a[0];
  }
  else
  {
    // Set the endpoint control register (starts at EP1, hence num-1)
    if ( dir == TUSB_DIR_IN )
    {
      ep->endpoint_control = &usb_dpram->ep_ctrl[num - 1].in;
    }
    else
    {
      ep->endpoint_control = &usb_dpram->ep_ctrl[num - 1].out;
    }

    // alloc a buffer and fill in endpoint control register
    _hw_endpoint_alloc(ep, transfer_type);
  }
}

static void hw_endpoint_xfer(uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    hw_endpoint_xfer_start(ep, buffer, total_bytes);
}

static void hw_handle_buff_status(void)
{
    uint32_t remaining_buffers = usb_hw->buf_status;
    pico_trace("buf_status 0x%08x\n", remaining_buffers);
    uint bit = 1u;
    for (uint i = 0; remaining_buffers && i < USB_MAX_ENDPOINTS * 2; i++)
    {
        if (remaining_buffers & bit)
        {
            // clear this in advance
            usb_hw_clear->buf_status = bit;
            // IN transfer for even i, OUT transfer for odd i
            struct hw_endpoint *ep = hw_endpoint_get_by_num(i >> 1u, !(i & 1u));
            // Continue xfer
            bool done = hw_endpoint_xfer_continue(ep);
            if (done)
            {
                // Notify
                dcd_event_xfer_complete(0, ep->ep_addr, ep->xferred_len, XFER_RESULT_SUCCESS, true);
                hw_endpoint_reset_transfer(ep);
            }
            remaining_buffers &= ~bit;
        }
        bit <<= 1u;
    }
}

static void reset_ep0(void)
{
    // If we have finished this transfer on EP0 set pid back to 1 for next
    // setup transfer. Also clear a stall in case
    uint8_t addrs[] = {0x0, 0x80};
    for (uint i = 0 ; i < TU_ARRAY_SIZE(addrs); i++)
    {
        struct hw_endpoint *ep = hw_endpoint_get_by_addr(addrs[i]);
        ep->next_pid = 1u;
    }
}

static void reset_all_endpoints(void)
{
  memset(hw_endpoints, 0, sizeof(hw_endpoints));
  next_buffer_ptr = &usb_dpram->epx_data[0];

  // Init Control endpoint out & in
  hw_endpoint_init(0x0, 64, TUSB_XFER_CONTROL);
  hw_endpoint_init(0x80, 64, TUSB_XFER_CONTROL);
}

static void dcd_rp2040_irq(void)
{
    uint32_t const status = usb_hw->ints;
    uint32_t handled = 0;

    if (status & USB_INTS_SETUP_REQ_BITS)
    {
        handled |= USB_INTS_SETUP_REQ_BITS;
        uint8_t const *setup = (uint8_t const *)&usb_dpram->setup_packet;
        // Clear stall bits and reset pid
        reset_ep0();
        // Pass setup packet to tiny usb
        dcd_event_setup_received(0, setup, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
    }

    if (status & USB_INTS_BUFF_STATUS_BITS)
    {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        hw_handle_buff_status();
    }

#if FORCE_VBUS_DETECT == 0
    // Since we force VBUS detect On, device will always think it is connected and
    // couldn't distinguish between disconnect and suspend
    if (status & USB_INTS_DEV_CONN_DIS_BITS)
    {
        handled |= USB_INTS_DEV_CONN_DIS_BITS;

        if ( usb_hw->sie_status & USB_SIE_STATUS_CONNECTED_BITS )
        {
          // Connected: nothing to do
        }else
        {
          // Disconnected
          dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
        }

        usb_hw_clear->sie_status = USB_SIE_STATUS_CONNECTED_BITS;
    }
#endif

    // SE0 for 2.5 us or more (will last at least 10ms)
    if (status & USB_INTS_BUS_RESET_BITS)
    {
        pico_trace("BUS RESET\n");

        handled |= USB_INTS_BUS_RESET_BITS;

        usb_hw->dev_addr_ctrl = 0;
        reset_all_endpoints();
        dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;

#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
        // Only run enumeration walk-around if pull up is enabled
        if ( usb_hw->sie_ctrl & USB_SIE_CTRL_PULLUP_EN_BITS ) rp2040_usb_device_enumeration_fix();
#endif
    }

    /* Note from pico datasheet 4.1.2.6.4 (v1.2)
     * If you enable the suspend interrupt, it is likely you will see a suspend interrupt when
     * the device is first connected but the bus is idle. The bus can be idle for a few ms before
     * the host begins sending start of frame packets. You will also see a suspend interrupt
     * when the device is disconnected if you do not have a VBUS detect circuit connected. This is
     * because without VBUS detection, it is impossible to tell the difference between
     * being disconnected and suspended.
     */
    if (status & USB_INTS_DEV_SUSPEND_BITS)
    {
        handled |= USB_INTS_DEV_SUSPEND_BITS;
        dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_SUSPENDED_BITS;
    }

    if (status & USB_INTS_DEV_RESUME_FROM_HOST_BITS)
    {
        handled |= USB_INTS_DEV_RESUME_FROM_HOST_BITS;
        dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_RESUME_BITS;
    }

    if (status ^ handled)
    {
        panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
    }
}

#define USB_INTS_ERROR_BITS ( \
    USB_INTS_ERROR_DATA_SEQ_BITS      |  \
    USB_INTS_ERROR_BIT_STUFF_BITS     |  \
    USB_INTS_ERROR_CRC_BITS           |  \
    USB_INTS_ERROR_RX_OVERFLOW_BITS   |  \
    USB_INTS_ERROR_RX_TIMEOUT_BITS)

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/

void dcd_init (uint8_t rhport)
{
  pico_trace("dcd_init %d\n", rhport);
  assert(rhport == 0);

  // Reset hardware to default state
  rp2040_usb_init();

#if FORCE_VBUS_DETECT
  // Force VBUS detect so the device thinks it is plugged into a host
  usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
#endif

  irq_set_exclusive_handler(USBCTRL_IRQ, dcd_rp2040_irq);

  // reset endpoints
  reset_all_endpoints();

  // Initializes the USB peripheral for device mode and enables it.
  // Don't need to enable the pull up here. Force VBUS
  usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

  // Enable individual controller IRQS here. Processor interrupt enable will be used
  // for the global interrupt enable...
  // Note: Force VBUS detect cause disconnection not detectable
  usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
  usb_hw->inte     = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS | USB_INTS_SETUP_REQ_BITS |
                     USB_INTS_DEV_SUSPEND_BITS | USB_INTS_DEV_RESUME_FROM_HOST_BITS |
                     (FORCE_VBUS_DETECT ? 0 : USB_INTS_DEV_CONN_DIS_BITS);

  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, true);
}

void dcd_int_disable(uint8_t rhport)
{
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, false);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  pico_trace("dcd_set_address %d %d\n", rhport, dev_addr);
  assert(rhport == 0);

  // Can't set device address in hardware until status xfer has complete
  // Send 0len complete response on EP0 IN
  reset_ep0();
  hw_endpoint_xfer(0x80, NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
    pico_info("dcd_remote_wakeup %d\n", rhport);
    assert(rhport == 0);
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_RESUME_BITS;
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
    pico_info("dcd_disconnect %d\n", rhport);
    assert(rhport == 0);
    usb_hw_clear->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
    pico_info("dcd_connect %d\n", rhport);
    assert(rhport == 0);
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  if ( request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
       request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
       request->bRequest == TUSB_REQ_SET_ADDRESS )
  {
    pico_trace("Set HW address %d\n", request->wValue);
    usb_hw->dev_addr_ctrl = (uint8_t) request->wValue;
  }

  reset_ep0();
}

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
    assert(rhport == 0);
    hw_endpoint_init(desc_edpt->bEndpointAddress, desc_edpt->wMaxPacketSize.size, desc_edpt->bmAttributes.xfer);
    return true;
}

void dcd_edpt_close_all (uint8_t rhport)
{
  (void) rhport;
  // TODO implement dcd_edpt_close_all()
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
    assert(rhport == 0);
    hw_endpoint_xfer(ep_addr, buffer, total_bytes);
    return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  pico_trace("dcd_edpt_stall %02x\n", ep_addr);
  assert(rhport == 0);

  if ( tu_edpt_number(ep_addr) == 0 )
  {
    // A stall on EP0 has to be armed so it can be cleared on the next setup packet
    usb_hw_set->ep_stall_arm = (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) ? USB_EP_STALL_ARM_EP0_IN_BITS : USB_EP_STALL_ARM_EP0_OUT_BITS;
  }

  struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);

  // TODO check with double buffered
  _hw_endpoint_buffer_control_set_mask32(ep, USB_BUF_CTRL_STALL);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  pico_trace("dcd_edpt_clear_stall %02x\n", ep_addr);
  assert(rhport == 0);

  if (tu_edpt_number(ep_addr))
  {
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);

    // clear stall also reset toggle to DATA0
    // TODO check with double buffered
    _hw_endpoint_buffer_control_clear_mask32(ep, USB_BUF_CTRL_STALL | USB_BUF_CTRL_DATA1_PID);
  }
}

void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
{
    (void) rhport;
    (void) ep_addr;

    // usbd.c says: In progress transfers on this EP may be delivered after this call
    pico_trace("dcd_edpt_close %02x\n", ep_addr);
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;
  dcd_rp2040_irq();
}

#endif
