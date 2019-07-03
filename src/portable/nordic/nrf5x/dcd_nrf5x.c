/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_NRF5X

#include "nrf.h"
#include "nrf_power.h"
#include "nrf_usbd.h"
#include "nrf_clock.h"

#include "device/dcd.h"

// TODO remove later
#include "device/usbd.h"
#include "device/usbd_pvt.h" // to use defer function helper

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum
{
  // Max allowed by USB specs
  MAX_PACKET_SIZE   = 64,

  // Mask of all END event (IN & OUT) for all endpoints. ENDEPIN0-7, ENDEPOUT0-7, ENDISOIN, ENDISOOUT
  EDPT_END_ALL_MASK = (0xff << USBD_INTEN_ENDEPIN0_Pos) | (0xff << USBD_INTEN_ENDEPOUT0_Pos) |
                      USBD_INTENCLR_ENDISOIN_Msk | USBD_INTEN_ENDISOOUT_Msk
};

// Transfer descriptor
typedef struct
{
  uint8_t* buffer;
  uint16_t total_len;
  volatile uint16_t actual_len;
  uint8_t  mps; // max packet size

  // nrf52840 will auto ACK OUT packet after DMA is done
  // indicate packet is already ACK
  volatile bool data_received;

} xfer_td_t;

// Data for managing dcd
static struct
{
  // All 8 endpoints including control IN & OUT (offset 1)
  xfer_td_t xfer[8][2];

  // Only one DMA can run at a time
  volatile bool dma_running;
}_dcd;

/*------------------------------------------------------------------*/
/* Control / Bulk / Interrupt (CBI) Transfer
 *------------------------------------------------------------------*/

// helper to start DMA
static void edpt_dma_start(volatile uint32_t* reg_startep)
{
  // Only one dma can be active
  if ( _dcd.dma_running )
  {
    if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
    {
      // If called within ISR, use usbd task to defer later
      usbd_defer_func( (osal_task_func_t) edpt_dma_start, (void*) reg_startep, true );
      return;
    }
    else
    {
      // Otherwise simply block wait
      while ( _dcd.dma_running )  { }
    }
  }

  _dcd.dma_running = true;

  (*reg_startep) = 1;
  __ISB(); __DSB();
}

// DMA is complete
static void edpt_dma_end(void)
{
  TU_ASSERT(_dcd.dma_running, );
  _dcd.dma_running = false;
}

// helper getting td
static inline xfer_td_t* get_td(uint8_t epnum, uint8_t dir)
{
  return &_dcd.xfer[epnum][dir];
}

/*------------- CBI OUT Transfer -------------*/

// Prepare for a CBI transaction OUT, call at the start
// Allow ACK incoming data
static void xact_out_prepare(uint8_t epnum)
{
  if ( epnum == 0 )
  {
    NRF_USBD->TASKS_EP0RCVOUT = 1;
  }
  else
  {
    // Write zero value to SIZE register will allow hw to ACK (accept data)
    // If it is not already done by DMA
    NRF_USBD->SIZE.EPOUT[epnum] = 0;
  }

  __ISB(); __DSB();
}

// Start DMA to move data from Endpoint -> RAM
static void xact_out_dma(uint8_t epnum)
{
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);

  uint8_t const xact_len = NRF_USBD->SIZE.EPOUT[epnum];

  // Trigger DMA move data from Endpoint -> SRAM
  NRF_USBD->EPOUT[epnum].PTR    = (uint32_t) xfer->buffer;
  NRF_USBD->EPOUT[epnum].MAXCNT = xact_len;

  edpt_dma_start(&NRF_USBD->TASKS_STARTEPOUT[epnum]);

  xfer->buffer     += xact_len;
  xfer->actual_len += xact_len;
}

/*------------- CBI IN Transfer -------------*/

// Prepare for a CBI transaction IN, call at the start
// it start DMA to transfer data from RAM -> Endpoint
static void xact_in_prepare(uint8_t epnum)
{
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);

  // Each transaction is up to Max Packet Size
  uint8_t const xact_len = tu_min16(xfer->total_len - xfer->actual_len, xfer->mps);

  NRF_USBD->EPIN[epnum].PTR    = (uint32_t) xfer->buffer;
  NRF_USBD->EPIN[epnum].MAXCNT = xact_len;

  xfer->buffer += xact_len;

  edpt_dma_start(&NRF_USBD->TASKS_STARTEPIN[epnum]);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
