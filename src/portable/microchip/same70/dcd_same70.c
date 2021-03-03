/* 
* The MIT License (MIT)
*
* Copyright (c) 2018, hathach (tinyusb.org)
* Copyright (c) 2021, HiFiPhile
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

#if CFG_TUSB_MCU == OPT_MCU_SAME70

#include "device/dcd.h"

#include "sam.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#ifndef USE_SOF
#  define USE_SOF     0
#endif

#ifndef USBHS_RAM_ADDR
#  define USBHS_RAM_ADDR        0xA0100000u
#endif

#define get_ep_fifo_ptr(ep, scale) (((TU_XSTRCAT(TU_STRCAT(uint, scale),_t) (*)[0x8000 / ((scale) / 8)])USBHS_RAM_ADDR)[(ep)])

#define EP_MAX            10

typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_packet_size;
  uint8_t interval;
} xfer_ctl_t;

xfer_ctl_t xfer_status[EP_MAX+1];

static const tusb_desc_endpoint_t ep0_desc =
{
  .bEndpointAddress = 0x00,
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
};

static tusb_speed_t get_speed(void);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix);
//------------------------------------------------------------------
// Device API
//------------------------------------------------------------------

// Initialize controller to device mode
void dcd_init (uint8_t rhport)
{
  // Enable USBPLL
  PMC->CKGR_UCKR = CKGR_UCKR_UPLLEN | CKGR_UCKR_UPLLCOUNT(0x3fU);
  // Wait until USB UTMI stabilize
  while (!(PMC->PMC_SR & PMC_SR_LOCKU));
  // Enable USB FS clk
  PMC->PMC_USB = PMC_USB_USBS | PMC_USB_USBDIV(10 - 1);
  PMC->PMC_SCER = PMC_SCER_USBCLK;
  dcd_connect(rhport);
}

// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ((IRQn_Type) ID_USBHS);
}

// Disable device interrupt
void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ((IRQn_Type) ID_USBHS);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  // DCD can only set address after status for this request is complete
  // do it at dcd_edpt0_status_complete()
  
  // Response with zlp status
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

// Wake up host
void dcd_remote_wakeup (uint8_t rhport)
{
  (void) rhport;
  USBHS->USBHS_DEVCTRL |= USBHS_DEVCTRL_RMWKUP;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  // Enable USB clock
  PMC->PMC_PCER1 = 1 << (ID_USBHS - 32);
  // Enable the USB controller in device mode
  USBHS->USBHS_CTRL = USBHS_CTRL_UIMOD | USBHS_CTRL_USBE;
  // Wait to unfreeze clock
  while(USBHS_SR_CLKUSABLE != (USBHS->USBHS_SR & USBHS_SR_CLKUSABLE));
  // Attach the device
  USBHS->USBHS_DEVCTRL &= ~USBHS_DEVCTRL_DETACH;
  // Enable the End Of Reset, Suspend & Wakeup interrupts
  USBHS->USBHS_DEVIER = (USBHS_DEVIER_EORSTES | USBHS_DEVIER_SUSPES | USBHS_DEVIER_WAKEUPES);
#if USE_SOF
  USBHS->USBHS_DEVIER = USBHS_DEVIER_SOFES;
#endif
  // Clear the End Of Reset, SOF & Wakeup interrupts
  USBHS->USBHS_DEVICR = (USBHS_DEVICR_EORSTC | USBHS_DEVICR_SOFC | USBHS_DEVICR_WAKEUPC);
  // Manually set the Suspend Interrupt
  USBHS->USBHS_DEVIFR |= USBHS_DEVIFR_SUSPS;
  // Ack the Wakeup Interrupt
  USBHS->USBHS_DEVICR = USBHS_DEVICR_WAKEUPC;
  // Freeze USB clock
  USBHS->USBHS_CTRL |= USBHS_CTRL_FRZCLK;
  __set_PRIMASK(irq_state);
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  // Disable all endpoints
  USBHS->USBHS_DEVEPT &= ~(0x3FF << USBHS_DEVEPT_EPEN0_Pos);
  // Unfreeze USB clock
  USBHS->USBHS_CTRL &= ~USBHS_CTRL_FRZCLK;
  // Wait to unfreeze clock
  while (USBHS_SR_CLKUSABLE != (USBHS->USBHS_SR & USBHS_SR_CLKUSABLE));
  // Clear all the pending interrupts
  USBHS->USBHS_DEVICR = USBHS_DEVICR_Msk;
  // Disable all interrupts
  USBHS->USBHS_DEVIDR = USBHS_DEVCTRL_UADD_Msk;
  // Detach the device
  USBHS->USBHS_DEVCTRL |= USBHS_DEVCTRL_DETACH;
  // Disable the device address
  USBHS->USBHS_DEVCTRL &=~(USBHS_DEVCTRL_ADDEN | USBHS_DEVCTRL_UADD_Msk);
  __set_PRIMASK(irq_state);
}

static tusb_speed_t get_speed(void)
{
  switch ((USBHS->USBHS_SR & USBHS_SR_SPEED_Msk) >> USBHS_SR_SPEED_Pos) {
  case USBHS_SR_SPEED_FULL_SPEED_Val:
  default:
    return TUSB_SPEED_FULL;
  case USBHS_SR_SPEED_HIGH_SPEED_Val:
    return TUSB_SPEED_HIGH;
  case USBHS_SR_SPEED_LOW_SPEED_Val:
    return TUSB_SPEED_LOW;
  }
}

static void dcd_ep_handler(uint8_t ep_ix)
{
  uint32_t int_status = USBHS->USBHS_DEVEPTISR[ep_ix];
  int_status &= USBHS->USBHS_DEVEPTIMR[ep_ix];
  uint16_t count = (USBHS->USBHS_DEVEPTISR[ep_ix] &
                    USBHS_DEVEPTISR_BYCT_Msk) >> USBHS_DEVEPTISR_BYCT_Pos;
  if (ep_ix == 0U) {
    if (int_status & USBHS_DEVEPTISR_CTRL_RXSTPI) {
      // Setup packet should always be 8 bytes. If not, ignore it, and try again.
      if (count == 8)
      {
        uint8_t *ptr = get_ep_fifo_ptr(0,8);
        dcd_event_setup_received(0, ptr, true);
      }
      // Acknowledge the interrupt 
      USBHS->USBHS_DEVEPTICR[0] = USBHS_DEVEPTICR_RXSTPIC;
    }
    if (int_status & USBHS_DEVEPTISR_RXOUTI) {
      xfer_ctl_t *xfer = &xfer_status[0];
      if (count) {
        uint8_t *ptr = get_ep_fifo_ptr(0,8);
        for (int i = 0; i < count; i++) {
          xfer->buffer[xfer->queued_len + i] = ptr[i];
        }
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }
      // Acknowledge the interrupt 
      USBHS->USBHS_DEVEPTICR[0] = USBHS_DEVEPTICR_RXOUTIC;
      if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len)) {
        // RX COMPLETE 
        dcd_event_xfer_complete(0, 0, xfer->queued_len, XFER_RESULT_SUCCESS, true);
        // Disable the interrupt 
        USBHS->USBHS_DEVEPTIDR[0] = USBHS_DEVEPTIDR_RXOUTEC;
        // Though the host could still send, we don't know.
      }
    }
    if (int_status & USBHS_DEVEPTISR_TXINI) {
      // Disable the interrupt 
      USBHS->USBHS_DEVEPTIDR[0] = USBHS_DEVEPTIDR_TXINEC;
      xfer_ctl_t * xfer = &xfer_status[EP_MAX];
      if ((xfer->total_len != xfer->queued_len)) {
        // TX not complete 
        dcd_transmit_packet(xfer, 0);
      }
      else {
        // TX complete 
        dcd_event_xfer_complete(0, (uint8_t)(0x80 + 0), xfer->total_len, XFER_RESULT_SUCCESS, true);
      }
    }
  }
  else {
    if (int_status & USBHS_DEVEPTISR_RXOUTI) { 
      xfer_ctl_t *xfer = &xfer_status[ep_ix];
      if (count) {
        uint8_t *ptr = get_ep_fifo_ptr(ep_ix,8);
        memcpy(xfer->buffer + xfer->queued_len, ptr, count);
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }
      // Acknowledge the interrupt 
      USBHS->USBHS_DEVEPTICR[ep_ix] = USBHS_DEVEPTICR_RXOUTIC;
      // Clear the FIFO control flag to receive more data.
      USBHS->USBHS_DEVEPTIDR[ep_ix] = USBHS_DEVEPTIDR_FIFOCONC;
      if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len)) {
        // RX COMPLETE 
        dcd_event_xfer_complete(0, ep_ix, xfer->queued_len, XFER_RESULT_SUCCESS, true);
        // Disable the interrupt 
        USBHS->USBHS_DEVEPTIDR[ep_ix] = USBHS_DEVEPTIDR_RXOUTEC;
        // Though the host could still send, we don't know.
      }
    }
    if (int_status & USBHS_DEVEPTISR_TXINI) {
      // Acknowledge the interrupt 
      USBHS->USBHS_DEVEPTICR[ep_ix] = USBHS_DEVEPTICR_TXINIC;
      xfer_ctl_t * xfer = &xfer_status[ep_ix];;
      if ((xfer->total_len != xfer->queued_len)) {
        // TX not complete 
        dcd_transmit_packet(xfer, ep_ix);
      }
      else  {
        // TX complete
        dcd_event_xfer_complete(0, (uint8_t)(0x80 + ep_ix), xfer->total_len, XFER_RESULT_SUCCESS, true);
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;
  uint32_t int_status = USBHS->USBHS_DEVISR;
  // End of reset interrupt 
  if (int_status & USBHS_DEVISR_EORST) {
    // Unfreeze USB clock 
    USBHS->USBHS_CTRL &= ~USBHS_CTRL_FRZCLK;
    while(USBHS_SR_CLKUSABLE != (USBHS->USBHS_SR & USBHS_SR_CLKUSABLE));
    // Reset all endpoints
    for (int ep_ix = 1; ep_ix < EP_MAX; ep_ix++) {
      USBHS->USBHS_DEVEPT |= 1 << (USBHS_DEVEPT_EPRST0_Pos + ep_ix);
      USBHS->USBHS_DEVEPT &=~(1 << (USBHS_DEVEPT_EPRST0_Pos + ep_ix));
    }
    dcd_edpt_open (0, &ep0_desc);
    // Acknowledge the End of Reset interrupt 
    USBHS->USBHS_DEVICR = USBHS_DEVICR_EORSTC;
    // Acknowledge the Wakeup interrupt 
    USBHS->USBHS_DEVICR = USBHS_DEVICR_WAKEUPC;
    // Acknowledge the suspend interrupt 
    USBHS->USBHS_DEVICR = USBHS_DEVICR_SUSPC;
    // Enable Suspend Interrupt 
    USBHS->USBHS_DEVIER = USBHS_DEVIER_SUSPES;
    
    dcd_event_bus_reset(rhport, get_speed(), true);
  }
  // End of Wakeup interrupt 
  if (int_status & USBHS_DEVISR_WAKEUP) {
    // Unfreeze USB clock 
    USBHS->USBHS_CTRL &= ~USBHS_CTRL_FRZCLK;
    // Wait to unfreeze clock 
    while (USBHS_SR_CLKUSABLE != (USBHS->USBHS_SR & USBHS_SR_CLKUSABLE));
    // Acknowledge the Wakeup interrupt 
    USBHS->USBHS_DEVICR = USBHS_DEVICR_WAKEUPC;
    // Disable Wakeup Interrupt 
    USBHS->USBHS_DEVIDR = USBHS_DEVIDR_WAKEUPEC;
    // Enable Suspend Interrupt 
    USBHS->USBHS_DEVIER = USBHS_DEVIER_SUSPES;
    
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }
  // Suspend interrupt 
  if (int_status & USBHS_DEVISR_SUSP) {
    // Unfreeze USB clock 
    USBHS->USBHS_CTRL &= ~USBHS_CTRL_FRZCLK;
    // Wait to unfreeze clock 
    while (USBHS_SR_CLKUSABLE != (USBHS->USBHS_SR & USBHS_SR_CLKUSABLE));
    // Acknowledge the suspend interrupt 
    USBHS->USBHS_DEVICR = USBHS_DEVICR_SUSPC;
    // Disable Suspend Interrupt 
    USBHS->USBHS_DEVIDR = USBHS_DEVIDR_SUSPEC;
    // Enable Wakeup Interrupt 
    USBHS->USBHS_DEVIER = USBHS_DEVIER_WAKEUPES;
    // Freeze USB clock 
    USBHS->USBHS_CTRL |= USBHS_CTRL_FRZCLK;
    
    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }
#if USE_SOF
  if(int_status & USBHS_DEVISR_SOF) {
    USBHS->USBHS_DEVICR = USBHS_DEVICR_SOFC;
    
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
#endif 
  // Endpoints interrupt 
  for (int ep_ix = 0; ep_ix < EP_MAX; ep_ix++) {
    if (int_status & (1 << (USBHS_DEVISR_PEP_0_Pos + ep_ix))) {
      dcd_ep_handler(ep_ix);
    }
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS )
  {
    uint8_t const dev_addr = (uint8_t) request->wValue;
    
    USBHS->USBHS_DEVCTRL |= dev_addr | USBHS_DEVCTRL_ADDEN;
  }
}

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(ep_desc->bEndpointAddress);
  uint16_t const epMaxPktSize = ep_desc->wMaxPacketSize.size;
  tusb_xfer_type_t const eptype = (tusb_xfer_type_t)ep_desc->bmAttributes.xfer;
  uint8_t fifoSize = 0;                       // FIFO size 
  uint16_t defaultEndpointSize = 8;           // Default size of Endpoint 
  // Find upper 2 power number of epMaxPktSize 
  if (epMaxPktSize) {
    while (defaultEndpointSize < epMaxPktSize) {
      fifoSize++;
      defaultEndpointSize <<= 1;
    }
  }
  xfer_status[epnum].max_packet_size = epMaxPktSize;
  
  USBHS->USBHS_DEVEPT |= 1 << (USBHS_DEVEPT_EPRST0_Pos + epnum);
  USBHS->USBHS_DEVEPT &=~(1 << (USBHS_DEVEPT_EPRST0_Pos + epnum));
    
  if (epnum == 0) {
    xfer_status[EP_MAX].max_packet_size = epMaxPktSize;
    // Enable the control endpoint - Endpoint 0 
    USBHS->USBHS_DEVEPT |= USBHS_DEVEPT_EPEN0;
    // Configure the Endpoint 0 configuration register 
    USBHS->USBHS_DEVEPTCFG[0] =
      (
       USBHS_DEVEPTCFG_EPSIZE(fifoSize) |
       USBHS_DEVEPTCFG_EPTYPE(TUSB_XFER_CONTROL) |
       USBHS_DEVEPTCFG_EPBK(USBHS_DEVEPTCFG_EPBK_1_BANK) |
       USBHS_DEVEPTCFG_ALLOC
      );
    USBHS->USBHS_DEVEPTIER[0] = USBHS_DEVEPTIER_RSTDTS;
    USBHS->USBHS_DEVEPTIDR[0] = USBHS_DEVEPTIDR_STALLRQC;
    if (USBHS_DEVEPTISR_CFGOK == (USBHS->USBHS_DEVEPTISR[0] & USBHS_DEVEPTISR_CFGOK)) {
      // Endpoint configuration is successful 
      USBHS->USBHS_DEVEPTIER[0] = USBHS_DEVEPTIER_RXSTPES;
      // Enable Endpoint 0 Interrupts 
      USBHS->USBHS_DEVIER = USBHS_DEVIER_PEP_0;
      return true;
    }
    else {
      // Endpoint configuration is not successful 
      return false;
    }
  }
  else {
    // Enable the endpoint 
    USBHS->USBHS_DEVEPT |= ((0x01 << epnum) << USBHS_DEVEPT_EPEN0_Pos);
    // Set up the maxpacket size, fifo start address fifosize
    // and enable the interrupt. CLear the data toggle. 
    USBHS->USBHS_DEVEPTCFG[epnum] =
      (
       USBHS_DEVEPTCFG_EPSIZE(fifoSize) |
       USBHS_DEVEPTCFG_EPTYPE(eptype) |
       USBHS_DEVEPTCFG_EPBK(USBHS_DEVEPTCFG_EPBK_1_BANK) |
       ((dir & 0x01) << USBHS_DEVEPTCFG_EPDIR_Pos)
      );
    
    if (eptype == TUSB_XFER_ISOCHRONOUS){
      USBHS->USBHS_DEVEPTCFG[epnum] |= USBHS_DEVEPTCFG_NBTRANS(1) | USBHS_DEVEPTCFG_EPBK_2_BANK; 
    }
    USBHS->USBHS_DEVEPTCFG[epnum] |= USBHS_DEVEPTCFG_ALLOC;
    USBHS->USBHS_DEVEPTIER[epnum] = USBHS_DEVEPTIER_RSTDTS;
    USBHS->USBHS_DEVEPTIDR[epnum] = USBHS_DEVEPTIDR_STALLRQC;
    if (USBHS_DEVEPTISR_CFGOK == (USBHS->USBHS_DEVEPTISR[epnum] & USBHS_DEVEPTISR_CFGOK)) {
      // Endpoint configuration is successful. Enable Endpoint Interrupts
      if (dir == TUSB_DIR_IN) {
        USBHS->USBHS_DEVEPTICR[epnum] = USBHS_DEVEPTICR_TXINIC;
		USBHS->USBHS_DEVEPTIER[epnum] = USBHS_DEVEPTIER_TXINES;
      }
      USBHS->USBHS_DEVIER = ((0x01 << epnum) << USBHS_DEVIER_PEP_0_Pos);
      return true;
    }
    else {
      // Endpoint configuration is not successful 
      return false;
    }
  }
}

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);
  
  if (len > xfer->max_packet_size) {
    len = xfer->max_packet_size;
  }
  
  uint8_t *ptr = get_ep_fifo_ptr(ep_ix,8);
  memcpy(ptr, xfer->buffer + xfer->queued_len, len);
  
  xfer->queued_len = (uint16_t)(xfer->queued_len + len);
  
  if (ep_ix == 0U) {
    // Control endpoint: clear the interrupt flag to send the data,
    // and re-enable the interrupts to trigger an interrupt at the
    // end of the transfer.
    USBHS->USBHS_DEVEPTICR[0] = USBHS_DEVEPTICR_TXINIC;
    USBHS->USBHS_DEVEPTIER[0] = USBHS_DEVEPTIER_TXINES;
  } else {
    // Other endpoint types: clear the FIFO control flag to send the data.
    USBHS->USBHS_DEVEPTIDR[ep_ix] = USBHS_DEVEPTIDR_FIFOCONC;
  }
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  
  xfer_ctl_t * xfer = &xfer_status[epnum];
  if(ep_addr == 0x80)
    xfer = &xfer_status[EP_MAX];
  
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  
  if (dir == TUSB_DIR_OUT){
    USBHS->USBHS_DEVEPTIER[epnum] = USBHS_DEVEPTIER_RXOUTES;
  }
  else {
    dcd_transmit_packet(xfer,epnum);
  }
  return true;
}

// Stall endpoint
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  USBHS->USBHS_DEVEPTIER[epnum] = USBHS_DEVEPTIER_STALLRQS;
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  USBHS->USBHS_DEVEPTIDR[epnum] = USBHS_DEVEPTIDR_STALLRQC;
  USBHS->USBHS_DEVEPTIER[epnum] = USBHS_HSTPIPIER_RSTDTS;
}

#endif
