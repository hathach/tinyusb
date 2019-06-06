/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_SAMD51

#include "device/dcd.h"
#include "sam.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
static UsbDeviceDescBank sram_registers[8][2];
static TU_ATTR_ALIGNED(4) uint8_t _setup_packet[8];

// Setup the control endpoint 0.
static void bus_reset(void)
{
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
void dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Reset to get in a clean state.
  USB->DEVICE.CTRLA.bit.SWRST = true;
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 0) {}
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 1) {}

  USB->DEVICE.PADCAL.bit.TRANSP = (*((uint32_t*) USB_FUSES_TRANSP_ADDR) & USB_FUSES_TRANSP_Msk) >> USB_FUSES_TRANSP_Pos;
  USB->DEVICE.PADCAL.bit.TRANSN = (*((uint32_t*) USB_FUSES_TRANSN_ADDR) & USB_FUSES_TRANSN_Msk) >> USB_FUSES_TRANSN_Pos;
  USB->DEVICE.PADCAL.bit.TRIM = (*((uint32_t*) USB_FUSES_TRIM_ADDR) & USB_FUSES_TRIM_Msk) >> USB_FUSES_TRIM_Pos;

  USB->DEVICE.QOSCTRL.bit.CQOS = 3;
  USB->DEVICE.QOSCTRL.bit.DQOS = 3;

  // Configure registers
  USB->DEVICE.DESCADD.reg = (uint32_t) &sram_registers;
  USB->DEVICE.CTRLB.reg = USB_DEVICE_CTRLB_SPDCONF_FS;
  USB->DEVICE.CTRLA.reg = USB_CTRLA_MODE_DEVICE | USB_CTRLA_ENABLE | USB_CTRLA_RUNSTDBY;
  while (USB->DEVICE.SYNCBUSY.bit.ENABLE == 1) {}

  USB->DEVICE.INTFLAG.reg |= USB->DEVICE.INTFLAG.reg; // clear pending
  USB->DEVICE.INTENSET.reg = USB_DEVICE_INTENSET_SOF | USB_DEVICE_INTENSET_EORST;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_0_IRQn);
  NVIC_EnableIRQ(USB_1_IRQn);
  NVIC_EnableIRQ(USB_2_IRQn);
  NVIC_EnableIRQ(USB_3_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_3_IRQn);
  NVIC_DisableIRQ(USB_2_IRQn);
  NVIC_DisableIRQ(USB_1_IRQn);
  NVIC_DisableIRQ(USB_0_IRQn);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  // Response with status first before changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

  // Wait for EP0 to finish before switching the address.
  while (USB->DEVICE.DeviceEndpoint[0].EPSTATUS.bit.BK1RDY == 1) {}

  USB->DEVICE.DADD.reg = USB_DEVICE_DADD_DADD(dev_addr) | USB_DEVICE_DADD_ADDEN;

  // Enable SUSPEND interrupt since the bus signal D+/D- are stable now.
  USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTENCLR_SUSPEND; // clear pending
  USB->DEVICE.INTENSET.reg = USB_DEVICE_INTENSET_SUSPEND;
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

  USB->DEVICE.CTRLB.bit.UPRSM = 1;
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
    ep->EPSTATUSSET.reg |= USB_DEVICE_EPSTATUSSET_BK1RDY;
    ep->EPINTFLAG.reg |= USB_DEVICE_EPINTFLAG_TRFAIL1;
  }

  return true;
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
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ1 | USB_DEVICE_EPSTATUSCLR_DTGLIN;
  } else {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ0 | USB_DEVICE_EPSTATUSCLR_DTGLOUT;
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
/*
 *------------------------------------------------------------------*/
/* USB_EORSM_DNRSM, USB_EORST_RST, USB_LPMSUSP_DDISC, USB_LPM_DCONN,
USB_MSOF, USB_RAMACER, USB_RXSTP_TXSTP_0, USB_RXSTP_TXSTP_1,
USB_RXSTP_TXSTP_2, USB_RXSTP_TXSTP_3, USB_RXSTP_TXSTP_4,
USB_RXSTP_TXSTP_5, USB_RXSTP_TXSTP_6, USB_RXSTP_TXSTP_7,
USB_STALL0_STALL_0, USB_STALL0_STALL_1, USB_STALL0_STALL_2,
USB_STALL0_STALL_3, USB_STALL0_STALL_4, USB_STALL0_STALL_5,
USB_STALL0_STALL_6, USB_STALL0_STALL_7, USB_STALL1_0, USB_STALL1_1,
USB_STALL1_2, USB_STALL1_3, USB_STALL1_4, USB_STALL1_5, USB_STALL1_6,
USB_STALL1_7, USB_SUSPEND, USB_TRFAIL0_TRFAIL_0, USB_TRFAIL0_TRFAIL_1,
USB_TRFAIL0_TRFAIL_2, USB_TRFAIL0_TRFAIL_3, USB_TRFAIL0_TRFAIL_4,
USB_TRFAIL0_TRFAIL_5, USB_TRFAIL0_TRFAIL_6, USB_TRFAIL0_TRFAIL_7,
USB_TRFAIL1_PERR_0, USB_TRFAIL1_PERR_1, USB_TRFAIL1_PERR_2,
USB_TRFAIL1_PERR_3, USB_TRFAIL1_PERR_4, USB_TRFAIL1_PERR_5,
USB_TRFAIL1_PERR_6, USB_TRFAIL1_PERR_7, USB_UPRSM, USB_WAKEUP */
void USB_0_Handler(void) {
  uint32_t int_status = USB->DEVICE.INTFLAG.reg & USB->DEVICE.INTENSET.reg;

  /*------------- Interrupt Processing -------------*/
  // SAMD doesn't distinguish between Suspend and Disconnect state.
  // Both condition will cause SUSPEND interrupt triggered.
  // To prevent being triggered when D+/D- are not stable, SUSPEND interrupt is only
  // enabled when we received SET_ADDRESS request and cleared on Bus Reset
  if ( int_status & USB_DEVICE_INTFLAG_SUSPEND )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_SUSPEND;

    // Enable wakeup interrupt
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_WAKEUP; // clear pending
    USB->DEVICE.INTENSET.reg = USB_DEVICE_INTFLAG_WAKEUP;

    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  // Wakeup interrupt is only enabled when we got suspended.
  // Wakeup interrupt will disable itself
  if ( int_status & USB_DEVICE_INTFLAG_WAKEUP )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_WAKEUP;

    // disable wakeup interrupt itself
    USB->DEVICE.INTENCLR.reg = USB_DEVICE_INTFLAG_WAKEUP;
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  if ( int_status & USB_DEVICE_INTFLAG_EORST )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_EORST;

    // Disable both suspend and wakeup interrupt
    USB->DEVICE.INTENCLR.reg = USB_DEVICE_INTFLAG_WAKEUP | USB_DEVICE_INTFLAG_SUSPEND;

    bus_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }

  // Setup packet received.
  maybe_handle_setup_packet();
}

