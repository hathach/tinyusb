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

#if MODE_DEVICE_SUPPORTED && TUSB_CFG_MCU == MCU_LPC43XX

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "tusb_hal.h"
#include "osal/osal.h"
#include "common/timeout_timer.h"

#include "tusb_dcd.h"
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
  dcd_qhd_t qhd[DCD_QHD_MAX]; ///< Must be at 2K alignment
  dcd_qtd_t qtd[DCD_QTD_MAX] ATTR_ALIGNED(32);

}dcd_data_t;

extern ATTR_WEAK dcd_data_t dcd_data0;
extern ATTR_WEAK dcd_data_t dcd_data1;

#if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE)
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(2048) STATIC_VAR dcd_data_t dcd_data0;
#endif

#if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_DEVICE)
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(2048) STATIC_VAR dcd_data_t dcd_data1;
#endif

static LPC_USB0_Type * const LPC_USB[2] = { LPC_USB0, ((LPC_USB0_Type*) LPC_USB1_BASE) };
static dcd_data_t* const dcd_data_ptr[2] = { &dcd_data0, &dcd_data1 };

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void tusb_dcd_connect(uint8_t port)
{
  LPC_USB[port]->USBCMD_D |= BIT_(0);
}

void tusb_dcd_set_address(uint8_t port, uint8_t dev_addr)
{
  LPC_USB[port]->DEVICEADDR = (dev_addr << 25) | BIT_(24);
}

void tusb_dcd_set_config(uint8_t port, uint8_t config_num)
{

}

/// follows LPC43xx User Manual 23.10.3
static void bus_reset(uint8_t port)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[port];

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  //direction is enabled and the paired endpoint of opposite direction is disabled, then the
  //endpoint type of the unused direction must bechanged from the control type to any other
  //type (e.g. bulk). Leaving an unconfigured endpoint control will cause undefined behavior
  //for the data PID tracking on the active endpoint.
  lpc_usb->ENDPTCTRL1 = lpc_usb->ENDPTCTRL2 = lpc_usb->ENDPTCTRL3 =
      (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);

  // USB1 only has 3 non-control endpoints
  if ( port == 0)
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
  dcd_data_t* p_dcd = dcd_data_ptr[port];

  memclr_(p_dcd, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
	p_dcd->qhd[0].zero_length_termination = p_dcd->qhd[1].zero_length_termination = 1;
	p_dcd->qhd[0].max_package_size = p_dcd->qhd[1].max_package_size = TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE;
	p_dcd->qhd[0].qtd_overlay.next = p_dcd->qhd[1].qtd_overlay.next = QTD_NEXT_INVALID;

	p_dcd->qhd[0].int_on_setup = 1; // OUT only

}

bool tusb_dcd_init(uint8_t port)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[port];
  dcd_data_t* p_dcd = dcd_data_ptr[port];

  memclr_(p_dcd, sizeof(dcd_data_t));

  lpc_usb->ENDPOINTLISTADDR = (uint32_t) p_dcd->qhd; // Endpoint List Address has to be 2K alignment
  lpc_usb->USBSTS_D  = lpc_usb->USBSTS_D;
  lpc_usb->USBINTR_D = INT_MASK_USB | INT_MASK_ERROR | INT_MASK_PORT_CHANGE | INT_MASK_RESET | INT_MASK_SUSPEND | INT_MASK_SOF;

  lpc_usb->USBCMD_D &= ~0x00FF0000; // Interrupt Threshold Interval = 0
  lpc_usb->USBCMD_D |= BIT_(0); // connect

  return true;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
#if 0
static inline uint8_t edpt_pos2phy(uint8_t pos)
{ // 0-5 --> OUT, 16-21 IN
  return (pos < DCD_QHD_MAX/2) ? (2*pos) : (2*(pos-16)+1);
}
#endif

static inline uint8_t edpt_phy2pos(uint8_t physical_endpoint)
{
  return physical_endpoint/2 + ( (physical_endpoint%2) ? 16 : 0);
}

static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 1 : 0);
}

static inline uint8_t edpt_phy2addr(uint8_t ep_idx)
{
  return (ep_idx/2) | ( ep_idx & 0x01 ? TUSB_DIR_DEV_TO_HOST_MASK : 0 );
}

static inline uint8_t edpt_phy2log(uint8_t physical_endpoint)
{
  return physical_endpoint/2;
}

