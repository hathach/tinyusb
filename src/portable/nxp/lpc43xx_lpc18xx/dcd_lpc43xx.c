/**************************************************************************/
/*!
    @file     dcd_lpc43xx.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_LPC43XX

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "tusb_hal.h"
#include "osal/osal.h"

#include "device/dcd.h"
#include "dcd_lpc43xx.h"

#include "LPC43xx.h"
#include "lpc43xx_cgu.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
typedef struct {
  dcd_qhd_t qhd[DCD_QHD_MAX] ATTR_ALIGNED(64); ///< Must be at 2K alignment
  dcd_qtd_t qtd[DCD_QTD_MAX] ATTR_ALIGNED(32);
}dcd_data_t;

#if (CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE)
CFG_TUSB_MEM_SECTION ATTR_ALIGNED(2048) static dcd_data_t dcd_data0;
#endif

#if (CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE)
CFG_TUSB_MEM_SECTION ATTR_ALIGNED(2048) static dcd_data_t dcd_data1;
#endif

static LPC_USB0_Type * const LPC_USB[2] = { LPC_USB0, ((LPC_USB0_Type*) LPC_USB1_BASE) };

static dcd_data_t* const dcd_data_ptr[2] =
{
#if (CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE)
  &dcd_data0,
#else
  NULL,
#endif

#if (CFG_TUSB_RHPORT1_MODE & OPT_MODE_DEVICE)
  &dcd_data1
#else
  NULL
#endif
};

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void dcd_connect(uint8_t rhport)
{
  LPC_USB[rhport]->USBCMD_D |= BIT_(0);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  LPC_USB[rhport]->DEVICEADDR = (dev_addr << 25) | BIT_(24);
}

void dcd_set_config(uint8_t rhport, uint8_t config_num)
{

}

/// follows LPC43xx User Manual 23.10.3
static void bus_reset(uint8_t rhport)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[rhport];

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  // direction is enabled and the paired endpoint of opposite direction is disabled, then the
  // endpoint type of the unused direction must bechanged from the control type to any other
  // type (e.g. bulk). Leaving an unconfigured endpoint control will cause undefined behavior
  // for the data PID tracking on the active endpoint.
  lpc_usb->ENDPTCTRL1 = lpc_usb->ENDPTCTRL2 = lpc_usb->ENDPTCTRL3 = (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);

  // USB1 only has 3 non-control endpoints
  if ( rhport == 0)
  {
    lpc_usb->ENDPTCTRL4 = lpc_usb->ENDPTCTRL5 = (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);
  }

  //------------- Clear All Registers -------------//
  lpc_usb->ENDPTNAK       = lpc_usb->ENDPTNAK;
  lpc_usb->ENDPTNAKEN     = 0;
  lpc_usb->USBSTS_D       = lpc_usb->USBSTS_D;
  lpc_usb->ENDPTSETUPSTAT = lpc_usb->ENDPTSETUPSTAT;
  lpc_usb->ENDPTCOMPLETE  = lpc_usb->ENDPTCOMPLETE;

  while (lpc_usb->ENDPTPRIME);
  lpc_usb->ENDPTFLUSH = 0xFFFFFFFF;
  while (lpc_usb->ENDPTFLUSH);

  // read reset bit in portsc

  //------------- Queue Head & Queue TD -------------//
  dcd_data_t* p_dcd = dcd_data_ptr[rhport];

  tu_memclr(p_dcd, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
	p_dcd->qhd[0].zero_length_termination = p_dcd->qhd[1].zero_length_termination = 1;
	p_dcd->qhd[0].max_package_size = p_dcd->qhd[1].max_package_size = CFG_TUD_ENDOINT0_SIZE;
	p_dcd->qhd[0].qtd_overlay.next = p_dcd->qhd[1].qtd_overlay.next = QTD_NEXT_INVALID;

	p_dcd->qhd[0].int_on_setup = 1; // OUT only

}

bool dcd_init(uint8_t rhport)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[rhport];
  dcd_data_t* p_dcd = dcd_data_ptr[rhport];

  tu_memclr(p_dcd, sizeof(dcd_data_t));

  lpc_usb->ENDPOINTLISTADDR = (uint32_t) p_dcd->qhd; // Endpoint List Address has to be 2K alignment
  lpc_usb->USBSTS_D  = lpc_usb->USBSTS_D;
  lpc_usb->USBINTR_D = INT_MASK_USB | INT_MASK_ERROR | INT_MASK_PORT_CHANGE | INT_MASK_RESET | INT_MASK_SUSPEND | INT_MASK_SOF;

  lpc_usb->USBCMD_D &= ~0x00FF0000; // Interrupt Threshold Interval = 0
  lpc_usb->USBCMD_D |= BIT_(0); // connect

  // enable interrupt
  NVIC_EnableIRQ(rhport ? USB1_IRQn : USB0_IRQn);

  return true;
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
// index to bit position in register
static inline uint8_t ep_idx2bit(uint8_t ep_idx)
{
  return ep_idx/2 + ( (ep_idx%2) ? 16 : 0);
}

static void qtd_init(dcd_qtd_t* p_qtd, void * data_ptr, uint16_t total_bytes)
{
  tu_memclr(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->next        = QTD_NEXT_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;

  if (data_ptr != NULL)
  {
    p_qtd->buffer[0]   = (uint32_t) data_ptr;
    for(uint8_t i=1; i<5; i++)
    {
      p_qtd->buffer[i] |= tu_align4k( p_qtd->buffer[i-1] ) + 4096;
    }
  }
}

static inline volatile uint32_t * get_endpt_ctrl_reg(uint8_t rhport, uint8_t ep_idx)
{
 return &(LPC_USB[rhport]->ENDPTCTRL0) + ep_idx/2;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = edpt_number(ep_addr);
  uint8_t const dir    = edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  volatile uint32_t * endpt_ctrl = get_endpt_ctrl_reg(rhport, ep_idx);

  if ( epnum == 0)
  {
    // Stall both Control IN and OUT
    (*endpt_ctrl) |= ( (ENDPTCTRL_MASK_STALL << 16) || (ENDPTCTRL_MASK_STALL << 0) );
  }else
  {
    (*endpt_ctrl) |= ENDPTCTRL_MASK_STALL << (ep_idx & 0x01 ? 16 : 0);
  }
}

// TOOD implement later
bool dcd_edpt_stalled (uint8_t rhport, uint8_t ep_addr)
{
  return false;
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = edpt_number(ep_addr);
  uint8_t const dir    = edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  volatile uint32_t * endpt_ctrl = get_endpt_ctrl_reg(rhport, ep_idx);

  // data toggle also need to be reset
  (*endpt_ctrl) |= ENDPTCTRL_MASK_TOGGLE_RESET << ((ep_addr & TUSB_DIR_IN_MASK) ? 16 : 0);
  (*endpt_ctrl) &= ~(ENDPTCTRL_MASK_STALL << ((ep_addr & TUSB_DIR_IN_MASK) ? 16 : 0));
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  // TODO USB1 only has 4 non-control enpoint (USB0 has 5)
  // TODO not support ISO yet
  TU_VERIFY ( p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  uint8_t const epnum  = edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir    = edpt_dir(p_endpoint_desc->bEndpointAddress);
  uint8_t const ep_idx = 2*epnum + dir;

  //------------- Prepare Queue Head -------------//
  dcd_qhd_t * p_qhd = &dcd_data_ptr[rhport]->qhd[ep_idx];
  tu_memclr(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size        = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;

  //------------- Endpoint Control Register -------------//
  volatile uint32_t * endpt_ctrl = get_endpt_ctrl_reg(rhport, ep_idx);

  // endpoint must not be already enabled
  TU_VERIFY( !( (*endpt_ctrl) &  (ENDPTCTRL_MASK_ENABLE << (dir ? 16 : 0)) ) );

  (*endpt_ctrl) |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_MASK_ENABLE | ENDPTCTRL_MASK_TOGGLE_RESET) << (dir ? 16 : 0);

  return true;
}

bool dcd_edpt_busy(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = edpt_number(ep_addr);
  uint8_t const dir    = edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  dcd_qhd_t const * p_qhd = &dcd_data_ptr[rhport]->qhd[ep_idx];
  dcd_qtd_t * p_qtd = &dcd_data_ptr[rhport]->qtd[ep_idx];

  return p_qtd->active;
//  return !p_qhd->qtd_overlay.halted && p_qhd->qtd_overlay.active;
}

bool  dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  if ( epnum == 0 )
  {
    // follows UM 24.10.8.1.1 Setup packet handling using setup lockout mechanism
    // wait until ENDPTSETUPSTAT before priming data/status in response TODO add time out
    while(LPC_USB[rhport]->ENDPTSETUPSTAT & BIT_(0)) {}
  }

  dcd_data_t* p_dcd = dcd_data_ptr[rhport];
  dcd_qhd_t * p_qhd = &p_dcd->qhd[ep_idx];
  dcd_qtd_t * p_qtd = &p_dcd->qtd[ep_idx];

  //------------- Prepare qtd -------------//
  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->int_on_complete = true;
  p_qhd->qtd_overlay.next = (uint32_t) p_qtd; // link qtd to qhd

  // start transfer
  LPC_USB[rhport]->ENDPTPRIME = BIT_( ep_idx2bit(ep_idx) ) ;

  return true;
}


//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void hal_dcd_isr(uint8_t rhport)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[rhport];

  uint32_t const int_enable = lpc_usb->USBINTR_D;
  uint32_t const int_status = lpc_usb->USBSTS_D & int_enable;
  lpc_usb->USBSTS_D = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;// disabled interrupt sources


  if (int_status & INT_MASK_RESET)
  {
    bus_reset(rhport);

    dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_BUS_RESET };
    dcd_event_handler(&event, true);
  }

  if (int_status & INT_MASK_SUSPEND)
  {
    if (lpc_usb->PORTSC1_D & PORTSC_SUSPEND_MASK)
    { // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
      if ((lpc_usb->DEVICEADDR >> 25) & 0x0f)
      {
        dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SUSPENDED };
        dcd_event_handler(&event, true);
      }
    }
  }

  // TODO disconnection does not generate interrupt !!!!!!
//	if (int_status & INT_MASK_PORT_CHANGE)
//	{
//	  if ( !(lpc_usb->PORTSC1_D & PORTSC_CURRENT_CONNECT_STATUS_MASK) )
//	  {
//      dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED };
//      dcd_event_handler(&event, true);
//	  }
//	}

  if (int_status & INT_MASK_USB)
  {
    uint32_t const edpt_complete = lpc_usb->ENDPTCOMPLETE;
    lpc_usb->ENDPTCOMPLETE = edpt_complete; // acknowledge

    dcd_data_t* const p_dcd = dcd_data_ptr[rhport];

    if (lpc_usb->ENDPTSETUPSTAT)
    {
      //------------- Set up Received -------------//
      // 23.10.10.2 Operational model for setup transfers
      lpc_usb->ENDPTSETUPSTAT = lpc_usb->ENDPTSETUPSTAT;// acknowledge

      dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SETUP_RECEIVED };
      event.setup_received = p_dcd->qhd[0].setup_request;

      dcd_event_handler(&event, true);
    }

    if ( edpt_complete )
    {
      for(uint8_t ep_idx = 0; ep_idx < DCD_QHD_MAX; ep_idx++)
      {
        if ( BIT_TEST_(edpt_complete, ep_idx2bit(ep_idx)) )
        {
          // 23.10.12.3 Failed QTD also get ENDPTCOMPLETE set
          dcd_qhd_t * p_qhd = &dcd_data_ptr[rhport]->qhd[ep_idx];
          dcd_qtd_t * p_qtd = &dcd_data_ptr[rhport]->qtd[ep_idx];

          uint8_t result = p_qtd->halted  ? XFER_RESULT_STALLED :
              ( p_qtd->xact_err ||p_qtd->buffer_err ) ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS;

          uint8_t ep_addr = (ep_idx/2) | ( (ep_idx & 0x01) ? TUSB_DIR_IN_MASK : 0 );
          dcd_event_xfer_complete(rhport, ep_addr, p_qtd->expected_bytes - p_qtd->total_bytes, result, true); // only number of bytes in the IOC qtd
        }
      }
    }
  }

  if (int_status & INT_MASK_SOF)
  {
    dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SOF };
    dcd_event_handler(&event, true);
  }

  if (int_status & INT_MASK_NAK) {}
  if (int_status & INT_MASK_ERROR) TU_ASSERT(false, );
}

#endif
