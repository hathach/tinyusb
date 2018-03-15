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

#include "nrf_drv_power.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_usbd_errata.h"

#include "tusb_dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum { MAX_PACKET_SIZE = 64 };

/*------------------------------------------------------------------*/
/* VARIABLE DECLARATION
 *------------------------------------------------------------------*/
static struct
{
  struct
  {
    uint8_t* buffer;
    uint16_t xfer_len;
    uint8_t  dir;
  }control;

  bool dma_running;
}_dcd_data;

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
static void hfclk_ready(nrf_drv_clock_evt_type_t event)
{
  // do nothing
}

static void enable_usb(void)
{
  /* Prepare for READY event receiving */
  nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);

  /* Enable the peripheral */
  nrf_usbd_enable();

  // Enable HFCLK
  nrf_drv_clock_handler_item_t clock_handler_item =
  {
      .event_handler = hfclk_ready
  };
  nrf_drv_clock_hfclk_request(&clock_handler_item);

  /* Waiting for peripheral to enable, this should take a few us */
  while ( !(NRF_USBD_EVENTCAUSE_READY_MASK & NRF_USBD->EVENTCAUSE) ) { }
  nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);
  nrf_usbd_event_clear(NRF_USBD_EVENT_USBEVENT);

  // Wait until power is ready
  while (!nrf_power_usbregstatus_outrdy_get()) {}
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
        static nrf_drv_clock_handler_item_t clock_handler_item =
        {
            .event_handler = hfclk_ready
        };
        nrf_drv_clock_hfclk_request(&clock_handler_item);

        /* Waiting for peripheral to enable, this should take a few us */
        while ( !(NRF_USBD_EVENTCAUSE_READY_MASK & NRF_USBD->EVENTCAUSE) ) { }
        nrf_usbd_eventcause_clear(NRF_USBD_EVENTCAUSE_READY_MASK);
        nrf_usbd_event_clear(NRF_USBD_EVENT_USBEVENT);
      }
    break;

    case NRF_DRV_POWER_USB_EVT_READY:
      // Wait for HFCLK
      while ( !nrf_drv_clock_hfclk_is_running() ) {}

      if ( nrf_drv_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;
        __ISB();
        __DSB();
      }

      nrf_usbd_isosplit_set(NRF_USBD_ISOSPLIT_Half);

      // Enable interrupt
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_ACCESSFAULT_Msk |
          USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk |
          /*USBD_INTEN_ENDEPIN0_Msk |*/  USBD_INTEN_ENDEPOUT0_Msk |
          /*USBD_INTEN_STARTED_Msk |*/  USBD_INTEN_EPDATA_Msk ;
      //USBD_INTEN_SOF_Msk

      //  if (enable_sof || nrf_drv_usbd_errata_104())
      //  {
      //    ints_to_enable |= NRF_USBD_INT_SOF_MASK;
      //  }

      // Enable interrupt
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
      }
    break;

    default: break;
  }
}

bool tusb_dcd_init (uint8_t port)
{
  // USB Power detection
  const nrf_drv_power_usbevt_config_t config =
  {
      .handler = power_usb_event_handler
  };
  VERIFY( NRF_SUCCESS == nrf_drv_power_usbevt_init(&config) );
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
  // address is automatically update by hw controller
}
void tusb_dcd_set_config (uint8_t port, uint8_t config_num)
{

}

/*------------------------------------------------------------------*/
/* Control
 *------------------------------------------------------------------*/

static void control_xact_start(void)
{
  // Each transaction is up to 64 bytes
  uint8_t xact_len = min16_of(_dcd_data.control.xfer_len, MAX_PACKET_SIZE);

  if ( _dcd_data.control.dir == TUSB_DIR_OUT )
  {
    // TODO control out
    NRF_USBD->EPOUT[0].PTR    = (uint32_t) _dcd_data.control.buffer;
    NRF_USBD->EPOUT[0].MAXCNT = xact_len;

    NRF_USBD->TASKS_EP0RCVOUT = 1;
  }else
  {
    NRF_USBD->EPIN[0].PTR        = (uint32_t) _dcd_data.control.buffer;
    NRF_USBD->EPIN[0].MAXCNT     = xact_len;

    NRF_USBD->TASKS_STARTEPIN[0] = 1;
  }

  _dcd_data.control.buffer   += xact_len;
  _dcd_data.control.xfer_len -= xact_len;

}

static void control_xact_done(void)
{
  if ( _dcd_data.control.xfer_len > 0 )
  {
    if ( _dcd_data.control.dir == TUSB_DIR_OUT )
    {
      // out control need to wait for END EPOUT event before updating Pointer
      NRF_USBD->TASKS_STARTEPOUT[0] = 1;
    }else
    {
      control_xact_start();
    }
  }else
  {
    tusb_dcd_xfer_complete(0, 0, 0, true);
  }
}


bool tusb_dcd_control_xfer (uint8_t port, tusb_dir_t dir, uint8_t * buffer, uint16_t length, bool int_on_complete)
{
  (void) port;

  if ( length )
  {
    // Data Phase
    _dcd_data.control.xfer_len = length;
    _dcd_data.control.buffer   = buffer;
    _dcd_data.control.dir      = (uint8_t) dir;

    control_xact_start();
  }else
  {
    // Status Phase
    NRF_USBD->TASKS_EP0STATUS = 1;
  }

  return true;
}
void tusb_dcd_control_stall (uint8_t port)
{
  (void) port;
  NRF_USBD->TASKS_EP0STALL = 1;
}

/*------------------------------------------------------------------*/
/*
 *------------------------------------------------------------------*/
