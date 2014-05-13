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

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hal/hal.h"
#include "osal/osal.h"
#include "common/timeout_timer.h"

#include "dcd.h"
#include "usbd_dcd.h"
#include "dcd_lpc43xx.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_QHD_MAX 12
#define DCD_QTD_MAX 12
#define DCD_QTD_PER_QHD_MAX 2 // maximum number of qtd that are linked into one queue head at a time

#define QTD_NEXT_INVALID 0x01

/*---------- ENDPTCTRL ----------*/
enum {
  ENDPTCTRL_MASK_STALL          = BIT_(0),
  ENDPTCTRL_MASK_TOGGLE_INHIBIT = BIT_(5), ///< used for test only
  ENDPTCTRL_MASK_TOGGLE_RESET   = BIT_(6),
  ENDPTCTRL_MASK_ENABLE         = BIT_(7)
};

/*---------- USBCMD ----------*/
enum {
  USBCMD_MASK_RUN_STOP         = BIT_(0),
  USBCMD_MASK_RESET            = BIT_(1),
  USBCMD_MASK_SETUP_TRIPWIRE   = BIT_(13),
  USBCMD_MASK_ADD_QTD_TRIPWIRE = BIT_(14)  ///< This bit is used as a semaphore to ensure the to proper addition of a new dTD to an active (primed) endpoint’s linked list. This bit is set and cleared by software during the process of adding a new dTD
};
// Interrupt Threshold bit 23:16

/*---------- USBSTS, USBINTR ----------*/
enum {
  INT_MASK_USB         = BIT_(0),
  INT_MASK_ERROR       = BIT_(1),
  INT_MASK_PORT_CHANGE = BIT_(2),
  INT_MASK_RESET       = BIT_(6),
  INT_MASK_SOF         = BIT_(7),
  INT_MASK_SUSPEND     = BIT_(8),
  INT_MASK_NAK         = BIT_(16)
};

//------------- PORTSC -------------//
enum {
  PORTSC_CURRENT_CONNECT_STATUS_MASK = BIT_(0),
  PORTSC_FORCE_PORT_RESUME_MASK      = BIT_(6),
  PORTSC_SUSPEND_MASK                = BIT_(7)

};

typedef struct {
  // Word 0: Next QTD Pointer
  uint32_t next; ///< Next link pointer This field contains the physical memory address of the next dTD to be processed

  // Word 1: qTQ Token
  uint32_t                      : 3  ;
  volatile uint32_t xact_err    : 1  ;
  uint32_t                      : 1  ;
  volatile uint32_t buffer_err  : 1  ;
  volatile uint32_t halted      : 1  ;
  volatile uint32_t active      : 1  ;
  uint32_t                      : 2  ;
  uint32_t iso_mult_override    : 2  ; ///< This field can be used for transmit ISOs to override the MULT field in the dQH. This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                      : 3  ;
  uint32_t int_on_complete      : 1  ;
  volatile uint32_t total_bytes : 15 ;
  uint32_t                      : 0  ;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //------------- DCD Area -------------//
  uint16_t expected_bytes;
  uint8_t used;
  uint8_t reserved;
} dcd_qtd_t;

STATIC_ASSERT( sizeof(dcd_qtd_t) == 32, "size is not correct");

typedef struct ATTR_ALIGNED(64) {
  // Word 0: Capabilities and Characteristics
  uint32_t                         : 15 ; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated by the USB variable length protocol where N is computed using Max_packet_length and the Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 - Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark: Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1  ; ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is set in response to a setup being received.
  uint32_t max_package_size        : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
  uint32_t                         : 2  ;
  uint32_t zero_length_termination : 1  ; ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on transfers that are equal in length to a multiple Max_packet_length.
  uint32_t iso_mult                : 2  ; ///<
  uint32_t                         : 0  ;

  // Word 1: Current qTD Pointer
	volatile uint32_t qtd_addr;

	// Word 2-9: Transfer Overlay
	volatile dcd_qtd_t qtd_overlay;

	// Word 10-11: Setup request (control OUT only)
	volatile tusb_control_request_t setup_request;

	//--------------------------------------------------------------------+
  /// Due to the fact QHD is 64 bytes aligned but occupies only 48 bytes
	/// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
  uint8_t class_code; // Class code that endpoint belongs to
  volatile uint8_t list_qtd_idx[DCD_QTD_PER_QHD_MAX];

	uint8_t reserved[15-DCD_QTD_PER_QHD_MAX];
} dcd_qhd_t;

STATIC_ASSERT( sizeof(dcd_qhd_t) == 64, "size is not correct");

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
void dcd_controller_connect(uint8_t coreid)
{
  LPC_USB[coreid]->USBCMD_D |= BIT_(0);
}

