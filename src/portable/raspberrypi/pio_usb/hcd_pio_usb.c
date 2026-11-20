/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && CFG_TUH_RPI_PIO_USB

#include "pico.h"
#include "hardware/gpio.h"

#include "pio_usb.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "pio_usb_ll.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"

#define RHPORT_OFFSET     1
#define RHPORT_PIO(_x)    ((_x)-RHPORT_OFFSET)

static pio_usb_configuration_t pio_host_cfg = PIO_USB_DEFAULT_CONFIG;

// Pico-PIO-USB has no deinit: pio_usb_host_init() claims the PIO and starts the SOF timer, and pio_usb_host_stop()/
// restart() never return (only its removed host stack cleared their flags). So hcd_deinit() parks the root ports and
// the next hcd_init() detects their devices again. Call it on the core of the first hcd_init(), where frames run.
static bool pio_inited = false;           // pio_usb_host_init() is done
static volatile bool hcd_running = false; // between hcd_init() and hcd_deinit()
static bool port_in_reset[PIO_USB_ROOT_PORT_CNT]; // between hcd_port_reset() and hcd_port_reset_end(): lines in SE0

// Park a root port: no SOF and no transfers. A frame then skips the port, and does not check it for a detach either.
// Its endpoints are freed and its pending interrupt status is cleared.
static void root_port_park(uint8_t pio_rhport) {
  root_port_t *root = PIO_USB_ROOT_PORT(pio_rhport);

  root->suspended = true;

  // End a bus reset that is under way: its hcd_port_reset_end() will not come, and the lines would stay in SE0.
  // As pio_usb_host_port_reset_end(), without un-suspending the port.
  gpio_set_oeover(root->pin_dp, GPIO_OVERRIDE_NORMAL);
  gpio_set_oeover(root->pin_dm, GPIO_OVERRIDE_NORMAL);
  gpio_set_outover(root->pin_dp, GPIO_OVERRIDE_NORMAL);
  gpio_set_outover(root->pin_dm, GPIO_OVERRIDE_NORMAL);
  port_in_reset[pio_rhport] = false;

  for (uint8_t ep_idx = 0; ep_idx < PIO_USB_EP_POOL_CNT; ep_idx++) {
    endpoint_t *ep = PIO_USB_ENDPOINT(ep_idx);
    if (ep->root_idx == pio_rhport) {
      ep->size         = 0; // size 0 marks a free endpoint
      ep->has_transfer = false;
    }
  }

  root->ep_complete = 0;
  root->ep_stalled  = 0;
  root->ep_error    = 0;
  root->ints        = 0;
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param) {
  (void) rhport;
  TU_VERIFY(cfg_id == TUH_CFGID_RPI_PIO_USB_CONFIGURATION);
  memcpy(&pio_host_cfg, cfg_param, sizeof(pio_usb_configuration_t));
  return true;
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;

  // before the first frame can run: its connect event must reach usbh
  hcd_running = true;

  if (!pio_inited) {
    // To run USB SOF interrupt in core1, call this init in core1
    pio_usb_host_init(&pio_host_cfg);
    pio_inited = true;
  } else {
    // After hcd_deinit(): the PIO and the SOF timer still run. Mark the ports as disconnected, so the next frame
    // reports the device that is attached now as a new connection. The bus reset of its enumeration ends the suspend
    // that hcd_deinit() started.
    for (uint8_t root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
      root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
      if (root->initialized) {
        root->connected = false;
      }
    }
  }

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;

  // Events from now on are dropped: the host stack is stopping
  hcd_running = false;

  // All ports: the IRQ handler drops the events of every port. Without SOF, the devices enter USB suspend.
  for (uint8_t root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    if (PIO_USB_ROOT_PORT(root_idx)->initialized) {
      root_port_park(root_idx);
    }
  }

  return true;
}

void hcd_port_reset(uint8_t rhport) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  port_in_reset[pio_rhport] = true;
  pio_usb_host_port_reset_start(pio_rhport);
}

void hcd_port_reset_end(uint8_t rhport) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  pio_usb_host_port_reset_end(pio_rhport);
  port_in_reset[pio_rhport] = false;
}