/* USB_SOF_HSOF */
void USB_1_Handler(void) {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
}

void transfer_complete(uint8_t direction) {
    uint32_t epints = USB->DEVICE.EPINTSMRY.reg;
    for (uint8_t epnum = 0; epnum < USB_EPT_NUM; epnum++) {
        if ((epints & (1 << epnum)) == 0) {
            continue;
        }

        if (direction == TUSB_DIR_OUT && maybe_handle_setup_packet()) {
            continue;
        }
        UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

        UsbDeviceDescBank* bank = &sram_registers[epnum][direction];
        uint16_t total_transfer_size = bank->PCKSIZE.bit.BYTE_COUNT;

        uint8_t ep_addr = epnum;
        if (direction == TUSB_DIR_IN) {
            ep_addr |= TUSB_DIR_IN_MASK;
        }
        dcd_event_xfer_complete(0, ep_addr, total_transfer_size, XFER_RESULT_SUCCESS, true);

        // just finished status stage (total size = 0), prepare for next setup packet
        if (epnum == 0 && total_transfer_size == 0) {
            dcd_edpt_xfer(0, 0, _setup_packet, sizeof(_setup_packet));
        }

        if (direction == TUSB_DIR_IN) {
            ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;
        } else {
            ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;
        }
    }
}

// Bank zero is for OUT and SETUP transactions.
/* USB_TRCPT0_0, USB_TRCPT0_1, USB_TRCPT0_2,
USB_TRCPT0_3, USB_TRCPT0_4, USB_TRCPT0_5,
USB_TRCPT0_6, USB_TRCPT0_7 */
void USB_2_Handler(void) {
    transfer_complete(TUSB_DIR_OUT);
}

// Bank one is used for IN transactions.
/* USB_TRCPT1_0, USB_TRCPT1_1, USB_TRCPT1_2,
USB_TRCPT1_3, USB_TRCPT1_4, USB_TRCPT1_5,
USB_TRCPT1_6, USB_TRCPT1_7 */
void USB_3_Handler(void) {
    transfer_complete(TUSB_DIR_IN);
}

#endif
