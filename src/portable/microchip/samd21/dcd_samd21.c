/**************************************************************************/
/*!
    @file     dcd_nrf5x.c
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, Scott Shawcroft for Adafruit Industries
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_SAMD21

#include "device/dcd.h"
#include "sam.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
static ATTR_ALIGNED(4) UsbDeviceDescBank sram_registers[8][2];
static ATTR_ALIGNED(4) uint8_t _setup_packet[8];

// Setup the control endpoint 0.
static void bus_reset(void) {
    // Max size of packets is 64 bytes.
    UsbDeviceDescBank* bank_out = &sram_registers[0][TUSB_DIR_OUT];
    bank_out->PCKSIZE.bit.SIZE = 0x3;
    UsbDeviceDescBank* bank_in = &sram_registers[0][TUSB_DIR_IN];
    bank_in->PCKSIZE.bit.SIZE = 0x3;

    UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[0];
    ep->EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE0(0x1) | USB_DEVICE_EPCFG_EPTYPE1(0x1);
    ep->EPINTENSET.reg = USB_DEVICE_EPINTENSET_TRCPT0 | USB_DEVICE_EPINTENSET_TRCPT1 | USB_DEVICE_EPINTENSET_RXSTP;

    // Prepare for setup packet
    dcd_edpt_xfer(0, 0, _setup_packet, sizeof(_setup_packet));
}


/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
bool dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Reset to get in a clean state.
  USB->DEVICE.CTRLA.bit.SWRST = true;
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 0) {}
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 1) {}

  USB->DEVICE.PADCAL.bit.TRANSP = (*((uint32_t*) USB_FUSES_TRANSP_ADDR) & USB_FUSES_TRANSP_Msk) >> USB_FUSES_TRANSP_Pos;
  USB->DEVICE.PADCAL.bit.TRANSN = (*((uint32_t*) USB_FUSES_TRANSN_ADDR) & USB_FUSES_TRANSN_Msk) >> USB_FUSES_TRANSN_Pos;
  USB->DEVICE.PADCAL.bit.TRIM = (*((uint32_t*) USB_FUSES_TRIM_ADDR) & USB_FUSES_TRIM_Msk) >> USB_FUSES_TRIM_Pos;

  USB->DEVICE.QOSCTRL.bit.CQOS = USB_QOSCTRL_CQOS_HIGH_Val;
  USB->DEVICE.QOSCTRL.bit.DQOS = USB_QOSCTRL_DQOS_HIGH_Val;

  // Configure registers
  USB->DEVICE.DESCADD.reg = (uint32_t) &sram_registers;
  USB->DEVICE.CTRLB.reg = USB_DEVICE_CTRLB_SPDCONF_FS;
  USB->DEVICE.CTRLA.reg = USB_CTRLA_MODE_DEVICE | USB_CTRLA_ENABLE | USB_CTRLA_RUNSTDBY;
  while (USB->DEVICE.SYNCBUSY.bit.ENABLE == 1) {}

  USB->DEVICE.INTENSET.reg = USB_DEVICE_INTENSET_SOF | USB_DEVICE_INTENSET_EORST;

  return true;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;

  // Wait for EP0 to finish before switching the address.
  while (USB->DEVICE.DeviceEndpoint[0].EPSTATUS.bit.BK1RDY == 1) {}
  USB->DEVICE.DADD.reg = USB_DEVICE_DADD_DADD(dev_addr) | USB_DEVICE_DADD_ADDEN;
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

uint32_t dcd_get_microframe(uint8_t rhport)
{
  (void) rhport;
  return USB->DEVICE.FNUM.reg & (TU_BIT(14) - 1);
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  UsbDeviceDescBank* bank = &sram_registers[epnum][dir];
  uint32_t size_value = 0;
  while (size_value < 7) {
    if (1 << (size_value + 3) == desc_edpt->wMaxPacketSize.size) {
      break;
    }
    size_value++;
  }

  // unsupported endpoint size
  if ( size_value == 7 && desc_edpt->wMaxPacketSize.size != 1023 ) return false;

  bank->PCKSIZE.bit.SIZE = size_value;

  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if ( dir == TUSB_DIR_OUT )
  {
    ep->EPCFG.bit.EPTYPE0 = desc_edpt->bmAttributes.xfer + 1;
    ep->EPINTENSET.bit.TRCPT0 = true;
  }else
  {
    ep->EPCFG.bit.EPTYPE1 = desc_edpt->bmAttributes.xfer + 1;
    ep->EPINTENSET.bit.TRCPT1 = true;
  }

  return true;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  UsbDeviceDescBank* bank = &sram_registers[epnum][dir];
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  // A setup token can occur immediately after an OUT STATUS packet so make sure we have a valid
  // buffer for the control endpoint.
  if (epnum == 0 && dir == 0 && buffer == NULL) {
      buffer = _setup_packet;
  }

  bank->ADDR.reg = (uint32_t) buffer;
  if ( dir == TUSB_DIR_OUT )
  {
    bank->PCKSIZE.bit.MULTI_PACKET_SIZE = total_bytes;
    bank->PCKSIZE.bit.BYTE_COUNT = 0;
    ep->EPSTATUSCLR.reg |= USB_DEVICE_EPSTATUSCLR_BK0RDY;
    ep->EPINTFLAG.reg |= USB_DEVICE_EPINTFLAG_TRFAIL0;
  } else
  {
    bank->PCKSIZE.bit.MULTI_PACKET_SIZE = 0;
    bank->PCKSIZE.bit.BYTE_COUNT = total_bytes;
    // bank->PCKSIZE.bit.AUTO_ZLP = 1;
    ep->EPSTATUSSET.reg |= USB_DEVICE_EPSTATUSSET_BK1RDY;
    ep->EPINTFLAG.reg |= USB_DEVICE_EPINTFLAG_TRFAIL1;
  }

  return true;
}

bool dcd_edpt_stalled (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // control is never got halted
  if ( ep_addr == 0 ) {
      return false;
  }

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
  return (tu_edpt_dir(ep_addr) == TUSB_DIR_IN ) ? ep->EPINTFLAG.bit.STALL1 : ep->EPINTFLAG.bit.STALL0;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
      ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
  } else {
      ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ0;

      // for control, stall both IN & OUT
      if (ep_addr == 0) {
        ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
      }
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ1;
  } else {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ0;
  }
}

bool dcd_edpt_busy (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // USBD shouldn't check control endpoint state
  if ( 0 == ep_addr ) return false;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    return ep->EPINTFLAG.bit.TRCPT1 == 0 && ep->EPSTATUS.bit.BK1RDY == 1;
  }
  return ep->EPINTFLAG.bit.TRCPT0 == 0 && ep->EPSTATUS.bit.BK0RDY == 1;
}

/*------------------------------------------------------------------*/

