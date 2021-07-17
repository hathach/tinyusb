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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_SAM3U

/* sam cmsis */
#include "sam.h"

#include "device/dcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#ifndef USE_SOF
#  define USE_SOF         0
#endif

/*
 * UDPHSEPT_NUMBER -> number of endpoints
 */

#ifndef USBHS_RAM_ADDR
#  define USBHS_RAM_ADDR        0xA0100000u
#endif

#define get_ep_fifo_ptr(ep, scale) (((TU_XSTRCAT(TU_STRCAT(uint, scale),_t) (*)[0x8000 / ((scale) / 8)])USBHS_RAM_ADDR)[(ep)])

typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_packet_size;
  uint8_t interval;
} xfer_ctl_t;

xfer_ctl_t xfer_status[UDPHSEPT_NUMBER+1];

static const tusb_desc_endpoint_t ep0_desc =
{
  .bEndpointAddress = 0x00,
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
};

static tusb_speed_t get_speed(void);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix);

/*------------------------------------------------------------------*/
/* Device API
 *------------------------------------------------------------------*/
void dcd_init(uint8_t rhport)
{
  (void) rhport;

  /* Atomic block */
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();

  /* Enable USBPLL */
  PMC->CKGR_UCKR = CKGR_UCKR_UPLLEN | CKGR_UCKR_UPLLCOUNT(0x3fU);
  /* Wait until UTMI PLL is locked */
  while(!(PMC->PMC_SR & PMC_SR_LOCKU));

  /* Enable USB clock */
  PMC->PMC_PCER0 = 1 << ID_UDPHS;

  /* Enable the USB controller (detached state) */
  UDPHS->UDPHS_CTRL = UDPHS_CTRL_DETACH | UDPHS_CTRL_EN_UDPHS;

  /* Enable the End Of Reset, Suspend & Wakeup interrupts */
  UDPHS->UDPHS_IEN = (UDPHS_IEN_ENDRESET | UDPHS_IEN_DET_SUSPD | UDPHS_IEN_WAKE_UP)
  #if USE_SOF
  /* Enable start of frame interrupt */
  UDPHS->UDPHS_IEN |= UDPHS_IEN_INT_SOF;
  #endif
  /* Clear the End Of Reset, Wakeup & SOF interrupts */
  UDPHS->UDPHS_CLRINT = (UDPHS_CLRINT_ENDRESET | UDPHS_CLRINT_WAKE_UP | UDPHS_CLRINT_INT_SOF);

  dcd_connect(rhport);

  /* End of atomic block */
  __set_PRIMASK(irq_state);
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;

  NVIC_EnableIRQ(UDPHS_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;

  NVIC_DisableIRQ(UDPHS_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) dev_addr;

  /*
   * DCD can only set address after status for this request is complete
   * do it at dcd_edpt0_status_complete()
   */

  /* Respond with zlp status */
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;

  UDPHS->UDPHS_CTRL |= UDPHS_CTRL_REWAKEUP;
}

/* Connect by enabling internal pull-up resistor on D+/D- */
void dcd_connect(uint8_t rhport)
{
  (void) rhport;

  /* Attach the device */
  UDPHS->UDPHS_CTRL &= ~UDPHS_CTRL_DETACH;
}

/* Disconnect by disabling internal pull-up resistor on D+/D- */
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;

  /* Disable all endpoints */
  for(uint8_t ept_ix = 0; ept_ix < UDPHSEPT_NUMBER; ept_ix++)
      UDPHS->UDPHS_EPT[ept_ix].UDPHS_EPTCTLDIS = UDPHS_EPTCTLDIS_EPT_DISABL;

  /* Detach the device */
  UDPHS->UDPHS_CTRL |= UDPHS_CTRL_DETACH;

  /* Disable the device address */
  UDPHS->UDPHS_CTRL &= ~(UDPHS_CTRL_DEV_ADDR_Msk | UDPHS_CTRL_FADDR_EN);
}