bool hcd_port_connect_status(uint8_t rhport) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);

  root_port_t *root = PIO_USB_ROOT_PORT(pio_rhport);
  port_pin_status_t line_state = pio_usb_bus_get_line_state(root);

  // A frame does not check a suspended port for a detach, and looks for a new connection only on a port that is not
  // connected. So a device that left while the port was suspended, e.g. during usbh's attach debounce (which then
  // ends without a bus reset), would leave the port 'connected' for good, and no device attached later would be seen.
  // Mark it disconnected, so the next frame detects a new connection. Not during a bus reset: the host drives SE0.
  if (line_state == PORT_PIN_SE0 && root->connected && root->suspended && !port_in_reset[pio_rhport]) {
    root->connected = false;
  }

  return line_state != PORT_PIN_SE0;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  // TODO determine link speed
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return PIO_USB_ROOT_PORT(pio_rhport)->is_fullspeed ? TUSB_SPEED_FULL : TUSB_SPEED_LOW;
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  pio_usb_host_close_device(pio_rhport, dev_addr);
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return pio_usb_host_get_frame_number();
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *desc_ep) {
  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);
  bool const need_pre = (bus_info.hub_addr && bus_info.speed == TUSB_SPEED_LOW);

  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return pio_usb_host_endpoint_open(pio_rhport, dev_addr, (uint8_t const *) desc_ep, need_pre);
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return pio_usb_host_endpoint_close(pio_rhport, daddr, ep_addr);
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return pio_usb_host_endpoint_transfer(pio_rhport, dev_addr, ep_addr, buffer, buflen);
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return pio_usb_host_endpoint_abort_transfer(pio_rhport, dev_addr, ep_addr);
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  uint8_t const pio_rhport = RHPORT_PIO(rhport);
  return pio_usb_host_send_setup(pio_rhport, dev_addr, setup_packet);
}

//bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr)
//{
//    // EPX is shared, so multiple device addresses and endpoint addresses share that
//    // so if any transfer is active on epx, we are busy. Interrupt endpoints have their own
//    // EPX so ep->active will only be busy if there is a pending transfer on that interrupt endpoint
//    // on that device
//    pico_trace("hcd_edpt_busy dev addr %d ep_addr 0x%x\r\n", dev_addr, ep_addr);
//    struct hw_endpoint *ep = get_dev_ep(dev_addr, ep_addr);
//    assert(ep);
//    bool busy = ep->active;
//    pico_trace("busy == %d\r\n", busy);
//    return busy;
//}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false; // data toggle reset not implemented
}

static void __no_inline_not_in_flash_func(handle_endpoint_irq)(root_port_t *rport, xfer_result_t result,
                                                               volatile uint32_t *ep_reg) {
  (void) rport;
  const uint32_t ep_all = *ep_reg;

  for ( uint8_t ep_idx = 0; ep_idx < PIO_USB_EP_POOL_CNT; ep_idx++ ) {
    uint32_t const mask = (1u << ep_idx);

    if ( ep_all & mask ) {
      endpoint_t * ep = PIO_USB_ENDPOINT(ep_idx);
      hcd_event_xfer_complete(ep->dev_addr, ep->ep_num, ep->actual_len, result, true);
    }
  }

  // clear all
  (*ep_reg) &= ~ep_all;
}

// IRQ Handler
void __no_inline_not_in_flash_func(pio_usb_host_irq_handler)(uint8_t root_id) {
  uint8_t const tu_rhport = root_id + 1;
  root_port_t *rport = PIO_USB_ROOT_PORT(root_id);
  uint32_t const ints = rport->ints;

  if (!hcd_running) {
    // after hcd_deinit(): the host stack is stopped, its event queue may be gone
    rport->ep_complete = 0;
    rport->ep_stalled  = 0;
    rport->ep_error    = 0;
    rport->ints &= ~ints;
    return;
  }

  if ( ints & PIO_USB_INTS_ENDPOINT_COMPLETE_BITS ) {
    handle_endpoint_irq(rport, XFER_RESULT_SUCCESS, &rport->ep_complete);
  }

  if ( ints & PIO_USB_INTS_ENDPOINT_STALLED_BITS ) {
    handle_endpoint_irq(rport, XFER_RESULT_STALLED, &rport->ep_stalled);
  }

  if ( ints & PIO_USB_INTS_ENDPOINT_ERROR_BITS ) {
    handle_endpoint_irq(rport, XFER_RESULT_FAILED, &rport->ep_error);
  }

  if ( ints & PIO_USB_INTS_CONNECT_BITS ) {
    hcd_event_device_attach(tu_rhport, true);
  }

  if ( ints & PIO_USB_INTS_DISCONNECT_BITS ) {
    hcd_event_device_remove(tu_rhport, true);
  }

  // clear all
  rport->ints &= ~ints;
}

#endif