void dcd_controller_set_address(uint8_t coreid, uint8_t dev_addr)
{
  LPC_USB[coreid]->DEVICEADDR = (dev_addr << 25) | BIT_(24);
}

void dcd_controller_set_configuration(uint8_t coreid)
{

}

/// follows LPC43xx User Manual 23.10.3
static void bus_reset(uint8_t coreid)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[coreid];

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  //direction is enabled and the paired endpoint of opposite direction is disabled, then the
  //endpoint type of the unused direction must bechanged from the control type to any other
  //type (e.g. bulk). Leaving an unconfigured endpoint control will cause undefined behavior
  //for the data PID tracking on the active endpoint.
  lpc_usb->ENDPTCTRL1 = lpc_usb->ENDPTCTRL2 = lpc_usb->ENDPTCTRL3 =
      (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);

  // USB1 only has 3 non-control endpoints
  if ( coreid == 0)
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
  dcd_data_t* p_dcd = dcd_data_ptr[coreid];

  memclr_(p_dcd, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
	p_dcd->qhd[0].zero_length_termination = p_dcd->qhd[1].zero_length_termination = 1;
	p_dcd->qhd[0].max_package_size = p_dcd->qhd[1].max_package_size = TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE;
	p_dcd->qhd[0].qtd_overlay.next = p_dcd->qhd[1].qtd_overlay.next = QTD_NEXT_INVALID;

	p_dcd->qhd[0].int_on_setup = 1; // OUT only

}

static void lpc43xx_controller_init(uint8_t coreid)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[coreid];
  dcd_data_t* p_dcd = dcd_data_ptr[coreid];

  memclr_(p_dcd, sizeof(dcd_data_t));

  lpc_usb->ENDPOINTLISTADDR = (uint32_t) p_dcd->qhd; // Endpoint List Address has to be 2K alignment
  lpc_usb->USBSTS_D  = lpc_usb->USBSTS_D;
  lpc_usb->USBINTR_D = INT_MASK_USB | INT_MASK_ERROR | INT_MASK_PORT_CHANGE | INT_MASK_RESET | INT_MASK_SUSPEND; // | INT_MASK_SOF;

  lpc_usb->USBCMD_D &= ~0x00FF0000; // Interrupt Threshold Interval = 0
  lpc_usb->USBCMD_D |= BIT_(0); // connect
}

tusb_error_t dcd_init(void)
{
  #if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE)
  lpc43xx_controller_init(0);
  #endif

  #if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_DEVICE)
  lpc43xx_controller_init(1);
  #endif

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
#if 0
static inline uint8_t edpt_pos2phy(uint8_t pos) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_pos2phy(uint8_t pos)
{ // 0-5 --> OUT, 16-21 IN
  return (pos < DCD_QHD_MAX/2) ? (2*pos) : (2*(pos-16)+1);
}
#endif

static inline uint8_t edpt_phy2pos(uint8_t physical_endpoint) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_phy2pos(uint8_t physical_endpoint)
{
  return physical_endpoint/2 + ( (physical_endpoint%2) ? 16 : 0);
}

static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 1 : 0);
}

static inline uint8_t edpt_phy2log(uint8_t physical_endpoint) ATTR_CONST ATTR_ALWAYS_INLINE;
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
static inline uint8_t qtd_find_free(uint8_t coreid) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t qtd_find_free(uint8_t coreid)
{
  for(uint8_t i=2; i<DCD_QTD_MAX; i++)
  { // exclude control's qtd
    if ( dcd_data_ptr[coreid]->qtd[i].used == 0) return i;
  }

  return 0;
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
void dcd_pipe_control_stall(uint8_t coreid)
{
  LPC_USB[coreid]->ENDPTCTRL0 |= (ENDPTCTRL_MASK_STALL << 16); // stall Control IN TODO stall control OUT as well
}

// control transfer does not need to use qtd find function
// follows UM 24.10.8.1.1 Setup packet handling using setup lockout mechanism
tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, uint8_t * p_buffer, uint16_t length, bool int_on_complete)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[coreid];
  dcd_data_t* const p_dcd      = dcd_data_ptr[coreid];

  // determine Endpoint where Data & Status phase occurred (IN or OUT)
  uint8_t const ep_data   = (dir == TUSB_DIR_DEV_TO_HOST) ? 1 : 0;
  uint8_t const ep_status = 1 - ep_data;

  while(lpc_usb->ENDPTSETUPSTAT & BIT_(0)) {} // wait until ENDPTSETUPSTAT before priming data/status in response TODO add time out
