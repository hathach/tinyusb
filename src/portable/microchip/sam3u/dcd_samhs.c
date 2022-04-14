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

#if CFG_TUD_ENABLED && TU_CHECK_MCU(OPT_MCU_SAM3U)

#include "device/dcd.h"
#include "sam.h"

#if TU_CHECK_MCU(OPT_MCU_SAM3U)
#include "samhs_sam3u.h"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// SAMHS registers
#define SAMHS_REG         ((samhs_reg_t*) SAMHS_BASE_REG)

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#ifndef USE_SOF
#  define USE_SOF         0
#endif

// Dual bank can imporve performance, but need 2 times bigger packet buffer
// Only 4KB packet buffer, use with caution !
// Enable in FS mode as packets are smaller
#ifndef USE_DUAL_BANK
#  if TUD_OPT_HIGH_SPEED
#    define USE_DUAL_BANK   0
#  else
#    define USE_DUAL_BANK   1
#  endif
#endif

#define EP_GET_FIFO_PTR(ep, scale) (((TU_XSTRCAT(TU_STRCAT(uint, scale),_t) (*)[0x8000 / ((scale) / 8)])FIFO_RAM_ADDR)[(ep)])

// DMA Channel Transfer Descriptor
typedef struct {
  volatile uint32_t next_desc;
  volatile uint32_t buff_addr;
  volatile uint32_t chnl_ctrl;
  uint32_t padding;
} dma_desc_t;

// Transfer control context
typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_packet_size;
  uint8_t interval;
  tu_fifo_t * fifo;
} xfer_ctl_t;

static tusb_speed_t get_speed(void);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix);

// DMA descriptors shouldn't be placed in ITCM !
CFG_TUSB_MEM_SECTION static dma_desc_t dma_desc[6];

static xfer_ctl_t xfer_status[EP_MAX];

static const tusb_desc_endpoint_t ep0_desc =
{
  .bEndpointAddress = 0x00,
  .wMaxPacketSize   = CFG_TUD_ENDPOINT0_SIZE,
};

TU_ATTR_ALWAYS_INLINE static inline void CleanInValidateCache(uint32_t *addr, int32_t size)
{
  if (SCB->CCR & SCB_CCR_DC_Msk)
  {
    SCB_CleanInvalidateDCache_by_Addr(addr, size);
  }
  else
  {
    __DSB();
    __ISB();
  }
}
//------------------------------------------------------------------
// Device API
//------------------------------------------------------------------

// Initialize controller to device mode
void dcd_init (uint8_t rhport)
{
  dcd_connect(rhport);
}

// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
//   NVIC_EnableIRQ((IRQn_Type) UDPHS_IRQn);
  NVIC_EnableIRQ((IRQn_Type) ID_USBHS);
}

// Disable device interrupt
void dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
//   NVIC_DisableIRQ((IRQn_Type) UDPHS_IRQn);
  NVIC_DisableIRQ((IRQn_Type) ID_USBHS);
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) dev_addr;
  // DCD can only set address after status for this request is complete
  // do it at dcd_edpt0_status_complete()

  // Response with zlp status
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

// Wake up host
void dcd_remote_wakeup (uint8_t rhport)
{
  (void) rhport;
  SAMHS_REG->SAMHS_DEV_CTRL |= SAMHS_DEV_CTRL_REWAKEUP;
}

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  dcd_int_disable(rhport);
  // Enable the USB controller in device mode
  SAMHS_REG->SAMHS_DEV_CTRL = SAMHS_DEV_CTRL_EN_SAMHS;

#if TUD_OPT_HIGH_SPEED
  SAMHS_REG->SAMHS_DEV_TST &= ~SAMHS_DEV_TST_SPEED_CFG_Msk;
#else
  SAMHS_REG->SAMHS_DEV_TST |= SAMHS_DEV_TST_SPEED_CFG_FULL_SPEED;
#endif
  // Enable the End Of Reset, Suspend & Wakeup interrupts
  SAMHS_REG->SAMHS_DEV_IEN = (SAMHS_DEV_IEN_ENDRESET | SAMHS_DEV_IEN_DET_SUSPD | SAMHS_DEV_IEN_WAKE_UP);
#if USE_SOF
  SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_INT_SOF;
