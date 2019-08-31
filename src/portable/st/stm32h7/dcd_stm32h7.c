/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 William D. Jones for Adafruit Industries
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_STM32H7

#include "device/dcd.h"
#include "stm32h7xx.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
#define DEVICE_BASE (USB_OTG_DeviceTypeDef *) (USB2_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE)

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Programming model begins on page 2634 of Rev 6 of reference manual.
  USB2_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_TXFELVL | USB_OTG_GAHBCFG_GINT;

  // No HNP/SRP (no OTG support), program timeout later, turnaround
  // programmed for 32+ MHz.
  USB2_OTG_FS->GUSBCFG |= (0x06 << USB_OTG_GUSBCFG_TRDT_Pos) | USB_OTG_GUSBCFG_PHYSEL;

  // Clear all used interrupts
  USB2_OTG_FS->GINTSTS |= USB_OTG_GINTSTS_OTGINT | USB_OTG_GINTSTS_MMIS | \
    USB_OTG_GINTSTS_USBRST | USB_OTG_GINTSTS_ENUMDNE | \
    USB_OTG_GINTSTS_ESUSP | USB_OTG_GINTSTS_USBSUSP | USB_OTG_GINTSTS_SOF;

  // Required as part of core initialization. Disable OTGINT as we don't use
  // it right now. TODO: How should mode mismatch be handled? It will cause
  // the core to stop working/require reset.
  USB2_OTG_FS->GINTMSK |= /* USB_OTG_GINTMSK_OTGINT | */ USB_OTG_GINTMSK_MMISM;

  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE;

  // If USB host misbehaves during status portion of control xfer
  // (non zero-length packet), send STALL back and discard. Full speed.
  dev->DCFG |=  USB_OTG_DCFG_NZLSOHSK | (3 << USB_OTG_DCFG_DSPD_Pos);

  USB2_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM | \
    USB_OTG_GINTMSK_SOFM | USB_OTG_GINTMSK_RXFLVLM /* SB_OTG_GINTMSK_ESUSPM | \
    USB_OTG_GINTMSK_USBSUSPM */;

  // Enable pullup, enable peripheral.
#ifdef USB_OTG_GCCFG_VBDEN
  USB2_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN | USB_OTG_GCCFG_PWRDWN;
#else
  USB2_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_PWRDWN;
#endif

  // This step does not appear to be specified in the programmer's model.
  dev->DCTL &= ~USB_OTG_DCTL_SDIS;
}

void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(OTG_FS_IRQn);
}

void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(OTG_FS_IRQn);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;
  (void) desc_edpt;

  return false;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  (void) ep_addr;
  (void) buffer;
  (void) total_bytes;

  return false;
}

// TODO: The logic for STALLing and disabling an endpoint is very similar
// (send STALL versus NAK handshakes back). Refactor into resuable function.
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  (void) ep_addr;
}

/*------------------------------------------------------------------*/

void OTG_FS_IRQHandler (void)
{

}

#endif