//  while(p_dcd->qhd[0].qtd_overlay.active || p_dcd->qhd[1].qtd_overlay.active) {}; // wait until previous device request is completed TODO add timeout

  ASSERT_FALSE(p_dcd->qhd[0].qtd_overlay.active || p_dcd->qhd[1].qtd_overlay.active, TUSB_ERROR_FAILED);

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

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INTERRUPT/ISOCHRONOUS PIPE API
//--------------------------------------------------------------------+
static inline volatile uint32_t * get_reg_control_addr(uint8_t coreid, uint8_t physical_endpoint) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline volatile uint32_t * get_reg_control_addr(uint8_t coreid, uint8_t physical_endpoint)
{
 return &(LPC_USB[coreid]->ENDPTCTRL0) + edpt_phy2log(physical_endpoint);
}

tusb_error_t dcd_pipe_stall(endpoint_handle_t edpt_hdl)
{
  volatile uint32_t * reg_control = get_reg_control_addr(edpt_hdl.coreid, edpt_hdl.index);

  (*reg_control) |= ENDPTCTRL_MASK_STALL << (edpt_hdl.index & 0x01 ? 16 : 0);

  return TUSB_ERROR_NONE;
}

tusb_error_t dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr)
{
  volatile uint32_t * reg_control = get_reg_control_addr(coreid, edpt_addr2phy(edpt_addr));

  // data toggle also need to be reset
  (*reg_control) |= ENDPTCTRL_MASK_TOGGLE_RESET << ((edpt_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 16 : 0);
  (*reg_control) &= ~(ENDPTCTRL_MASK_STALL << ((edpt_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 16 : 0));

  return TUSB_ERROR_NONE;
}

endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  // TODO USB1 only has 4 non-control enpoint (USB0 has 5)
  endpoint_handle_t const null_handle = { 0 };

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return null_handle; // TODO not support ISO yet

  tusb_direction_t dir = (p_endpoint_desc->bEndpointAddress & TUSB_DIR_DEV_TO_HOST_MASK) ? TUSB_DIR_DEV_TO_HOST : TUSB_DIR_HOST_TO_DEV;

  //------------- Prepare Queue Head -------------//
  uint8_t ep_idx    = edpt_addr2phy(p_endpoint_desc->bEndpointAddress);
  dcd_qhd_t * p_qhd = &dcd_data_ptr[coreid]->qhd[ep_idx];

  memclr_(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->class_code              = class_code;
  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size        = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;

  //------------- Endpoint Control Register -------------//
  volatile uint32_t * reg_control = get_reg_control_addr(coreid, ep_idx);

  ASSERT_FALSE( (*reg_control) &  (ENDPTCTRL_MASK_ENABLE << (dir ? 16 : 0)), null_handle ); // endpoint must not be already enabled
  (*reg_control) |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_MASK_ENABLE | ENDPTCTRL_MASK_TOGGLE_RESET) << (dir ? 16 : 0);

  return (endpoint_handle_t)
      {
          .coreid     = coreid,
          .index      = ep_idx,
          .class_code = class_code
      };
}

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl)
{
  dcd_qhd_t const * p_qhd = &dcd_data_ptr[edpt_hdl.coreid]->qhd[edpt_hdl.index];

  return p_qhd->list_qtd_idx[0] != 0; // qtd list is not empty
//  return !p_qhd->qtd_overlay.halted && p_qhd->qtd_overlay.active;
}

// add only, controller virtually cannot know
static tusb_error_t pipe_add_xfer(endpoint_handle_t edpt_hdl, void * buffer, uint16_t total_bytes, bool int_on_complete)
{
  uint8_t qtd_idx  = qtd_find_free(edpt_hdl.coreid);
  ASSERT(qtd_idx != 0, TUSB_ERROR_DCD_NOT_ENOUGH_QTD);

  dcd_data_t* p_dcd = dcd_data_ptr[edpt_hdl.coreid];
  dcd_qhd_t * p_qhd = &p_dcd->qhd[edpt_hdl.index];
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

tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes)
{
  return pipe_add_xfer( edpt_hdl, buffer, total_bytes, false);
}

tusb_error_t  dcd_pipe_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete)
{
  ASSERT_STATUS ( pipe_add_xfer(edpt_hdl, buffer, total_bytes, int_on_complete) );

  dcd_qhd_t* p_qhd = &dcd_data_ptr[edpt_hdl.coreid]->qhd[ edpt_hdl.index ];
  dcd_qtd_t* p_qtd = &dcd_data_ptr[edpt_hdl.coreid]->qtd[ p_qhd->list_qtd_idx[0] ];

  p_qhd->qtd_overlay.next = (uint32_t) p_qtd; // attach head QTD to QHD start transferring

	LPC_USB[edpt_hdl.coreid]->ENDPTPRIME = BIT_( edpt_phy2pos(edpt_hdl.index) ) ;

	return TUSB_ERROR_NONE;
}