bool tusb_dcd_edpt_open (uint8_t port, tusb_descriptor_endpoint_t const * p_endpoint_desc)
{
  (void) port;

  return true;
}

bool tusb_dcd_edpt_xfer (uint8_t port, uint8_t edpt_addr, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete)
{
  return true;
}

bool tusb_dcd_edpt_queue_xfer (uint8_t port, uint8_t edpt_addr, uint8_t * buffer, uint16_t total_bytes)
{
  return true;
}

void tusb_dcd_edpt_stall (uint8_t port, uint8_t edpt_addr)
{

}

void tusb_dcd_edpt_clear_stall (uint8_t port, uint8_t edpt_addr)
{

}

// TODO may remove
bool tusb_dcd_edpt_busy (uint8_t port, uint8_t edpt_addr)
{
  return true;
}

/*------------------------------------------------------------------*/
/*
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

  varclr(&_dcd_data);
}

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
      __ISB();
      __DSB();
    }
  }

#if 0
  if (nrf_drv_usbd_errata_104())
  {
    /* Event correcting */
    if ((0 == m_dma_pending) && (0 != (active & (USBD_INTEN_SOF_Msk))))
    {
      uint8_t usbi, uoi, uii;
      /* Testing */
      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7A9;
      uii = (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      if (0 != uii)
      {
        uii &= (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      }

      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AA;
      uoi = (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      if (0 != uoi)
      {
        uoi &= (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      }
      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AB;
      usbi = (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      if (0 != usbi)
      {
        usbi &= (uint8_t)(*((volatile uint32_t *)(NRF_USBD_BASE + 0x804)));
      }
      /* Processing */
      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AC;
      uii &= (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
      if (0 != uii)
      {
        uint8_t rb;
        m_simulated_dataepstatus |= ((uint32_t)uii) << USBD_EPIN_BITPOS_0;
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7A9;
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = uii;
        rb = (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
        NRF_DRV_USBD_LOG_PROTO1_FIX_PRINTF("   uii: 0x%.2x (0x%.2x)", uii, rb);
      }

      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AD;
      uoi &= (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
      if (0 != uoi)
      {
        uint8_t rb;
        m_simulated_dataepstatus |= ((uint32_t)uoi) << USBD_EPOUT_BITPOS_0;
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AA;
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = uoi;
        rb = (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
        NRF_DRV_USBD_LOG_PROTO1_FIX_PRINTF("   uoi: 0x%.2u (0x%.2x)", uoi, rb);
      }

      *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AE;
      usbi &= (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
      if (0 != usbi)
      {
        uint8_t rb;
        if (usbi & 0x01)
        {
          active |= USBD_INTEN_EP0SETUP_Msk;
        }
        if (usbi & 0x10)
        {
          active |= USBD_INTEN_USBRESET_Msk;
        }
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7AB;
        *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = usbi;
        rb = (uint8_t)*((volatile uint32_t *)(NRF_USBD_BASE + 0x804));
        NRF_DRV_USBD_LOG_PROTO1_FIX_PRINTF("   usbi: 0x%.2u (0x%.2x)", usbi, rb);
      }

      if (0 != (m_simulated_dataepstatus &
          ~((1U << USBD_EPOUT_BITPOS_0) | (1U << USBD_EPIN_BITPOS_0))))
      {
        active |= enabled & NRF_USBD_INT_DATAEP_MASK;
      }
      if (0 != (m_simulated_dataepstatus &
          ((1U << USBD_EPOUT_BITPOS_0) | (1U << USBD_EPIN_BITPOS_0))))
      {
        if (0 != (enabled & NRF_USBD_INT_EP0DATADONE_MASK))
        {
          m_simulated_dataepstatus &=
              ~((1U << USBD_EPOUT_BITPOS_0) | (1U << USBD_EPIN_BITPOS_0));
          active |= NRF_USBD_INT_EP0DATADONE_MASK;
        }
      }
    }
  }
#endif

  /*------------- Interrupt Processing -------------*/

  if ( int_status & USBD_INTEN_USBRESET_Msk )
  {
    bus_reset();

    tusb_dcd_bus_event(0, USBD_BUS_EVENT_RESET);
  }

  if ( int_status & USBD_INTEN_EP0SETUP_Msk )
  {
    uint8_t setup[8] = {
        NRF_USBD->BMREQUESTTYPE, NRF_USBD->BREQUEST, NRF_USBD->WVALUEL, NRF_USBD->WVALUEH,
        NRF_USBD->WINDEXL, NRF_USBD->WINDEXH, NRF_USBD->WLENGTHL, NRF_USBD->WLENGTHH
    };

    //NRF_USBD->TASKS_EP0STALL = 0; // clear stall upon receive new setup
    tusb_dcd_setup_received(0, setup);
  }

  if ( int_status & USBD_INTEN_EP0DATADONE_Msk )
  {
    if ( _dcd_data.control.dir == TUSB_DIR_OUT )
    {
      // out control need to wait for END EPOUT (DMA complete) event
      NRF_USBD->TASKS_STARTEPOUT[0] = 1;
    }else
    {
      if ( _dcd_data.control.xfer_len > 0 )
      {
        control_xact_start();
      }else
      {
        // Data IN xfer complete
        tusb_dcd_xfer_complete(0, 0, 0, true);
      }
    }
  }

  if ( int_status & USBD_INTEN_ENDEPOUT0_Msk)
  {
    if ( _dcd_data.control.xfer_len > 0 )
    {
      control_xact_start();
    }else
    {
      // Data OUT xfer complete
      tusb_dcd_xfer_complete(0, 0, 0, true);
    }
  }
}
