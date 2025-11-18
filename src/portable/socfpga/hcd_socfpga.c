/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Ha Thach (tinyusb.org)
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
#include "hcd_dwc2.h"
#include "hcd_dwc3.h"

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// optional hcd configuration, called by tuh_configure()
// Since this api has no valid definition in both controllers, we keep it untouched
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  (void) cfg_id;
  (void) cfg_param;

  return true;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  bool ret;
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
    //USB2.0 dwc2 controller initialization
	return hcd_dwc2_init(rhport, rh_init);
  }
  else
  {
    //USB3.1 controller initialization
	ret = hcd_dwc3_init(rhport, rh_init);

  }

  return ret;
}

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_int_enable(rhport);
  }
  else
  {
	hcd_dwc3_int_enable(rhport);
  }
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_int_disable(rhport);
  }
  else
  {
    hcd_dwc3_int_disable(rhport);
  }
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_frame_number(rhport);
  }
  else
  {
  }
  return 0;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_port_connect_status(rhport);
  }
  else
  {
	return hcd_dwc3_port_connect_status(rhport);
  }

  return true;
}

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_port_reset(rhport);
  }
  else
  {
	hcd_dwc3_port_reset(rhport);
  }
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_port_reset_end(rhport);
  }
  else
  {
	hcd_dwc3_port_reset_end(rhport);
  }
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_port_speed_get(rhport);
  }
  else
  {
	return hcd_dwc3_port_speed_get(rhport);
  }
  return 0;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_device_close(rhport, dev_addr);
  }
  else
  {
	hcd_dwc3_device_close(rhport, dev_addr);
  }
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t* desc_ep) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_edpt_open(rhport, dev_addr, desc_ep);
  }
  else
  {
  }

  return true;
}

// No definition present in both controller porting layer
bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport; (void) daddr; (void) ep_addr;
  return false; // TODO not implemented yet
}


// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_edpt_xfer(rhport, dev_addr, ep_addr, buffer, buflen);
  }
  else
  {
	return hcd_dwc3_edpt_xfer(rhport, dev_addr, ep_addr, buffer, buflen);
  }
  return true;
}

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_edpt_abort_xfer(rhport, dev_addr, ep_addr);
  }
  else
  {
  }
  return true;
}

// Submit a special transfer to send 8-byte Setup Packet, when complete hcd_event_xfer_complete() must be invoked
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_setup_send(rhport, dev_addr, setup_packet);
  }
  else
  {
	return hcd_dwc3_setup_send(rhport, dev_addr, setup_packet);
  }
  return true;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	return hcd_dwc2_edpt_clear_stall(rhport, dev_addr, ep_addr);
  }
  else
  {
  }

  return true;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  if( rhport == SOCFPGA_USB2_OTG_PORT )
  {
	hcd_dwc2_int_handler(rhport, in_isr);
  }
  else
  {
    hcd_dwc3_int_handler(rhport, in_isr);
  }

}