//------------- Device Controller Driver's Interrupt Handler -------------//
void xfer_complete_isr(uint8_t coreid, uint32_t reg_complete)
{
  for(uint8_t ep_idx = 2; ep_idx < DCD_QHD_MAX; ep_idx++)
  {
    if ( BIT_TEST_(reg_complete, edpt_phy2pos(ep_idx)) )
    { // 23.10.12.3 Failed QTD also get ENDPTCOMPLETE set
      dcd_qhd_t * p_qhd = &dcd_data_ptr[coreid]->qhd[ep_idx];

      endpoint_handle_t edpt_hdl =
      {
          .coreid     = coreid,
          .index      = ep_idx,
          .class_code = p_qhd->class_code
      };

      // retire all QTDs in array list, up to 1st still-active QTD
      while( p_qhd->list_qtd_idx[0] != 0 )
      {
        dcd_qtd_t * p_qtd = &dcd_data_ptr[coreid]->qtd[ p_qhd->list_qtd_idx[0] ];

        if (p_qtd->active)  break; // stop immediately if found still-active QTD and shift array list

        //------------- Free QTD and shift array list -------------//
        p_qtd->used = 0; // free QTD
        memmove( (void*) p_qhd->list_qtd_idx, (void*) (p_qhd->list_qtd_idx+1), DCD_QTD_PER_QHD_MAX-1);
        p_qhd->list_qtd_idx[DCD_QTD_PER_QHD_MAX-1]=0;

        if (p_qtd->int_on_complete)
        {
          tusb_event_t event = ( p_qtd->xact_err || p_qtd->halted || p_qtd->buffer_err ) ? TUSB_EVENT_XFER_ERROR : TUSB_EVENT_XFER_COMPLETE;
          usbd_xfer_isr(edpt_hdl, event, p_qtd->expected_bytes - p_qtd->total_bytes); // only number of bytes in the IOC qtd
        }
      }
    }
  }
}

void dcd_isr(uint8_t coreid)
{
  LPC_USB0_Type* const lpc_usb = LPC_USB[coreid];

  uint32_t const int_enable = lpc_usb->USBINTR_D;
  uint32_t const int_status = lpc_usb->USBSTS_D & int_enable;
  lpc_usb->USBSTS_D = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;// disabled interrupt sources

  if (int_status & INT_MASK_RESET)
  {
    bus_reset(coreid);
    usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_RESET);
  }

  if (int_status & INT_MASK_SUSPEND)
  {
    if (lpc_usb->PORTSC1_D & PORTSC_SUSPEND_MASK)
    { // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
      if ((lpc_usb->DEVICEADDR >> 25) & 0x0f)
      {
        usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_SUSPENDED);
      }
    }
  }

  // TODO disconnection does not generate interrupt !!!!!!
//	if (int_status & INT_MASK_PORT_CHANGE)
//	{
//	  if ( !(lpc_usb->PORTSC1_D & PORTSC_CURRENT_CONNECT_STATUS_MASK) )
//	  {
//	    usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_UNPLUGGED);
//	  }
//	}

  if (int_status & INT_MASK_USB)
  {
    uint32_t const edpt_complete = lpc_usb->ENDPTCOMPLETE;
    lpc_usb->ENDPTCOMPLETE = edpt_complete; // acknowledge

    dcd_data_t* const p_dcd = dcd_data_ptr[coreid];

    //------------- Set up Received -------------//
    if (lpc_usb->ENDPTSETUPSTAT)
    { // 23.10.10.2 Operational model for setup transfers
      tusb_control_request_t control_request = p_dcd->qhd[0].setup_request;
      lpc_usb->ENDPTSETUPSTAT = lpc_usb->ENDPTSETUPSTAT;// acknowledge

      usbd_setup_received_isr(coreid, &control_request);
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
            endpoint_handle_t edpt_hdl =
            {
                .coreid = coreid,
                .index = 0,
                .class_code = 0
            };
            tusb_event_t event = ( p_qtd->xact_err || p_qtd->halted || p_qtd->buffer_err ) ? TUSB_EVENT_XFER_ERROR : TUSB_EVENT_XFER_COMPLETE;

            usbd_xfer_isr(edpt_hdl, event, 0); // TODO xferred bytes for control xfer is not needed yet !!!!
          }
        }
      }
    }

    //------------- Transfer Complete -------------//
    if ( edpt_complete & ~(0x03UL) )
    {
      xfer_complete_isr(coreid, edpt_complete);
    }
  }

  if (int_status & INT_MASK_SOF) {}
  if (int_status & INT_MASK_NAK) {}
  if (int_status & INT_MASK_ERROR) ASSERT(false, VOID_RETURN);
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
#endif
