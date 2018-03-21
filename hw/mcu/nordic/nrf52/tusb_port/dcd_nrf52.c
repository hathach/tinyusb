/**************************************************************************/
/*!
    @file     dcd_nrf52.c
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
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
*/
/**************************************************************************/

// TODO remove
#include "nrf.h"
#include "nrf_power.h"
#include "nrf_usbd.h"
#include "nrf_clock.h"

#include "nrf_drv_power.h"
#include "nrf_drv_usbd_errata.h"

#include "tusb_dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum
{
  // Max allowed by USB specs
  MAX_PACKET_SIZE   = 64,

  // Mask of all END event (IN & OUT) for all endpoints. ENDEPIN0-7, ENDEPOUT0-7, ENDISOIN, ENDISOOUT
  EDPT_END_ALL_MASK = 0x1FFBFC
};

/*------------------------------------------------------------------*/
/* VARIABLE DECLARATION
 *------------------------------------------------------------------*/
typedef struct
{
  uint8_t* buffer;
  uint16_t total_len;
  uint16_t actual_len;

  uint8_t  mps; // max packet size
} nom_xfer_t;

/*static*/ struct
{
  struct
  {
    uint8_t* buffer;
    uint16_t len;
    uint8_t  dir;
  }control;

  // Non control: 7 endpoints IN & OUT (offset 1)
  nom_xfer_t xfer[2][7];

  volatile bool dma_running;
}_dcd;

/*------------------------------------------------------------------*/
/* Controller Start up Sequence
 *------------------------------------------------------------------*/
static bool hfclk_running(void)
{
#ifdef SOFTDEVICE_PRESENT
  if (nrf_sdh_is_enabled())
  {
    uint32_t is_running;
    (void) sd_clock_hfclk_is_running(&is_running);
    return (is_running ? true : false);
  }
#endif

  return nrf_clock_hf_is_running(NRF_CLOCK_HFCLK_HIGH_ACCURACY);
}

static void hfclk_enable(void)
{
  // already running, nothing to do
  if ( hfclk_running() ) return;

#ifdef SOFTDEVICE_PRESENT
  if (nrf_sdh_is_enabled())
  {
    (void)sd_clock_hfclk_request();
    return;
  }
#endif

  nrf_clock_event_clear(NRF_CLOCK_EVENT_HFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
}

static void hfclk_disable(void)
{
#ifdef SOFTDEVICE_PRESENT
  if (nrf_sdh_is_enabled())
  {
    (void)sd_clock_hfclk_release();
    return;
  }
#endif // SOFTDEVICE_PRESENT

  nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTOP);
}

static void power_usb_event_handler(nrf_drv_power_usb_evt_t event)
{
  // 51.4 specs USBD start-up sequene
  switch ( event )
  {
    case NRF_DRV_POWER_USB_EVT_DETECTED:
      if ( !NRF_USBD->ENABLE )
      {
        /* Prepare for READY event receiving */
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

        /* Enable the peripheral */
        nrf_usbd_enable();

        // Enable HFCLK
        hfclk_enable();

        /* Waiting for peripheral to enable, this should take a few us */
        while ( !(NRF_USBD_EVENTCAUSE_READY_MASK & NRF_USBD->EVENTCAUSE) ) { }
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);
        nrf_usbd_event_clear(NRF_USBD_EVENT_USBEVENT);
      }
    break;

    case NRF_DRV_POWER_USB_EVT_READY:
      // Wait for HFCLK
      while ( !hfclk_running() ) {}

      if ( nrf_drv_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB(); __DSB();
      }

      nrf_usbd_isosplit_set(NRF_USBD_ISOSPLIT_Half);

      // Enable interrupt. SOF is used as CDC auto flush
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_ACCESSFAULT_Msk |
                           USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk |  USBD_INTEN_ENDEPOUT0_Msk |
                           USBD_INTEN_EPDATA_Msk   | USBD_INTEN_SOF_Msk;

      //  if (enable_sof || nrf_drv_usbd_errata_104())
      //  {
      //    ints_to_enable |= NRF_USBD_INT_SOF_MASK;
      //  }

      // Enable interrupt, Priorities 0,1,4,5 (nRF52) are reserved for SoftDevice
      NVIC_SetPriority(USBD_IRQn, 7);
      NVIC_ClearPendingIRQ(USBD_IRQn);
      NVIC_EnableIRQ(USBD_IRQn);

      // Enable pull up
      nrf_usbd_pullup_enable();
    break;

    case NRF_DRV_POWER_USB_EVT_REMOVED:
      if ( NRF_USBD->ENABLE )
      {
        // Abort all transfers

        // Disable pull up
        nrf_usbd_pullup_disable();

        // Disable Interrupt
        NVIC_DisableIRQ(USBD_IRQn);

        // disable all interrupt
        NRF_USBD->INTENCLR = NRF_USBD->INTEN;

        nrf_usbd_disable();
        hfclk_disable();
      }
    break;

    default: break;
  }
}