#endif
  // Clear the End Of Reset, SOF & Wakeup interrupts
  SAMHS_REG->SAMHS_DEV_CLRINT = (SAMHS_DEV_CLRINT_ENDRESET | SAMHS_DEV_CLRINT_INT_SOF | SAMHS_DEV_CLRINT_WAKE_UP);

  // Ack the Wakeup Interrupt
  SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_WAKE_UP;
  // Attach the device
  SAMHS_REG->SAMHS_DEV_CTRL &= ~SAMHS_DEV_CTRL_DETACH;
}

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  dcd_int_disable(rhport);
  // Disable all endpoints
  for(size_t i = 0; i < EP_MAX; i++)
  {
	SAMHS_REG->SAMHS_DEV_EPT[i].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_EPT_DISABL;
  }
  // Clear all the pending interrupts
  SAMHS_REG->SAMHS_DEV_CLRINT = 0xF;
  // Disable all interrupts
  SAMHS_REG->SAMHS_DEV_IEN = 0;
  // Detach the device
  SAMHS_REG->SAMHS_DEV_CTRL |= SAMHS_DEV_CTRL_DETACH;
  // Disable the device address
  SAMHS_REG->SAMHS_DEV_CTRL &= ~(SAMHS_DEV_CTRL_FADDR_EN | SAMHS_DEV_CTRL_DEV_ADDR_Msk);
}

static tusb_speed_t get_speed(void)
{
  if (SAMHS_REG->SAMHS_DEV_INTSTA & SAMHS_DEV_INTSTA_SPEED) {
    return TUSB_SPEED_HIGH;
  }

  return TUSB_SPEED_FULL;
}

static void dcd_ep_handler(uint8_t ep_ix)
{
  uint32_t int_status = SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTSTA;
  int_status &= SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCTL;

  uint16_t count = (SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTSTA &
                    SAMHS_DEV_EPTSTA_BYTE_COUNT_Msk) >> SAMHS_DEV_EPTSTA_BYTE_COUNT_Pos;
  xfer_ctl_t *xfer = &xfer_status[ep_ix];

  if (ep_ix == 0U)
  {
    static uint8_t ctrl_dir;

    if (int_status & SAMHS_DEV_EPTSTA_RX_SETUP) // Received SETUP
    {
      ctrl_dir = (SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTSTA & SAMHS_DEV_EPTSTA_CURBK_CTLDIR_Msk) >> SAMHS_DEV_EPTSTA_CURBK_CTLDIR_Pos;
      // Setup packet should always be 8 bytes. If not, ignore it, and try again.
      if (count == 8)
      {
        uint8_t *ptr = EP_GET_FIFO_PTR(0,8);
        dcd_event_setup_received(0, ptr, true);
      }
      // Ack and disable SETUP interrupt
      SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_RX_SETUP;
      SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_RX_SETUP;
    }
    if (int_status & SAMHS_DEV_EPTSTA_RXRDY_TXKL) // Received OUT
    {
      uint8_t *ptr = EP_GET_FIFO_PTR(0,8);
      
      if (count && xfer->total_len)
      {
        uint16_t remain = xfer->total_len - xfer->queued_len;
        if (count > remain)
        {
          count = remain;
        }
        if (xfer->buffer)
        {
          memcpy(xfer->buffer + xfer->queued_len, ptr, count);
        } else 
        {
          tu_fifo_write_n(xfer->fifo, ptr, count);
        }
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }
      // Acknowledge the interrupt
      SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_RXRDY_TXKL;
      if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
      {
        // RX COMPLETE
        dcd_event_xfer_complete(0, 0, xfer->queued_len, XFER_RESULT_SUCCESS, true);
        // Disable the interrupt
      	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_RXRDY_TXKL;
        // Re-enable SETUP interrupt
        if (ctrl_dir == 1)
        {
      	  SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RX_SETUP;
        }
      }
    }
    if (int_status & SAMHS_DEV_EPTSTA_TX_COMPLT)
    {
      // Disable the interrupt
      SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_TX_COMPLT;
      if ((xfer->total_len != xfer->queued_len))
      {
        // TX not complete
        dcd_transmit_packet(xfer, 0);
      } else 
      {
        // TX complete
        dcd_event_xfer_complete(0, 0x80 + 0, xfer->total_len, XFER_RESULT_SUCCESS, true);
        // Re-enable SETUP interrupt
        if (ctrl_dir == 0)
        {
      	  SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RX_SETUP;
        }
      }
    }
  } else 
  {
    if (int_status & SAMHS_DEV_EPTSTA_RXRDY_TXKL)
    {
      if (count && xfer->total_len)
      {
        uint16_t remain = xfer->total_len - xfer->queued_len;
        if (count > remain)
        {
          count = remain;
        }
        uint8_t *ptr = EP_GET_FIFO_PTR(ep_ix,8);
        if (xfer->buffer)
        {
          memcpy(xfer->buffer + xfer->queued_len, ptr, count);
        } else {
          tu_fifo_write_n(xfer->fifo, ptr, count);
        }
        xfer->queued_len = (uint16_t)(xfer->queued_len + count);
      }

      // Acknowledge the interrupt
      SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_RXRDY_TXKL;
      if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
      {
        // RX COMPLETE
        dcd_event_xfer_complete(0, ep_ix, xfer->queued_len, XFER_RESULT_SUCCESS, true);
        // Disable the interrupt
        SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_RXRDY_TXKL;
        // Though the host could still send, we don't know.
      }
    }
    if (int_status & SAMHS_DEV_EPTSTA_TXRDY)
    {
      // Acknowledge the interrupt
    //   SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_TXRDY; // !TODO: check this
      if ((xfer->total_len != xfer->queued_len))
      {
        // TX not complete
        dcd_transmit_packet(xfer, ep_ix);
      } else 
      {
        // TX complete
        dcd_event_xfer_complete(0, 0x80 + ep_ix, xfer->total_len, XFER_RESULT_SUCCESS, true);
        // Disable the interrupt
        SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCTLDIS = SAMHS_DEV_EPTCTLDIS_TXRDY;
      }
    }
  }
}