void dcd_init (uint8_t rhport)
{
  (void) rhport;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USBD_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USBD_IRQn);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;
  // Set Address is automatically update by hw controller, nothing to do

  // Enable usbevent for suspend and resume detection
  // Since the bus signal D+/D- are stable now.

  // Clear current pending first
  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;
  NRF_USBD->EVENTS_USBEVENT = 0;

  NRF_USBD->INTENSET = USBD_INTEN_USBEVENT_Msk;
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;

  // Bring controller out of low power mode
  NRF_USBD->LOWPOWER = 0;

  // Initiate RESUME signal
  NRF_USBD->DPDMVALUE = USBD_DPDMVALUE_STATE_Resume;
  NRF_USBD->TASKS_DPDMDRIVE = 1;

  // TODO There is no USBEVENT Resume interrupt
  // We may manually raise DCD_EVENT_RESUME event here
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  _dcd.xfer[epnum][dir].mps = desc_edpt->wMaxPacketSize.size;

  if ( dir == TUSB_DIR_OUT )
  {
    NRF_USBD->INTENSET = TU_BIT(USBD_INTEN_ENDEPOUT0_Pos + epnum);
    NRF_USBD->EPOUTEN |= TU_BIT(epnum);
  }else
  {
    NRF_USBD->INTENSET = TU_BIT(USBD_INTEN_ENDEPIN0_Pos + epnum);
    NRF_USBD->EPINEN  |= TU_BIT(epnum);
  }
  __ISB(); __DSB();

  return true;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_td_t* xfer = get_td(epnum, dir);

  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->actual_len = 0;

  // Control endpoint with zero-length packet --> status stage
  if ( epnum == 0 && total_bytes == 0 )
  {
    // Status Phase also require Easy DMA has to be free as well !!!!
    edpt_dma_start(&NRF_USBD->TASKS_EP0STATUS);
    edpt_dma_end();

    // The nRF doesn't interrupt on status transmit so we queue up a success response.
    dcd_event_xfer_complete(0, ep_addr, 0, XFER_RESULT_SUCCESS, false);
  }
  else if ( dir == TUSB_DIR_OUT )
  {
    if ( xfer->data_received )
    {
      // nrf52840 auto ACK OUT packet after DMA is done
      // Data already received previously --> trigger DMA to copy to SRAM
      xact_out_dma(epnum);
    }
    else
    {
      xact_out_prepare(epnum);
    }
  }
  else
  {
    xact_in_prepare(epnum);
  }

  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  if ( tu_edpt_number(ep_addr) == 0 )
  {
    NRF_USBD->TASKS_EP0STALL = 1;
  }else
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | ep_addr;
  }

  __ISB(); __DSB();
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  if ( tu_edpt_number(ep_addr)  )
  {
    // clear stall
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;

    // reset data toggle to DATA0
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep_addr;

    __ISB(); __DSB();
  }
}

/*------------------------------------------------------------------*/
/* Interrupt Handler
 *------------------------------------------------------------------*/
void bus_reset(void)
{
  for(int i=0; i<8; i++)
  {
    NRF_USBD->TASKS_STARTEPIN[i] = 0;
    NRF_USBD->TASKS_STARTEPOUT[i] = 0;
  }

  NRF_USBD->TASKS_STARTISOIN  = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  tu_varclr(&_dcd);
  _dcd.xfer[0][TUSB_DIR_IN].mps = MAX_PACKET_SIZE;
  _dcd.xfer[0][TUSB_DIR_OUT].mps = MAX_PACKET_SIZE;
}