void bus_reset(void)
{
  for(int i=0; i<8; i++)
  {
    NRF_USBD->TASKS_STARTEPIN[i] = 0;
    NRF_USBD->TASKS_STARTEPOUT[i] = 0;
  }

  NRF_USBD->TASKS_STARTISOIN  = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  varclr(&_dcd);
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
bool tusb_dcd_init (uint8_t port)
{
  // USB Power detection
  const nrf_drv_power_usbevt_config_t config =
  {
      .handler = power_usb_event_handler
  };
  return ( NRF_SUCCESS == nrf_drv_power_usbevt_init(&config) );
}

void tusb_dcd_connect (uint8_t port)
{

}
void tusb_dcd_disconnect (uint8_t port)
{

}

void tusb_dcd_set_address (uint8_t port, uint8_t dev_addr)
{
  (void) port;
  // Set Address is automatically update by hw controller
}

void tusb_dcd_set_config (uint8_t port, uint8_t config_num)
{
  (void) port;
  (void) config_num;
  // Nothing to do
}

/*------------------------------------------------------------------*/
/* Control
 *------------------------------------------------------------------*/
static void edpt_dma_start(uint8_t epnum, uint8_t dir)
{
  // Only one dma could be active
  while ( _dcd.dma_running ) { }

  _dcd.dma_running = true;

  if ( dir == TUSB_DIR_OUT )
  {
    NRF_USBD->TASKS_STARTEPOUT[epnum] = 1;
  } else
  {
    NRF_USBD->TASKS_STARTEPIN[epnum] = 1;
  }

  __ISB(); __DSB();
}

static void edpt_dma_end(void)
{
  _dcd.dma_running = false;
}

static void control_xact_start(void)
{
  // Each transaction is up to 64 bytes
  uint8_t const xact_len = min16_of(_dcd.control.len, MAX_PACKET_SIZE);

  if ( _dcd.control.dir == TUSB_DIR_OUT )
  {
    // TODO control out
    NRF_USBD->EPOUT[0].PTR    = (uint32_t) _dcd.control.buffer;
    NRF_USBD->EPOUT[0].MAXCNT = xact_len;

    NRF_USBD->TASKS_EP0RCVOUT = 1;
    __ISB(); __DSB();
  }else
  {
    NRF_USBD->EPIN[0].PTR        = (uint32_t) _dcd.control.buffer;
    NRF_USBD->EPIN[0].MAXCNT     = xact_len;

    edpt_dma_start(0, TUSB_DIR_IN);
  }

  _dcd.control.buffer += xact_len;
  _dcd.control.len    -= xact_len;
}

//static void control_xact_done(void)
//{
//  if ( _dcd_data.control.xfer_len > 0 )
//  {
//    if ( _dcd_data.control.dir == TUSB_DIR_OUT )
//    {
//      // out control need to wait for END EPOUT event before updating Pointer
//      edpt_dma_start(0, TUSB_DIR_OUT);
//    }else
//    {
//      control_xact_start();
//    }
//  }else
//  {
//    tusb_dcd_xfer_complete(0, 0, 0, true);
//  }
//}


bool tusb_dcd_control_xfer (uint8_t port, tusb_dir_t dir, uint8_t * buffer, uint16_t length)
{
  (void) port;

  if ( length )
  {
    // Data Phase
    _dcd.control.len    = length;
    _dcd.control.buffer = buffer;
    _dcd.control.dir    = (uint8_t) dir;

    control_xact_start();
  }else
  {
    // Status Phase
    NRF_USBD->TASKS_EP0STATUS = 1;
    __ISB(); __DSB();
  }

  return true;
}

/*------------------------------------------------------------------*/
/*
 *------------------------------------------------------------------*/
static void normal_xact_start(uint8_t epnum, uint8_t dir)
{
  nom_xfer_t* xfer = &_dcd.xfer[dir][epnum-1];

  // Each transaction is up to Max Packet Size
  uint8_t const xact_len = min16_of(xfer->total_len - xfer->actual_len, xfer->mps);

  if ( dir == TUSB_DIR_OUT )
  {
    // HW issue on nrf5284 sample, SIZE.EPOUT won't trigger ACK as spec
    // use the back door interface as sdk for walk around
    if ( nrf_drv_usbd_errata_sizeepout_rw() )
    {
      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7C5 + 2*epnum;
      *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = 0;
      (void) (((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
    }
    else
    {
      // Overwrite size will allow hw to accept data
      NRF_USBD->SIZE.EPOUT[epnum] = 0;
      __ISB(); __DSB();
    }
  }else
  {
    NRF_USBD->EPIN[epnum].PTR    = (uint32_t) xfer->buffer;
    NRF_USBD->EPIN[epnum].MAXCNT = xact_len;

    xfer->buffer += xact_len;

    edpt_dma_start(epnum, TUSB_DIR_IN);
  }
}

bool tusb_dcd_edpt_open (uint8_t port, tusb_descriptor_endpoint_t const * desc_edpt)
{
  (void) port;

  uint8_t const epnum = edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = edpt_dir(desc_edpt->bEndpointAddress);

  _dcd.xfer[dir][epnum-1].mps = desc_edpt->wMaxPacketSize.size;

  if ( dir == TUSB_DIR_OUT )
  {
    NRF_USBD->INTENSET = BIT_(USBD_INTEN_ENDEPOUT0_Pos + epnum);
    NRF_USBD->EPOUTEN |= BIT_(epnum);
  }else
  {
    NRF_USBD->INTENSET = BIT_(USBD_INTEN_ENDEPIN0_Pos + epnum);
    NRF_USBD->EPINEN  |= BIT_(epnum);
  }
  __ISB(); __DSB();

  return true;
}

bool tusb_dcd_edpt_xfer (uint8_t port, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete)
{
  (void) port;

  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  _dcd.xfer[dir][epnum-1].buffer     = buffer;
  _dcd.xfer[dir][epnum-1].total_len  = total_bytes;
  _dcd.xfer[dir][epnum-1].actual_len = 0;

  normal_xact_start(epnum, dir);

//  if ( dir == TUSB_DIR_OUT )
//  {
//    // TODO
//  }else
//  {
//
//  }

  return true;
}

bool tusb_dcd_edpt_queue_xfer (uint8_t port, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  return true;
}

void tusb_dcd_edpt_stall (uint8_t port, uint8_t ep_addr)
{
  (void) port;

  if ( ep_addr == 0)
  {
    NRF_USBD->TASKS_EP0STALL = 1;
  }else
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | ep_addr;
  }

  __ISB(); __DSB();
}

void tusb_dcd_edpt_clear_stall (uint8_t port, uint8_t ep_addr)
{
  (void) port;
  if ( ep_addr )
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;
  }
}

bool tusb_dcd_edpt_busy (uint8_t port, uint8_t ep_addr)
{
  (void) port;

  // USBD shouldn't check control endpoint state
  if ( 0 == ep_addr ) return false;

  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  nom_xfer_t* xfer = &_dcd.xfer[dir][epnum-1];

  return xfer->actual_len < xfer->total_len;
}

/*------------------------------------------------------------------*/
/*
 *------------------------------------------------------------------*/
void USBD_IRQHandler(void)
{
  uint32_t const inten  = NRF_USBD->INTEN;
  uint32_t int_status = 0;

  volatile uint32_t* regclr = &NRF_USBD->EVENTS_USBRESET;

  for(int i=0; i<32; i++)
  {
    if ( BIT_TEST_(inten, i) && regclr[i]  )
    {
      int_status |= BIT_(i);

      // nrf_usbd_event_clear()
      regclr[i] = 0;

      __ISB(); __DSB();
    }
  }

  /*------------- Interrupt Processing -------------*/
  if ( int_status & USBD_INTEN_USBRESET_Msk )
  {
    bus_reset();

    tusb_dcd_bus_event(0, USBD_BUS_EVENT_RESET);
  }

  if ( int_status & USBD_INTEN_SOF_Msk )
  {
    tusb_dcd_bus_event(0, USBD_BUS_EVENT_SOF);
  }

  if ( int_status & EDPT_END_ALL_MASK )
  {
    // DMA complete move data from SRAM -> Endpoint
    edpt_dma_end();
  }

  /*------------- Control Transfer -------------*/
  if ( int_status & USBD_INTEN_EP0SETUP_Msk )
  {
    uint8_t setup[8] = {
        NRF_USBD->BMREQUESTTYPE, NRF_USBD->BREQUEST, NRF_USBD->WVALUEL, NRF_USBD->WVALUEH,
        NRF_USBD->WINDEXL, NRF_USBD->WINDEXH, NRF_USBD->WLENGTHL, NRF_USBD->WLENGTHH
    };

    tusb_dcd_setup_received(0, setup);
  }

  if ( int_status & USBD_INTEN_EP0DATADONE_Msk )
  {
    if ( _dcd.control.dir == TUSB_DIR_OUT )
    {
      // OUT data from Host -> Endpoint
      // Trigger DMA to move Endpoint -> SRAM
      edpt_dma_start(0, TUSB_DIR_OUT);
    }else
    {
      // IN: data transferred from Endpoint -> Host
      if ( _dcd.control.len > 0 )
      {
        control_xact_start();
      }else
      {
        // Control IN complete
        tusb_dcd_xfer_complete(0, 0, 0, true);
      }
    }
  }

  if ( int_status & USBD_INTEN_ENDEPOUT0_Msk)
  {
    // OUT data moved from Endpoint -> SRAM
    if ( _dcd.control.len > 0 )
    {
      control_xact_start();
    }else
    {
      // Control OUT complete
      tusb_dcd_xfer_complete(0, 0, 0, true);
    }
  }

  /*------------- Bulk/Interrupt Transfer -------------*/
  if ( int_status & USBD_INTEN_EPDATA_Msk)
  {
    uint32_t data_status = NRF_USBD->EPDATASTATUS;

    nrf_usbd_epdatastatus_clear(data_status);

    // In: data from Endpoint -> Host
    for(uint8_t epnum=1; epnum<8; epnum++)
    {
      if ( BIT_TEST_(data_status, epnum ) )
      {
        nom_xfer_t* xfer = &_dcd.xfer[TUSB_DIR_IN][epnum-1];

        xfer->actual_len += NRF_USBD->EPIN[epnum].MAXCNT;

        if ( xfer->actual_len < xfer->total_len )
        {
          // more to xfer
          normal_xact_start(epnum, TUSB_DIR_IN);
        } else
        {
          // BULK/INT IN complete
          tusb_dcd_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, xfer->actual_len, true);
        }
      }
    }

    // OUT: data from Host -> Endpoint
    for(uint8_t epnum=1; epnum<8; epnum++)
    {
      if ( BIT_TEST_(data_status, 16+epnum ) )
      {
        nom_xfer_t* xfer = &_dcd.xfer[TUSB_DIR_OUT][epnum-1];

        uint8_t const xact_len = NRF_USBD->SIZE.EPOUT[epnum];

        // Trigger DMA move data from Endpoint -> SRAM
        NRF_USBD->EPOUT[epnum].PTR    = (uint32_t) xfer->buffer;
        NRF_USBD->EPOUT[epnum].MAXCNT = xact_len;

        edpt_dma_start(epnum, TUSB_DIR_OUT);

        xfer->buffer     += xact_len;
        xfer->actual_len += xact_len;
      }
    }
  }

  // OUT: data from DMA -> SRAM
  for(uint8_t epnum=1; epnum<8; epnum++)
  {
    if ( BIT_TEST_(int_status, USBD_INTEN_ENDEPOUT0_Pos+epnum) )
    {
      nom_xfer_t* xfer = &_dcd.xfer[TUSB_DIR_OUT][epnum-1];

      // Transfer complete if transaction len < Max Packet Size or total len is transferred
      if ( (NRF_USBD->EPOUT[epnum].AMOUNT == xfer->mps) && (xfer->actual_len < xfer->total_len) )
      {
        // Allow Host -> Endpoint

        // HW issue on nrf5284 sample, SIZE.EPOUT won't trigger ACK as spec
        // use the back door interface as sdk for walk around
        if ( nrf_drv_usbd_errata_sizeepout_rw() )
        {
          *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7C5 + 2*epnum;
          *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = 0;
          (void) (((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
        }
        else
        {
          // Overwrite size will allow hw to accept data
          NRF_USBD->SIZE.EPOUT[epnum] = 0;
          __ISB(); __DSB();
        }
      }else
      {
        xfer->total_len = xfer->actual_len;

        // BULK/INT OUT complete
        tusb_dcd_xfer_complete(0, epnum, xfer->actual_len, true);
      }
    }
  }

}