static void dcd_dma_handler(uint8_t ep_ix)
{
  uint32_t status = SAMHS_REG->SAMHS_DEV_DMA[ep_ix - 1].SAMHS_DEV_DMASTATUS;
  if (status & SAMHS_DEV_DMASTATUS_CHANN_ENB)
  {
    return; // Ignore EOT_STA interrupt
  }
  // Disable DMA interrupt
  SAMHS_REG->SAMHS_DEV_IEN &= ~(SAMHS_DEV_IEN_DMA_1 << (ep_ix - 1));

  xfer_ctl_t *xfer = &xfer_status[ep_ix];
  uint16_t count = xfer->total_len - ((status & SAMHS_DEV_DMASTATUS_BUFF_COUNT_Msk) >> SAMHS_DEV_DMASTATUS_BUFF_COUNT_Pos);
  if(SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCFG & SAMHS_DEV_EPTCFG_EPT_DIR)
  {
    dcd_event_xfer_complete(0, 0x80 + ep_ix, count, XFER_RESULT_SUCCESS, true);
  } else 
  {
    dcd_event_xfer_complete(0, ep_ix, count, XFER_RESULT_SUCCESS, true);
  }
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;
  uint32_t int_status = SAMHS_REG->SAMHS_DEV_INTSTA;
  int_status &= SAMHS_REG->SAMHS_DEV_IEN;
  // End of reset interrupt
  if (int_status & SAMHS_DEV_INTSTA_ENDRESET)
  {
    // Reset all endpoints
    for (int ep_ix = 1; ep_ix < EP_MAX; ep_ix++)
    {
       SAMHS_REG->SAMHS_DEV_EPTRST = (SAMHS_DEV_EPTRST_EPT_0 << ep_ix);
    }
    dcd_edpt_open (0, &ep0_desc);
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_ENDRESET;
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_WAKE_UP;
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_DET_SUSPD;
    SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_DET_SUSPD;

    dcd_event_bus_reset(rhport, get_speed(), true);
  }
  // End of Wakeup interrupt
  if (int_status & SAMHS_DEV_INTSTA_WAKE_UP)
  {
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_WAKE_UP;
    SAMHS_REG->SAMHS_DEV_IEN &= ~(SAMHS_DEV_IEN_WAKE_UP);
    SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_DET_SUSPD;

    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }
  // Suspend interrupt
  if (int_status & SAMHS_DEV_INTSTA_DET_SUSPD)
  {
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_DET_SUSPD;
    SAMHS_REG->SAMHS_DEV_IEN &= ~(SAMHS_DEV_IEN_DET_SUSPD);
    SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_WAKE_UP;

    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }
#if USE_SOF
  if(int_status & SAMHS_DEV_INTSTA_INT_SOF)
  {
    SAMHS_REG->SAMHS_DEV_CLRINT = SAMHS_DEV_CLRINT_INT_SOF;

    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
#endif
  // Endpoints interrupt
  for (int ep_ix = 0; ep_ix < EP_MAX; ep_ix++)
  {
    if (int_status & (SAMHS_DEV_INTSTA_EPT_0 << ep_ix))
    {
      dcd_ep_handler(ep_ix);
    }
  }
  // Endpoints DMA interrupt
  for (int ep_ix = 0; ep_ix < EP_MAX; ep_ix++)
  {
    if (EP_DMA_SUPPORT(ep_ix))
    {
      if (int_status & (SAMHS_DEV_INTSTA_DMA_1 << (ep_ix - 1)))
      {
        dcd_dma_handler(ep_ix);
      }
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

    SAMHS_REG->SAMHS_DEV_CTRL |= SAMHS_DEV_CTRL_DEV_ADDR(dev_addr) | SAMHS_DEV_CTRL_FADDR_EN;
  }
}

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_desc->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(ep_desc->bEndpointAddress);
  uint16_t const epMaxPktSize = tu_edpt_packet_size(ep_desc);
  tusb_xfer_type_t const eptype = (tusb_xfer_type_t)ep_desc->bmAttributes.xfer;
  uint8_t fifoSize = 0;                       // FIFO size
  uint16_t defaultEndpointSize = 8;           // Default size of Endpoint
  // Find upper 2 power number of epMaxPktSize
  if (epMaxPktSize)
  {
    while (defaultEndpointSize < epMaxPktSize)
    {
      fifoSize++;
      defaultEndpointSize <<= 1;
    }
  }
  xfer_status[epnum].max_packet_size = epMaxPktSize;

  SAMHS_REG->SAMHS_DEV_EPTRST = (SAMHS_DEV_EPTRST_EPT_0 << epnum);

  if (epnum == 0)
  {
    // Enable the control endpoint - Endpoint 0
    SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLENB |= SAMHS_DEV_EPTCTLENB_EPT_ENABL;
    // Configure the Endpoint 0 configuration register
	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCFG = (SAMHS_DEV_EPTCFG_EPT_SIZE(fifoSize) |
                                            SAMHS_DEV_EPTCFG_EPT_TYPE(TUSB_XFER_CONTROL) |
                                            SAMHS_DEV_EPTCFG_BK_NUMBER_1);


	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_TOGGLESQ;
	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_FRCESTALL;

    if (SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCFG & SAMHS_DEV_EPTCFG_EPT_MAPD)
    {
      // Endpoint configuration is successful
      SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RX_SETUP;
      // Enable Endpoint 0 Interrupts
      SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_EPT_0;
      return true;
    } else 
    {
      // Endpoint configuration is not successful
      return false;
    }
  } else 
  {
    // Enable the endpoint
    SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCTLENB |= SAMHS_DEV_EPTCTLENB_EPT_ENABL;
    // Set up the maxpacket size, fifo start address fifosize
    // and enable the interrupt. CLear the data toggle.
    // AUTOSW is needed for DMA ack !
	SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCFG = (SAMHS_DEV_EPTCFG_EPT_SIZE(fifoSize) |
                                            SAMHS_DEV_EPTCFG_EPT_TYPE(eptype) |
                                            SAMHS_DEV_EPTCFG_BK_NUMBER_1 |
											((dir & 0x01) << 3));
    if (eptype == TUSB_XFER_ISOCHRONOUS)
    {
      SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCFG |= SAMHS_DEV_EPTCFG_NB_TRANS(1);
    }
#if USE_DUAL_BANK
    if (eptype == TUSB_XFER_ISOCHRONOUS || eptype == TUSB_XFER_BULK)
    {
      SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCFG |= SAMHS_DEV_EPTCFG_BK_NUMBER_2;
    }
#endif
	SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_TOGGLESQ;
	SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_FRCESTALL;
    if (SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCFG & SAMHS_DEV_EPTCFG_EPT_MAPD)
    {
      SAMHS_REG->SAMHS_DEV_IEN |= (SAMHS_DEV_IEN_EPT_0 << epnum);
      return true;
    } else 
    {
      // Endpoint configuration is not successful
      return false;
    }
  }
}

void dcd_edpt_close_all (uint8_t rhport)
{
  (void) rhport;
  // TODO implement dcd_edpt_close_all()
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum  = tu_edpt_number(ep_addr);

  // Disable endpoint interrupt
  SAMHS_REG->SAMHS_DEV_IEN &= ~(SAMHS_DEV_IEN_EPT_0 << epnum);
  // Disable EP
  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCTLENB &= ~(SAMHS_DEV_EPTCTLENB_EPT_ENABL);
}

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint8_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);
  if (len)
  {
    if (len > xfer->max_packet_size)
    {
      len = xfer->max_packet_size;
    }
    uint8_t *ptr = EP_GET_FIFO_PTR(ep_ix,8);
    if(xfer->buffer)
    {
      memcpy(ptr, xfer->buffer + xfer->queued_len, len);
    }
    else 
    {
      tu_fifo_read_n(xfer->fifo, ptr, len);
    }
    __DSB();
    __ISB();
    xfer->queued_len = (uint16_t)(xfer->queued_len + len);
  }
  if (ep_ix == 0U)
  {
    // Control endpoint: clear the interrupt flag to send the data
	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTSETSTA = SAMHS_DEV_EPTSETSTA_TXRDY;
  } else 
  {
    // Other endpoint types: clear the FIFO control flag to send the data
	SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTSETSTA = SAMHS_DEV_EPTSETSTA_TXRDY;
  }
  SAMHS_REG->SAMHS_DEV_EPT[ep_ix].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_TX_COMPLT;
}

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = &xfer_status[epnum];

  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->fifo = NULL;
  
  if (EP_DMA_SUPPORT(epnum) && total_bytes != 0)
  {
    // Force the CPU to flush the buffer. We increase the size by 32 because the call aligns the
    // address to 32-byte boundaries.
    CleanInValidateCache((uint32_t*) tu_align((uint32_t) buffer, 4), total_bytes + 31);
    uint32_t udd_dma_ctrl = total_bytes << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos;
    if (dir == TUSB_DIR_OUT)
    {
      udd_dma_ctrl |= SAMHS_DEV_DMACONTROL_END_TR_IT | SAMHS_DEV_DMACONTROL_END_TR_EN;
    } else {
      udd_dma_ctrl |= SAMHS_DEV_DMACONTROL_END_B_EN;
    }
    SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMAADDRESS = (uint32_t)buffer;
    udd_dma_ctrl |= SAMHS_DEV_DMACONTROL_END_BUFFIT | SAMHS_DEV_DMACONTROL_CHANN_ENB;
    // Disable IRQs to have a short sequence
    // between read of EOT_STA and DMA enable
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    if (!(SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMASTATUS & SAMHS_DEV_DMASTATUS_END_TR_ST))
    {
      SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMACONTROL = udd_dma_ctrl;
      SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_DMA_1 << (epnum - 1);
      __set_PRIMASK(irq_state);
      return true;
    }
    __set_PRIMASK(irq_state);

    // Here a ZLP has been recieved
    // and the DMA transfer must be not started.
    // It is the end of transfer
    return false;
  } else 
  {
    if (dir == TUSB_DIR_OUT)
    {
  	  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RXRDY_TXKL;
    } else 
    {
      dcd_transmit_packet(xfer,epnum);
    }
  }
  return true;
}