void USBD_IRQHandler(void)
{
  uint32_t const inten  = NRF_USBD->INTEN;
  uint32_t int_status = 0;

  volatile uint32_t* regevt = &NRF_USBD->EVENTS_USBRESET;

  for(uint8_t i=0; i<USBD_INTEN_EPDATA_Pos+1; i++)
  {
    if ( tu_bit_test(inten, i) && regevt[i]  )
    {
      int_status |= TU_BIT(i);

      // event clear
      regevt[i] = 0;
      __ISB(); __DSB();
    }
  }

  if ( int_status & USBD_INTEN_USBRESET_Msk )
  {
    bus_reset();
    dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
  }

  if ( int_status & USBD_INTEN_SOF_Msk )
  {
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  if ( int_status & USBD_INTEN_USBEVENT_Msk )
  {
    uint32_t const evt_cause = NRF_USBD->EVENTCAUSE & (USBD_EVENTCAUSE_SUSPEND_Msk | USBD_EVENTCAUSE_RESUME_Msk);
    NRF_USBD->EVENTCAUSE = evt_cause; // clear interrupt

    if ( evt_cause & USBD_EVENTCAUSE_SUSPEND_Msk )
    {
      dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);

      // Put controller into low power mode
      NRF_USBD->LOWPOWER = 1;

      // Leave HFXO disable to application, since it may be used by other
    }

    if ( evt_cause & USBD_EVENTCAUSE_RESUME_Msk  )
    {
      dcd_event_bus_signal(0, DCD_EVENT_RESUME , true);
    }
  }

  if ( int_status & EDPT_END_ALL_MASK )
  {
    // DMA complete move data from SRAM -> Endpoint
    edpt_dma_end();
  }
 
  // Setup tokens are specific to the Control endpoint.
  if ( int_status & USBD_INTEN_EP0SETUP_Msk )
  {
    uint8_t const setup[8] = {
        NRF_USBD->BMREQUESTTYPE , NRF_USBD->BREQUEST, NRF_USBD->WVALUEL , NRF_USBD->WVALUEH,
        NRF_USBD->WINDEXL       , NRF_USBD->WINDEXH , NRF_USBD->WLENGTHL, NRF_USBD->WLENGTHH
    };

    // nrf5x hw auto handle set address, there is no need to inform usb stack
    tusb_control_request_t const * request = (tusb_control_request_t const *) setup;

    if ( !(TUSB_REQ_RCPT_DEVICE == request->bmRequestType_bit.recipient &&
           TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
           TUSB_REQ_SET_ADDRESS == request->bRequest) )
    {
      dcd_event_setup_received(0, setup, true);
    }
  }

  //--------------------------------------------------------------------+
  /* Control/Bulk/Interrupt (CBI) Transfer
   *
   * Data flow is:
   *           (bus)              (dma)
   *    Host <-------> Endpoint <-------> RAM
   *
   * For CBI OUT:
   *  - Host -> Endpoint
   *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPOUT[i]
   *      to start DMA. This step can occur automatically (without sw),
   *      which means data may or may not ready (data_received flag).
   *  - Endpoint -> RAM
   *      ENDEPOUT[i] interrupted, transaction complete, sw prepare next transaction
   *
   * For CBI IN:
   *  - RAM -> Endpoint
   *      ENDEPIN[i] interrupted indicate DMA is complete. HW will start
   *      to move daat to host
   *  - Endpoint -> Host
   *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPIN[i].
   *      Transaction is complete, sw prepare next transaction
   *
   * Note: in both Control In and Out of Data stage from Host <-> Endpoint
   * EP0DATADONE will be set as interrupt source
   */
  //--------------------------------------------------------------------+

  /* CBI OUT: Endpoint -> SRAM (aka transaction complete)
   * Note: Since nRF controller auto ACK next packet without SW awareness
   * We must handle this stage before Host -> Endpoint just in case
   * 2 event happens at once
   */
  for(uint8_t epnum=0; epnum<8; epnum++)
  {
    if ( tu_bit_test(int_status, USBD_INTEN_ENDEPOUT0_Pos+epnum))
    {
      xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);
      uint8_t const xact_len = NRF_USBD->EPOUT[epnum].AMOUNT;

      // Data in endpoint has been consumed
      xfer->data_received = false;

      // Transfer complete if transaction len < Max Packet Size or total len is transferred
      if ( (xact_len == xfer->mps) && (xfer->actual_len < xfer->total_len) )
      {
        // Prepare for next transaction
        xact_out_prepare(epnum);
      }else
      {
        xfer->total_len = xfer->actual_len;

        // CBI OUT complete
        dcd_event_xfer_complete(0, epnum, xfer->actual_len, XFER_RESULT_SUCCESS, true);
      }
    }

    // Ended event for CBI IN : nothing to do
  }

  // Endpoint <-> Host
  if ( int_status & (USBD_INTEN_EPDATA_Msk | USBD_INTEN_EP0DATADONE_Msk) )
  {
    uint32_t data_status = NRF_USBD->EPDATASTATUS;
    nrf_usbd_epdatastatus_clear(data_status);

    // EP0DATADONE is set with either Control Out on IN Data
    // Since EPDATASTATUS cannot be used to determine whether it is control OUT or IN.
    // We will use BMREQUESTTYPE in setup packet to determine the direction
    bool const is_control_in = (int_status & USBD_INTEN_EP0DATADONE_Msk) && (NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);
    bool const is_control_out = (int_status & USBD_INTEN_EP0DATADONE_Msk) && !(NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);

    // CBI In: Endpoint -> Host (transaction complete)
    for(uint8_t epnum=0; epnum<8; epnum++)
    {
      if ( tu_bit_test(data_status, epnum ) || ( epnum == 0 && is_control_in) )
      {
        xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);

        xfer->actual_len += NRF_USBD->EPIN[epnum].MAXCNT;

        if ( xfer->actual_len < xfer->total_len )
        {
          // prepare next transaction
          xact_in_prepare(epnum);
        } else
        {
          // CBI IN complete
          dcd_event_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, xfer->actual_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }

    // CBI OUT: Host -> Endpoint
    for(uint8_t epnum=0; epnum<8; epnum++)
    {
      if ( tu_bit_test(data_status, 16+epnum ) || ( epnum == 0 && is_control_out) )
      {
        xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);

        if (xfer->actual_len < xfer->total_len)
        {
          xact_out_dma(epnum);
        }else
        {
          // Data overflow !!! Nah, nrf52840 will auto ACK OUT packet after DMA is done
          // Mark this endpoint with data received
          xfer->data_received = true;
        }
      }
    }
  }
}

#endif