static tusb_speed_t get_speed(void)
{
  if(UDPHS->UDPHS_INTSTA & UDPHS_INTSTA_SPEED)
    return TUSB_SPEED_HIGH;
  else
    return TUSB_SPEED_FULL;
}

static void dcd_ep_handler(uint8_t ep_ix)
{
  uint32_t int_status = UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTSTA;
  int_status &= UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCTL;
  uint16_t count = (UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTSTA & UDPHS_EPTSTA_BYTE_COUNT_Msk) >> UDPHS_EPTSTA_BYTE_COUNT_Pos;

  if(ep_ix == 0)
  {
    if(int_status & UDPHS_EPTSTA_RX_SETUP)
	{
      /* Setup packet should always be 8 bytes. If not, ignore it, and try again. */
      if(count == 8)
      {
        uint8_t *ptr = get_ep_fifo_ptr(0, 8);
        dcd_event_setup_received(0, ptr, true);
      }

      /* Acknowledge the interrupt */
	  UDPHS->UDPHS_EPT[0].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_RX_SETUP;
    }
    if(int_status & UDPHS_EPTSTA_RXRDY_TXKL)
	{
      xfer_ctl_t *xfer = &xfer_status[0];

      if(count)
	  {
        uint8_t *ptr = get_ep_fifo_ptr(0, 8);
        memcpy(xfer->buffer + xfer->queued_len, ptr, count);
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }

      /* Acknowledge the interrupt */
	  UDPHS->UDPHS_EPT[0].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_RXRDY_TXKL;

      if((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
	  {
        /* RX complete */
        dcd_event_xfer_complete(0, 0, xfer->queued_len, XFER_RESULT_SUCCESS, true);

        /* Disable the interrupt */
		UDPHS->UDPHS_EPT[0].UDPHS_EPTCTLDIS = UDPHS_EPTCTLDIS_RXRDY_TXKL;

        /* Though the host could still send, we don't know. */
      }
    }
    if(int_status & UDPHS_EPTSTA_TXRDY)
	{
      /* Disable the interrupt */
	  UDPHS->UDPHS_EPT[0].UDPHS_EPTCTLDIS = UDPHS_EPTCTLDIS_TXRDY;

      xfer_ctl_t * xfer = &xfer_status[UDPHSEPT_NUMBER];

      if((xfer->total_len != xfer->queued_len))
	  {
        /* TX not complete */
        dcd_transmit_packet(xfer, 0);
      }
      else {
        /* TX complete */
        dcd_event_xfer_complete(0, (uint8_t)(0x80 + 0), xfer->total_len, XFER_RESULT_SUCCESS, true);
      }
    }
  }
  else
  {
    if(int_status & UDPHS_EPTSTA_RXRDY_TXKL)
	{
      xfer_ctl_t *xfer = &xfer_status[ep_ix];

      if(count)
	  {
        uint8_t *ptr = get_ep_fifo_ptr(ep_ix, 8);
        memcpy(xfer->buffer + xfer->queued_len, ptr, count);
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }

      /* Acknowledge the interrupt */
	  UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_RXRDY_TXKL;

      if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
	  {
        /* RX complete */
        dcd_event_xfer_complete(0, ep_ix, xfer->queued_len, XFER_RESULT_SUCCESS, true);

        /* Disable the interrupt */
		UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCTLDIS = UDPHS_EPTCTLDIS_RXRDY_TXKL;

        /* Though the host could still send, we don't know. */
      }
    }
    if(int_status & UDPHS_EPTSTA_TXRDY)
	{
      /* Acknowledge the interrupt */
	  UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCTLDIS = UDPHS_EPTCTLDIS_TXRDY;

      xfer_ctl_t * xfer = &xfer_status[ep_ix];

      if((xfer->total_len != xfer->queued_len))
	  {
        /* TX not complete */
        dcd_transmit_packet(xfer, ep_ix);
      }
      else
	  {
        /* TX complete */
        dcd_event_xfer_complete(0, (uint8_t)(0x80 + ep_ix), xfer->total_len, XFER_RESULT_SUCCESS, true);
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;

  uint32_t int_status = UDPHS->UDPHS_INTSTA;

  /* End of reset interrupt */
  if(int_status & UDPHS_INTSTA_ENDRESET)
  {
    /* Reset all endpoints */
    for(int ep_ix = 1; ep_ix < UDPHSEPT_NUMBER; ep_ix++)
	{
      UDPHS->UDPHS_EPTRST = (1 << ep_ix);
    }

    dcd_edpt_open (0, &ep0_desc);

    /* Acknowledge the End of Reset interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_CLRINT_ENDRESET;
    /* Acknowledge the Wakeup interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_CLRINT_WAKE_UP;
    /* Acknowledge the suspend interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_CLRINT_DET_SUSPD;
    /* Enable Suspend Interrupt */
    UDPHS->UDPHS_IEN = UDPHS_IEN_DET_SUSPD;

    dcd_event_bus_reset(rhport, get_speed(), true);
  }

  // End of Wakeup interrupt
  if(int_status & UDPHS_INTSTA_WAKE_UP)
  {
    /* Acknowledge the Wakeup interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_CLRINT_WAKE_UP;
    /* Disable the Wakeup interrupt */
    UDPHS->UDPHS_IEN &= ~UDPHS_IEN_WAKE_UP;
    /* Enable Suspend Interrupt */
    UDPHS->UDPHS_IEN = UDPHS_IEN_DET_SUSPD;

    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  // Suspend interrupt
  if(int_status & UDPHS_INTSTA_DET_SUSPD)
  {
	/* Acknowledge the suspend interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_DET_SUSPD;
	/* Disable Suspend Interrupt */
    UDPHS->UDPHS_IEN &= ~UDPHS_IEN_DET_SUSPD;
    /* Enable the Wakeup interrupt */
    UDPHS->UDPHS_IEN |= UDPHS_IEN_WAKE_UP;

    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  #if USE_SOF
  if(int_status & UDPHS_INTSTA_INT_SOF)
  {
    /* Acknowledge the Wakeup interrupt */
    UDPHS->UDPHS_CLRINT = UDPHS_CLRINT_INT_SOF;

    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
  #endif

  // Endpoints interrupt
  for(int ept_ix = 0; ept_ix < UDPHSEPT_NUMBER; ept_ix++)
  {
    if(int_status & (1 << (8 + ept_ix)))
	{
      dcd_ep_handler(ept_ix);
    }
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

/*
 * Invoked when a control transfer's status stage is complete.
 * May help DCD to prepare for next control transfer, this API is optional.
 */
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  if(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
     request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
     request->bRequest == TUSB_REQ_SET_ADDRESS )
  {
    uint8_t const dev_addr = (uint8_t)equest->wValue;

    UDPHS->UDPHS_CTRL |= UDPHS_CTRL_DEV_ADDR(dev_addr) | UDPHS_CTRL_FADDR_EN;
  }
}

/* Configure endpoint's registers according to descriptor */
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(ep_desc->bEndpointAddress);
  uint16_t const epMaxPktSize = ep_desc->wMaxPacketSize.size;
  tusb_xfer_type_t const eptype = (tusb_xfer_type_t)ep_desc->bmAttributes.xfer;
  uint8_t fifoSize = 0;                       /* FIFO size */
  uint16_t defaultEndpointSize = 8;           /* Default size of Endpoint */

  /* Find upper 2 power number of epMaxPktSize */
  if(epMaxPktSize)
  {
    while(defaultEndpointSize < epMaxPktSize)
    {
      fifoSize++;
      defaultEndpointSize <<= 1;
    }
  }

  xfer_status[epnum].max_packet_size = epMaxPktSize;

  UDPHS->UDPHS_EPTRST = (1 << epnum);

  if(epnum == 0)
  {
    xfer_status[UDPHSEPT_NUMBER].max_packet_size = epMaxPktSize;

    /* Enable the control endpoint - Endpoint 0 */
    UDPHS->UDPHS_EPT[0].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_EPT_ENABL;

    /* Configure the Endpoint 0 configuration register */
    USBHS->UDPHS_EPT[0].UDPHS_EPTCFG = (UDPHS_EPTCFG_EPT_SIZE(fifoSize) |
                                            UDPHS_EPTCFG_EPT_TYPE(TUSB_XFER_CONTROL) |
                                            UDPHS_EPTCFG_BK_NUMBER_1 |
                                            UDPHS_EPTCFG_EPT_MAPD);

    /* Clear STALL request */
    UDPHS->UDPHS_EPT[0].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_FRCESTALL;
    /* Clear the PID data of the current bank */
    UDPHS->UDPHS_EPT[0].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_TOGGLESQ;

	/* Enable RX_SETUP interrupt */
	UDPHS->UDPHS_EPT[0].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_RX_SETUP;

    /* Enable endpoint 0 interrupts */
    UDPHS->UDPHS_IEN |= UDPHS_IEN_EPT_0;

    return true;
  }
  else
  {
    /* Enable the control endpoint - Endpoint 0 */
    UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_EPT_ENABL;

    /* Configure the Endpoint 0 configuration register */
    USBHS->UDPHS_EPT[epnum].UDPHS_EPTCFG = (UDPHS_EPTCFG_EPT_SIZE(fifoSize) |
                                            UDPHS_EPTCFG_EPT_TYPE(eptype) |
                                            UDPHS_EPTCFG_BK_NUMBER_1 |
											((dir & 0x01) << 3));

    if(eptype == TUSB_XFER_ISOCHRONOUS)
	{
      USBHS->UDPHS_EPT[epnum].UDPHS_EPTCFG |= UDPHS_EPTCFG_NB_TRANS(1) | UDPHS_EPTCFG_BK_NUMBER_2;
    }

    USBHS->UDPHS_EPT[epnum].UDPHS_EPTCFG |= UDPHS_EPTCFG_EPT_MAPD;

    /* Clear STALL request */
    UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_FRCESTALL;
    /* Clear the PID data of the current bank */
    UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_TOGGLESQ;

	/* Enable Endpoint Interrupts */
	if(dir == TUSB_DIR_IN)
	{
	  UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_TXRDY;
	}

    /* Enable endpoint interrupts */
    UDPHS->UDPHS_IEN |= (1 << (8 + epnum));

	return true;
  }
}

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix)
{

  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);

  if(len > xfer->max_packet_size)
  {
    len = xfer->max_packet_size;
  }

  uint8_t *ptr = get_ep_fifo_ptr(ep_ix, 8);
  memcpy(ptr, xfer->buffer + xfer->queued_len, len);

  xfer->queued_len = (uint16_t)(xfer->queued_len + len);

  if(ep_ix == 0)
  {
    /*
	 * Control endpoint: clear the interrupt flag to send the data,
     * and re-enable the interrupts to trigger an interrupt at the
     * end of the transfer.
     */
	UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTSETSTA = UDPHS_EPTSETSTA_TXRDY;
    UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_TXRDY;
  }
  else
  {
    /* Other endpoint types: clear the FIFO control flag to send the data. */
	UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTSETSTA = UDPHS_EPTSETSTA_TXRDY;
  }
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = &xfer_status[epnum];
  if(ep_addr == 0x80)
    xfer = &xfer_status[UDPHSEPT_NUMBER];

  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  if(dir == TUSB_DIR_OUT)
  {
    UDPHS->UDPHS_EPT[ep_ix].UDPHS_EPTCTLENB = UDPHS_EPTCTLENB_RXRDY_TXKL;
  }
  else
  {
    dcd_transmit_packet(xfer,epnum);
  }
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  /* enable request STALL */
  UDPHS->UDPHS_EPT[epnum].UDPHS_EPTSETSTA = UDPHS_EPTSETSTA_FRCESTALL;
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  /* clear STALL request */
  UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_FRCESTALL;
  /* clear the PID data of the current bank */
  UDPHS->UDPHS_EPT[epnum].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_TOGGLESQ;
}

#endif