static void qtd_init(dcd_qtd_t* p_qtd, void * data_ptr, uint16_t total_bytes)
{
  memclr_(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->used        = 1;

  p_qtd->next        = QTD_NEXT_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;

  if (data_ptr != NULL)
  {
    p_qtd->buffer[0]   = (uint32_t) data_ptr;
    for(uint8_t i=1; i<5; i++)
    {
      p_qtd->buffer[i] |= align4k( p_qtd->buffer[i-1] ) + 4096;
    }
  }
}

// retval 0: invalid
static inline uint8_t qtd_find_free(uint8_t port)
{
  for(uint8_t i=2; i<DCD_QTD_MAX; i++)
  { // exclude control's qtd
    if ( dcd_data_ptr[port]->qtd[i].used == 0) return i;
  }

  return 0;
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
void tusb_dcd_control_stall(uint8_t port)
{
  LPC_USB[port]->ENDPTCTRL0 |= (ENDPTCTRL_MASK_STALL << 16); // stall Control IN TODO stall control OUT as well
}

// control transfer does not need to use qtd find function
// follows UM 24.10.8.1.1 Setup packet handling using setup lockout mechanism
bool tusb_dcd_control_xfer(uint8_t port, tusb_direction_t dir, uint8_t * p_buffer, uint16_t length, bool int_on_complete)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[port];
  dcd_data_t* const p_dcd      = dcd_data_ptr[port];

  // determine Endpoint where Data & Status phase occurred (IN or OUT)
  uint8_t const ep_data   = (dir == TUSB_DIR_DEV_TO_HOST) ? 1 : 0;
  uint8_t const ep_status = 1 - ep_data;

  while(lpc_usb->ENDPTSETUPSTAT & BIT_(0)) {} // wait until ENDPTSETUPSTAT before priming data/status in response TODO add time out
//  while(p_dcd->qhd[0].qtd_overlay.active || p_dcd->qhd[1].qtd_overlay.active) {}; // wait until previous device request is completed TODO add timeout

  VERIFY( !(p_dcd->qhd[0].qtd_overlay.active || p_dcd->qhd[1].qtd_overlay.active) );

  //------------- Data Phase -------------//
  if (length > 0)
  {
    dcd_qtd_t* p_qtd_data = &p_dcd->qtd[0];
    qtd_init(p_qtd_data, p_buffer, length);
    p_dcd->qhd[ep_data].qtd_overlay.next = (uint32_t) p_qtd_data;

    lpc_usb->ENDPTPRIME = BIT_(edpt_phy2pos(ep_data));
  }

  //------------- Status Phase -------------//
  dcd_qtd_t* p_qtd_status = &p_dcd->qtd[1];
  qtd_init(p_qtd_status, NULL, 0); // zero length xfer
  p_qtd_status->int_on_complete = int_on_complete ? 1 : 0;

  p_dcd->qhd[ep_status].qtd_overlay.next = (uint32_t) p_qtd_status;

  lpc_usb->ENDPTPRIME = BIT_(edpt_phy2pos(ep_status));

  return true;
}

//--------------------------------------------------------------------+
// BULK/INTERRUPT/ISOCHRONOUS PIPE API
//--------------------------------------------------------------------+
static inline volatile uint32_t * get_reg_control_addr(uint8_t port, uint8_t physical_endpoint)
{
 return &(LPC_USB[port]->ENDPTCTRL0) + edpt_phy2log(physical_endpoint);
}

void tusb_dcd_edpt_stall(uint8_t port, uint8_t edpt_addr)
{
  uint8_t ep_idx    = edpt_addr2phy(edpt_addr);
  volatile uint32_t * reg_control = get_reg_control_addr(port, ep_idx);

  (*reg_control) |= ENDPTCTRL_MASK_STALL << (ep_idx & 0x01 ? 16 : 0);
}

void tusb_dcd_edpt_clear_stall(uint8_t port, uint8_t edpt_addr)
{
  volatile uint32_t * reg_control = get_reg_control_addr(port, edpt_addr2phy(edpt_addr));

  // data toggle also need to be reset
  (*reg_control) |= ENDPTCTRL_MASK_TOGGLE_RESET << ((edpt_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 16 : 0);
  (*reg_control) &= ~(ENDPTCTRL_MASK_STALL << ((edpt_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 16 : 0));
}

bool tusb_dcd_edpt_open(uint8_t port, tusb_descriptor_endpoint_t const * p_endpoint_desc)
{
  // TODO USB1 only has 4 non-control enpoint (USB0 has 5)
  // TODO not support ISO yet
  VERIFY ( p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  tusb_direction_t dir = (p_endpoint_desc->bEndpointAddress & TUSB_DIR_DEV_TO_HOST_MASK) ? TUSB_DIR_DEV_TO_HOST : TUSB_DIR_HOST_TO_DEV;

  //------------- Prepare Queue Head -------------//
  uint8_t ep_idx    = edpt_addr2phy(p_endpoint_desc->bEndpointAddress);
  dcd_qhd_t * p_qhd = &dcd_data_ptr[port]->qhd[ep_idx];

  memclr_(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size        = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;

  //------------- Endpoint Control Register -------------//
  volatile uint32_t * reg_control = get_reg_control_addr(port, ep_idx);

  // endpoint must not be already enabled
  VERIFY( !( (*reg_control) &  (ENDPTCTRL_MASK_ENABLE << (dir ? 16 : 0)) ) );

  (*reg_control) |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_MASK_ENABLE | ENDPTCTRL_MASK_TOGGLE_RESET) << (dir ? 16 : 0);

  return true;
}

bool tusb_dcd_edpt_busy(uint8_t port, uint8_t edpt_addr)
{
  uint8_t ep_idx    = edpt_addr2phy(edpt_addr);
  dcd_qhd_t const * p_qhd = &dcd_data_ptr[port]->qhd[ep_idx];

  return p_qhd->list_qtd_idx[0] != 0; // qtd list is not empty
//  return !p_qhd->qtd_overlay.halted && p_qhd->qtd_overlay.active;
}

// add only, controller virtually cannot know
static tusb_error_t pipe_add_xfer(uint8_t port, uint8_t ed_idx, void * buffer, uint16_t total_bytes, bool int_on_complete)
{
  uint8_t qtd_idx  = qtd_find_free(port);
  ASSERT(qtd_idx != 0, TUSB_ERROR_DCD_NOT_ENOUGH_QTD);

  dcd_data_t* p_dcd = dcd_data_ptr[port];
  dcd_qhd_t * p_qhd = &p_dcd->qhd[ed_idx];
  dcd_qtd_t * p_qtd = &p_dcd->qtd[qtd_idx];

  //------------- Find free slot in qhd's array list -------------//
  uint8_t free_slot;
  for(free_slot=0; free_slot < DCD_QTD_PER_QHD_MAX; free_slot++)
  {
    if ( p_qhd->list_qtd_idx[free_slot] == 0 )  break; // found free slot
  }
  ASSERT(free_slot < DCD_QTD_PER_QHD_MAX, TUSB_ERROR_DCD_NOT_ENOUGH_QTD);

  p_qhd->list_qtd_idx[free_slot] = qtd_idx; // add new qtd to qhd's array list

  //------------- Prepare qtd -------------//
  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->int_on_complete = int_on_complete;

  if ( free_slot > 0 ) p_dcd->qtd[ p_qhd->list_qtd_idx[free_slot-1] ].next = (uint32_t) p_qtd;

  return TUSB_ERROR_NONE;
}

tusb_error_t tusb_dcd_edpt_queue_xfer(uint8_t port, uint8_t edpt_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t ep_idx = edpt_addr2phy(edpt_addr);
  return pipe_add_xfer(port, ep_idx, buffer, total_bytes, false);
}

tusb_error_t  tusb_dcd_edpt_xfer(uint8_t port, uint8_t edpt_addr, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete)
{
  uint8_t ep_idx = edpt_addr2phy(edpt_addr);

  ASSERT_STATUS ( pipe_add_xfer(port, ep_idx, buffer, total_bytes, int_on_complete) );

  dcd_qhd_t* p_qhd = &dcd_data_ptr[port]->qhd[ ep_idx ];
  dcd_qtd_t* p_qtd = &dcd_data_ptr[port]->qtd[ p_qhd->list_qtd_idx[0] ];

  p_qhd->qtd_overlay.next = (uint32_t) p_qtd; // attach head QTD to QHD start transferring

	LPC_USB[port]->ENDPTPRIME = BIT_( edpt_phy2pos(ep_idx) ) ;

	return TUSB_ERROR_NONE;
}

//------------- Device Controller Driver's Interrupt Handler -------------//
void xfer_complete_isr(uint8_t port, uint32_t reg_complete)
{
  for(uint8_t ep_idx = 2; ep_idx < DCD_QHD_MAX; ep_idx++)
  {
    if ( BIT_TEST_(reg_complete, edpt_phy2pos(ep_idx)) )
    { // 23.10.12.3 Failed QTD also get ENDPTCOMPLETE set
      dcd_qhd_t * p_qhd = &dcd_data_ptr[port]->qhd[ep_idx];

      // retire all QTDs in array list, up to 1st still-active QTD
      while( p_qhd->list_qtd_idx[0] != 0 )
      {
        dcd_qtd_t * p_qtd = &dcd_data_ptr[port]->qtd[ p_qhd->list_qtd_idx[0] ];

        if (p_qtd->active)  break; // stop immediately if found still-active QTD and shift array list

        //------------- Free QTD and shift array list -------------//
        p_qtd->used = 0; // free QTD
        memmove( (void*) p_qhd->list_qtd_idx, (void*) (p_qhd->list_qtd_idx+1), DCD_QTD_PER_QHD_MAX-1);
        p_qhd->list_qtd_idx[DCD_QTD_PER_QHD_MAX-1]=0;

        if (p_qtd->int_on_complete)
        {
          bool succeeded = ( p_qtd->xact_err || p_qtd->halted || p_qtd->buffer_err ) ? false : true;

          uint8_t edpt_addr = edpt_phy2addr(ep_idx);
          tusb_dcd_xfer_complete(port, edpt_addr, p_qtd->expected_bytes - p_qtd->total_bytes, succeeded); // only number of bytes in the IOC qtd
        }
      }
    }
  }
}

void hal_dcd_isr(uint8_t port)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[port];

  uint32_t const int_enable = lpc_usb->USBINTR_D;
  uint32_t const int_status = lpc_usb->USBSTS_D & int_enable;
  lpc_usb->USBSTS_D = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;// disabled interrupt sources

  if (int_status & INT_MASK_RESET)
  {
    bus_reset(port);
    tusb_dcd_bus_event(port, USBD_BUS_EVENT_RESET);
  }

  if (int_status & INT_MASK_SUSPEND)
  {
    if (lpc_usb->PORTSC1_D & PORTSC_SUSPEND_MASK)
    { // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
      if ((lpc_usb->DEVICEADDR >> 25) & 0x0f)
      {
        tusb_dcd_bus_event(0, USBD_BUS_EVENT_SUSPENDED);
      }
    }
  }

  // TODO disconnection does not generate interrupt !!!!!!
//	if (int_status & INT_MASK_PORT_CHANGE)
//	{
//	  if ( !(lpc_usb->PORTSC1_D & PORTSC_CURRENT_CONNECT_STATUS_MASK) )
//	  {
//	    tusb_dcd_bus_event(0, USBD_BUS_EVENT_UNPLUGGED);
//	  }
//	}

  if (int_status & INT_MASK_USB)
  {
    uint32_t const edpt_complete = lpc_usb->ENDPTCOMPLETE;
    lpc_usb->ENDPTCOMPLETE = edpt_complete; // acknowledge

    dcd_data_t* const p_dcd = dcd_data_ptr[port];

    //------------- Set up Received -------------//
    if (lpc_usb->ENDPTSETUPSTAT)
    { // 23.10.10.2 Operational model for setup transfers
      lpc_usb->ENDPTSETUPSTAT = lpc_usb->ENDPTSETUPSTAT;// acknowledge

      tusb_dcd_setup_received(port, (uint8_t*) &p_dcd->qhd[0].setup_request);
    }
    //------------- Control Request Completed -------------//
    else if ( edpt_complete & 0x03 )
    {
      for(uint8_t ep_idx = 0; ep_idx < 2; ep_idx++)
      {
        if ( BIT_TEST_(edpt_complete, edpt_phy2pos(ep_idx)) )
        {
          // TODO use the actual QTD instead of the qhd's overlay to get expected bytes for actual byte xferred
          dcd_qtd_t volatile * const p_qtd = &p_dcd->qhd[ep_idx].qtd_overlay;

          if ( p_qtd->int_on_complete )
          {
            bool succeeded = ( p_qtd->xact_err || p_qtd->halted || p_qtd->buffer_err ) ? false : true;
            tusb_dcd_xfer_complete(port, 0, 0, succeeded); // TODO xferred bytes for control xfer is not needed yet !!!!
          }
        }
      }
    }

    //------------- Transfer Complete -------------//
    if ( edpt_complete & ~(0x03UL) )
    {
      xfer_complete_isr(port, edpt_complete);
    }
  }

  if (int_status & INT_MASK_SOF)
  {
    tusb_dcd_bus_event(port, USBD_BUS_EVENT_SOF);
  }

  if (int_status & INT_MASK_NAK) {}
  if (int_status & INT_MASK_ERROR) ASSERT(false, VOID_RETURN);
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
#endif
