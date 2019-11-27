/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#if CFG_TUSB_MCU == OPT_MCU_SAMG

#include "sam.h"
#include "device/dcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+


/*------------------------------------------------------------------*/
/* Device API
 *------------------------------------------------------------------*/

// Set up endpoint 0, clear all other endpoints
static void bus_reset(void)
{

}

// Initialize controller to device mode
void dcd_init (uint8_t rhport)
{
  (void) rhport;




  // Pull-up & Transceiver enable
  UDP->UDP_TXVC = UDP_TXVC_PUON;
}

// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(UDP_IRQn);
}

// Disable device interrupt
void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(UDP_IRQn);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;
}

// Receive Set Configure request
void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
}

// Wake up host
void dcd_remote_wakeup (uint8_t rhport)
{
  (void) rhport;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;
  (void) ep_desc;
  return false;
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  (void) ep_addr;
  (void) buffer;
  (void) total_bytes;
  return false;
}

// Stall endpoint
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void dcd_isr(uint8_t rhport)
{
  uint32_t const intr_mask   = UDP->UDP_IMR;
  uint32_t const intr_status = UDP->UDP_ISR & intr_mask;

  // clear interrupt
  UDP->UDP_ICR = intr_status;

  // Bus reset
  if (intr_status & UDP_ISR_ENDBUSRES)
  {
    bus_reset();
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET, true);
  }
}

#endif