// The number of bytes has to be given explicitly to allow more flexible control of how many
// bytes should be written and second to keep the return value free to give back a boolean
// success message. If total_bytes is too big, the FIFO will copy only what is available
// into the USB buffer!
bool dcd_edpt_xfer_fifo (uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = &xfer_status[epnum];
  if(epnum == 0x80)
    xfer = &xfer_status[EP_MAX];

  xfer->buffer = NULL;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->fifo = ff;

  if (EP_DMA_SUPPORT(epnum) && total_bytes != 0)
  {
    tu_fifo_buffer_info_t info;
    uint32_t udd_dma_ctrl_lin = SAMHS_DEV_DMACONTROL_CHANN_ENB;
    uint32_t udd_dma_ctrl_wrap = SAMHS_DEV_DMACONTROL_CHANN_ENB | SAMHS_DEV_DMACONTROL_END_BUFFIT;
    if (dir == TUSB_DIR_OUT)
    {
      tu_fifo_get_write_info(ff, &info);
      udd_dma_ctrl_lin |= SAMHS_DEV_DMACONTROL_END_TR_IT | SAMHS_DEV_DMACONTROL_END_TR_EN;
      udd_dma_ctrl_wrap |= SAMHS_DEV_DMACONTROL_END_TR_IT | SAMHS_DEV_DMACONTROL_END_TR_EN;
    } else {
      tu_fifo_get_read_info(ff, &info);
      if(info.len_wrap == 0)
      {
        udd_dma_ctrl_lin |= SAMHS_DEV_DMACONTROL_END_B_EN;
      }
      udd_dma_ctrl_wrap |= SAMHS_DEV_DMACONTROL_END_B_EN;
    }

    // Clean invalidate cache of linear part
    CleanInValidateCache((uint32_t*) tu_align((uint32_t) info.ptr_lin, 4), info.len_lin + 31);
    
    SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMAADDRESS = (uint32_t)info.ptr_lin;
    if (info.len_wrap)
    {
      // Clean invalidate cache of wrapped part
      CleanInValidateCache((uint32_t*) tu_align((uint32_t) info.ptr_wrap, 4), info.len_wrap + 31);
      
      dma_desc[epnum - 1].next_desc = 0;
      dma_desc[epnum - 1].buff_addr = (uint32_t)info.ptr_wrap;
      dma_desc[epnum - 1].chnl_ctrl =
        udd_dma_ctrl_wrap | (info.len_wrap << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos);
      // Clean cache of wrapped DMA descriptor
      CleanInValidateCache((uint32_t*)&dma_desc[epnum - 1], sizeof(dma_desc_t));
      
      udd_dma_ctrl_lin |= SAMHS_DEV_DMASTATUS_DESC_LDST;
      SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMANXTDSC = (uint32_t)&dma_desc[epnum - 1];
    } else {
      udd_dma_ctrl_lin |= SAMHS_DEV_DMACONTROL_END_BUFFIT;
    }
    udd_dma_ctrl_lin |= (info.len_lin << SAMHS_DEV_DMACONTROL_BUFF_LENGTH_Pos);
    // Disable IRQs to have a short sequence
    // between read of EOT_STA and DMA enable
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    if (!(SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMASTATUS & SAMHS_DEV_DMASTATUS_END_TR_ST))
    {
      SAMHS_REG->SAMHS_DEV_DMA[epnum - 1].SAMHS_DEV_DMACONTROL = udd_dma_ctrl_lin;
      SAMHS_REG->SAMHS_DEV_IEN |= SAMHS_DEV_IEN_DMA_1 << (epnum - 1);
      __set_PRIMASK(irq_state);
      return true;
    }
    __set_PRIMASK(irq_state);

    // Here a ZLP has been recieved
    // and the DMA transfer must be not started.
    // It is the end of transfer
    return false;
  } else
  {
    if (dir == TUSB_DIR_OUT)
    {
  	  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RXRDY_TXKL;
    } else 
    {
      dcd_transmit_packet(xfer,epnum);
    }
  }
  return true;
}

// Stall endpoint
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTSETSTA = SAMHS_DEV_EPTSETSTA_FRCESTALL;
  // Re-enable SETUP interrupt
  if (epnum == 0)
  {
	SAMHS_REG->SAMHS_DEV_EPT[0].SAMHS_DEV_EPTCTLENB = SAMHS_DEV_EPTCTLENB_RX_SETUP;
  }
}

// clear stall, data toggle is also reset to DATA0
void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_FRCESTALL;
  SAMHS_REG->SAMHS_DEV_EPT[epnum].SAMHS_DEV_EPTCLRSTA = SAMHS_DEV_EPTCLRSTA_TOGGLESQ;
}

#endif
