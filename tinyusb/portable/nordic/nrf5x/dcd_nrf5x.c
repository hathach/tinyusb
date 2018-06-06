/**************************************************************************/
/*!
    @file     dcd_nrf5x.c
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

#include "tusb_option.h"

#if MODE_DEVICE_SUPPORTED && CFG_TUSB_MCU == OPT_MCU_NRF5X

// TODO remove
#include "nrf.h"
#include "nrf_power.h"
#include "nrf_usbd.h"
#include "nrf_clock.h"

#include "nrf_drv_usbd_errata.h"

#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum
{
  // Max allowed by USB specs
  MAX_PACKET_SIZE   = 64,

  // Mask of all END event (IN & OUT) for all endpoints. ENDEPIN0-7, ENDEPOUT0-7, ENDISOIN, ENDISOOUT
  EDPT_END_ALL_MASK = 0x1FFBFCUL
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
    uint16_t total_len;
    uint16_t actual_len;

    uint8_t  dir;
  }control;

  // Non control: 7 endpoints IN & OUT (offset 1)
  nom_xfer_t xfer[7][2];

  volatile bool dma_running;
}_dcd;

void bus_reset(void)
{
  for(int i=0; i<8; i++)
  {
    NRF_USBD->TASKS_STARTEPIN[i] = 0;
    NRF_USBD->TASKS_STARTEPOUT[i] = 0;
  }

  NRF_USBD->TASKS_STARTISOIN  = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  varclr_(&_dcd);
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
bool dcd_init (uint8_t rhport)
{
  (void) rhport;
  return true;
}

void dcd_connect (uint8_t rhport)
{

}
void dcd_disconnect (uint8_t rhport)
{

}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  // Set Address is automatically update by hw controller
}

void dcd_set_config (uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // Nothing to do
}

/*------------------------------------------------------------------*/
/* Control
 *------------------------------------------------------------------*/
static void edpt_dma_start(uint8_t epnum, uint8_t dir)
{
  // Only one dma could be active, TODO resolve when this is called in ISR and dma is running
  while ( _dcd.dma_running ) 
  {
    TU_ASSERT ( 0 == (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk), );
  }

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
  uint8_t const xact_len = min16_of(_dcd.control.total_len-_dcd.control.actual_len, MAX_PACKET_SIZE);

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

  _dcd.control.buffer     += xact_len;
  _dcd.control.actual_len += xact_len;
}

bool dcd_control_xfer (uint8_t rhport, tusb_dir_t dir, uint8_t * buffer, uint16_t length)
{
  (void) rhport;

  if ( length )
  {
    // Data Phase
    _dcd.control.total_len  = length;
    _dcd.control.actual_len = 0;
    _dcd.control.buffer     = buffer;
    _dcd.control.dir        = (uint8_t) dir;

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

static inline nom_xfer_t* get_td(uint8_t epnum, uint8_t dir)
{
  return &_dcd.xfer[epnum-1][dir];
}

static void normal_xact_start(uint8_t epnum, uint8_t dir)
{
  if ( dir == TUSB_DIR_OUT )
  {
    // Errata : HW issue on nrf5284 sample, SIZE.EPOUT won't trigger ACK as spec
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
    nom_xfer_t* xfer = get_td(epnum, dir);

    // Each transaction is up to Max Packet Size
    uint8_t const xact_len = min16_of(xfer->total_len - xfer->actual_len, xfer->mps);

    NRF_USBD->EPIN[epnum].PTR    = (uint32_t) xfer->buffer;
    NRF_USBD->EPIN[epnum].MAXCNT = xact_len;

    xfer->buffer += xact_len;

    edpt_dma_start(epnum, TUSB_DIR_IN);
  }
}

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = edpt_dir(desc_edpt->bEndpointAddress);

  _dcd.xfer[epnum-1][dir].mps = desc_edpt->wMaxPacketSize.size;

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

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  nom_xfer_t* xfer = get_td(epnum, dir);

  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->actual_len = 0;

  normal_xact_start(epnum, dir);

  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  if ( ep_addr == 0)
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
  if ( ep_addr )
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;
  }
}

bool dcd_edpt_busy (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // USBD shouldn't check control endpoint state
  if ( 0 == ep_addr ) return false;

  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  nom_xfer_t* xfer = get_td(epnum, dir);

  return xfer->actual_len < xfer->total_len;
}

/*------------------------------------------------------------------*/
/*
 *------------------------------------------------------------------*/
void USBD_IRQHandler(void)
{
  uint32_t const inten  = NRF_USBD->INTEN;
  uint32_t int_status = 0;

  volatile uint32_t* regevt = &NRF_USBD->EVENTS_USBRESET;

  for(int i=0; i<32; i++)
  {
    if ( BIT_TEST_(inten, i) && regevt[i]  )
    {
      int_status |= BIT_(i);

      // event clear
      regevt[i] = 0;
      __ISB(); __DSB();
    }
  }

  /*------------- Interrupt Processing -------------*/
  if ( int_status & USBD_INTEN_USBRESET_Msk )
  {
    bus_reset();

    dcd_bus_event(0, USBD_BUS_EVENT_RESET);
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

    dcd_setup_received(0, setup);
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
      if ( _dcd.control.actual_len < _dcd.control.total_len )
      {
        control_xact_start();
      }else
      {
        // Control IN complete
        dcd_control_complete(0, _dcd.control.actual_len);
      }
    }
  }

  // OUT data moved from Endpoint -> SRAM
  if ( int_status & USBD_INTEN_ENDEPOUT0_Msk)
  {
    if ( _dcd.control.actual_len < _dcd.control.total_len )
    {
      control_xact_start();
    }else
    {
      // Control OUT complete
      dcd_control_complete(0, _dcd.control.actual_len);
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
        nom_xfer_t* xfer = get_td(epnum, TUSB_DIR_IN);

        xfer->actual_len += NRF_USBD->EPIN[epnum].MAXCNT;

        if ( xfer->actual_len < xfer->total_len )
        {
          // more to xfer
          normal_xact_start(epnum, TUSB_DIR_IN);
        } else
        {
          // BULK/INT IN complete
          dcd_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, xfer->actual_len, true);
        }
      }
    }

    // OUT: data from Host -> Endpoint
    for(uint8_t epnum=1; epnum<8; epnum++)
    {
      if ( BIT_TEST_(data_status, 16+epnum ) )
      {
        nom_xfer_t* xfer = get_td(epnum, TUSB_DIR_OUT);

        uint8_t const xact_len = NRF_USBD->SIZE.EPOUT[epnum];

        // FIXME nrf52840 rev A does not NAK OUT packet properly
        TU_ASSERT(xfer->actual_len < xfer->total_len, );

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
      nom_xfer_t* xfer = get_td(epnum, TUSB_DIR_OUT);

      uint8_t const xact_len = NRF_USBD->EPOUT[epnum].AMOUNT;

      // Transfer complete if transaction len < Max Packet Size or total len is transferred
      if ( (xact_len == xfer->mps) && (xfer->actual_len < xfer->total_len) )
      {
        // Prepare for more data from Host -> Endpoint
        normal_xact_start(epnum, TUSB_DIR_OUT);
      }else
      {
        xfer->total_len = xfer->actual_len;

        // BULK/INT OUT complete
        dcd_xfer_complete(0, epnum, xfer->actual_len, true);
      }
    }
  }


  // SOF interrupt
  if ( int_status & USBD_INTEN_SOF_Msk )
  {
    dcd_bus_event(0, USBD_BUS_EVENT_SOF);
  }


}

#endif