static bool maybe_handle_setup_packet(void) {
    if (USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.bit.RXSTP)
    {
        USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_RXSTP;

        // This copies the data elsewhere so we can reuse the buffer.
        dcd_event_setup_received(0, (uint8_t*) sram_registers[0][0].ADDR.reg, true);
        return true;
    }
    return false;
}

void maybe_transfer_complete(void) {
    uint32_t epints = USB->DEVICE.EPINTSMRY.reg;
    for (uint8_t epnum = 0; epnum < USB_EPT_NUM; epnum++) {
        if ((epints & (1 << epnum)) == 0) {
            continue;
        }

        if (maybe_handle_setup_packet()) {
            continue;
        }
        UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

        uint32_t epintflag = ep->EPINTFLAG.reg;

        uint16_t total_transfer_size;

        // Handle IN completions
        if ((epintflag & USB_DEVICE_EPINTFLAG_TRCPT1) != 0) {
            ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;

            UsbDeviceDescBank* bank = &sram_registers[epnum][TUSB_DIR_IN];
            total_transfer_size = bank->PCKSIZE.bit.BYTE_COUNT;

            uint8_t ep_addr = epnum | TUSB_DIR_IN_MASK;
            dcd_event_xfer_complete(0, ep_addr, total_transfer_size, XFER_RESULT_SUCCESS, true);
        }

        // Handle OUT completions
        if ((epintflag & USB_DEVICE_EPINTFLAG_TRCPT0) != 0) {
            ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;

            UsbDeviceDescBank* bank = &sram_registers[epnum][TUSB_DIR_OUT];
            total_transfer_size = bank->PCKSIZE.bit.BYTE_COUNT;

            uint8_t ep_addr = epnum;
            dcd_event_xfer_complete(0, ep_addr, total_transfer_size, XFER_RESULT_SUCCESS, true);
        }

        // just finished status stage (total size = 0), prepare for next setup packet
        if (epnum == 0 && total_transfer_size == 0) {
            dcd_edpt_xfer(0, 0, _setup_packet, sizeof(_setup_packet));
        }
    }
}

void USB_Handler(void) {
  uint32_t int_status = USB->DEVICE.INTFLAG.reg;

  /*------------- Interrupt Processing -------------*/
  if ( int_status & USB_DEVICE_INTFLAG_EORST )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTENCLR_EORST;
    bus_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }

  if ( int_status & USB_DEVICE_INTFLAG_SOF )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  // Setup packet received.
  maybe_handle_setup_packet();

  // Handle complete transfer
  maybe_transfer_complete();
}

#endif
