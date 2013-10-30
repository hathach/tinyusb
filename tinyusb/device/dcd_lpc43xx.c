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

#if MODE_DEVICE_SUPPORTED && MCU == MCU_LPC43XX

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
#define QTD_INVALID 0x01
#define CONTROL_ENDOINT_SIZE  64

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
  uint32_t mult_override        : 2  ; ///< This field can be used for transmit ISOs to override the MULT field in the dQH. This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                      : 3  ;
  uint32_t int_on_complete      : 1  ;
  volatile uint32_t total_bytes : 15 ;
  uint32_t                      : 0  ;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //-------------  -------------//
  uint32_t reserved;
} dcd_qtd_t;

STATIC_ASSERT( sizeof(dcd_qtd_t) == 32, "size is not correct");

typedef struct {
  // Word 0: Capabilities and Characteristics
  uint32_t                         : 15 ; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated by the USB variable length protocol where N is computed using Max_packet_length and the Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 - Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark: Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1  ; ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is set in response to a setup being received.
  uint32_t max_package_size        : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
  uint32_t                         : 2  ;
  uint32_t zero_length_termination : 1  ; ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on transfers that are equal in length to a multiple Max_packet_length.
  uint32_t mult                    : 2  ; ///<
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
	uint8_t reserved[16];

} ATTR_ALIGNED(64) dcd_qhd_t;

STATIC_ASSERT( sizeof(dcd_qhd_t) == 64, "size is not correct");

typedef struct {
  dcd_qhd_t qhd[DCD_QHD_MAX]; ///< Must be at 2K alignment
  dcd_qtd_t qtd[DCD_QHD_MAX] ATTR_ALIGNED(32);

}dcd_data_t;

ATTR_ALIGNED(2048) dcd_data_t dcd_data TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD-DCD API
//--------------------------------------------------------------------+
tusb_error_t dcd_controller_reset(uint8_t coreid)
{
  volatile uint32_t * p_reg_usbcmd;

  p_reg_usbcmd = (coreid ? &LPC_USB1->USBCMD_D : &LPC_USB0->USBCMD_D);
// NXP chip powered with non-host mode --> sts bit is not correctly reflected
  (*p_reg_usbcmd) |= BIT_(1); // TODO refractor reset controller

//  timeout_timer_t timeout;
//  timeout_set(&timeout, 2); // should not take longer the time to stop controller
  while( ((*p_reg_usbcmd) & BIT_(1)) /*&& !timeout_expired(&timeout)*/) {}
//
//  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
  return TUSB_ERROR_NONE;
}

void dcd_controller_connect(uint8_t coreid)
{
  volatile uint32_t * p_reg_usbcmd = (coreid ? &LPC_USB1->USBCMD_D : &LPC_USB0->USBCMD_D);

  (*p_reg_usbcmd) |= BIT_(0);
}

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

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void dcd_controller_set_address(uint8_t coreid, uint8_t dev_addr)
{
  LPC_USB0->DEVICEADDR = (dev_addr << 25) | BIT_(24);
}

void dcd_controller_set_configuration(uint8_t coreid, uint8_t config_num)
{

}

/// follows LPC43xx User Manual 23.10.3
void bus_reset(uint8_t coreid)
{
  // TODO mutliple core id support

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  //direction is enabled and the paired endpoint of opposite direction is disabled, then the
  //endpoint type of the unused direction must bechanged from the control type to any other
  //type (e.g. bulk). Leaving an unconfigured endpoint control will cause undefined behavior
  //for the data PID tracking on the active endpoint.
  LPC_USB0->ENDPTCTRL1 = LPC_USB0->ENDPTCTRL2 = LPC_USB0->ENDPTCTRL3 = LPC_USB0->ENDPTCTRL4 = LPC_USB0->ENDPTCTRL5 =
      (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);

  //------------- Clear All Registers -------------//
  LPC_USB0->ENDPTNAK       = LPC_USB0->ENDPTNAK;
  LPC_USB0->ENDPTNAKEN     = 0;
  LPC_USB0->USBSTS_D       = LPC_USB0->USBSTS_D;
  LPC_USB0->ENDPTSETUPSTAT = LPC_USB0->ENDPTSETUPSTAT;
  LPC_USB0->ENDPTCOMPLETE  = LPC_USB0->ENDPTCOMPLETE;

  while (LPC_USB0->ENDPTPRIME);
  LPC_USB0->ENDPTFLUSH = 0xFFFFFFFF;
  while (LPC_USB0->ENDPTFLUSH);

  // read reset bit in portsc

  //------------- Queue Head & Queue TD -------------//
  memclr_(&dcd_data, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
	dcd_data.qhd[0].zero_length_termination = dcd_data.qhd[1].zero_length_termination = 1;
	dcd_data.qhd[0].max_package_size = dcd_data.qhd[1].max_package_size = CONTROL_ENDOINT_SIZE;
	dcd_data.qhd[0].qtd_overlay.next = dcd_data.qhd[1].qtd_overlay.next = QTD_INVALID;

	dcd_data.qhd[0].int_on_setup = 1; // OUT only
}

tusb_error_t dcd_init(void)
{
	/* Set the interrupt Threshold control interval to 0 */
	LPC_USB0->USBCMD_D &= ~0x00FF0000;

	/* Configure the Endpoint List Address */ /* make sure it in on 2K boundary !!! */
	LPC_USB0->ENDPOINTLISTADDR = (uint32_t) dcd_data.qhd;

	/* Enable interrupts: USB interrupt, error, port change, reset, suspend, NAK interrupt */
	LPC_USB0->USBINTR_D =  INT_MASK_USB | INT_MASK_ERROR | INT_MASK_PORT_CHANGE | INT_MASK_RESET | INT_MASK_SUSPEND; // | INT_MASK_SOF| INT_MASK_NAK;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PIPE API
//--------------------------------------------------------------------+
static inline uint8_t endpoint_to_pos(uint8_t logical_endpoint, tusb_direction_t dir) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t endpoint_to_pos(uint8_t logical_endpoint, tusb_direction_t dir)
{
  return logical_endpoint + (dir == TUSB_DIR_HOST_TO_DEV ? 0 : 16);
}

static inline uint8_t endpoint_phy2pos(uint8_t physical_endpoint) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t endpoint_phy2pos(uint8_t physical_endpoint)
{
  return physical_endpoint/2 + ( (physical_endpoint%2) ? 16 : 0);
}

static inline uint8_t endpoint_log2phy(uint8_t logical_endpoint, tusb_direction_t dir) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t endpoint_log2phy(uint8_t logical_endpoint, tusb_direction_t dir)
{
  return 2*logical_endpoint + (dir == TUSB_DIR_DEV_TO_HOST ? 1 : 0);
}

static inline uint8_t endpoint_addr2phy(uint8_t endpoint_addr) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t endpoint_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 1 : 0);
}

static void qtd_init(dcd_qtd_t* p_qtd, void * data_ptr, uint16_t total_bytes)
{
  memclr_(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->next        = QTD_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = total_bytes;

  if (data_ptr != NULL)
  {
    p_qtd->buffer[0]   = (uint32_t) data_ptr;
    for(uint8_t i=1; i<5; i++)
    {
      p_qtd->buffer[i] |= align4k( p_qtd->buffer[i-1] ) + 4096;
    }
  }
}

tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, void * buffer, uint16_t length)
{
  uint8_t const endpoint_data = (dir == TUSB_DIR_DEV_TO_HOST) ? 1 : 0; // IN xfer --> data phase on Control IN, other Control OUT

  //------------- Data Phase -------------//
  if (length)
  {
    dcd_qtd_t* p_data = &dcd_data.qtd[0];
    qtd_init(p_data, buffer, length);
    dcd_data.qhd[endpoint_data].qtd_overlay.next = (uint32_t) p_data;

    LPC_USB0->ENDPTPRIME |= BIT_( endpoint_phy2pos(endpoint_data) );
  }

  //------------- Status Phase (other endpoint, opposite direction) -------------//
  dcd_qtd_t* p_status = &dcd_data.qtd[1];
  qtd_init(p_status, NULL, 0); // zero length xfer
  dcd_data.qhd[1 - endpoint_data].qtd_overlay.next = (uint32_t) p_status;

  LPC_USB0->ENDPTPRIME |= BIT_( endpoint_phy2pos(1 - endpoint_data) );

  return TUSB_ERROR_NONE;
}

endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc)
{
  // TODO USB1 only has 4 non-control enpoint (USB0 has 5)
  endpoint_handle_t const null_handle = { .coreid = 0, .xfer_type = 0, .index = 0 };

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return null_handle; // TODO not support ISO yet

  //------------- Prepare Queue Head -------------//
  uint8_t ep_idx = endpoint_addr2phy(p_endpoint_desc->bEndpointAddress);
  dcd_qhd_t * p_qhd = &dcd_data.qhd[ep_idx];

  memclr_(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next = QTD_INVALID;

  //------------- Endpoint Control Register -------------//
  volatile uint32_t * reg_control = (&LPC_USB0->ENDPTCTRL0) + (p_endpoint_desc->bEndpointAddress & 0x0f);
  (*reg_control) |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_MASK_ENABLE | ENDPTCTRL_MASK_TOGGLE_RESET) << ((p_endpoint_desc->bEndpointAddress & TUSB_DIR_DEV_TO_HOST_MASK) ? 16 : 0);

  return (endpoint_handle_t) { .coreid = coreid, .xfer_type = p_endpoint_desc->bmAttributes.xfer, .index = ep_idx };
}

STATIC_ INLINE_ dcd_qhd_t*  qhd_get_from_endpoint_handle(endpoint_handle_t edpt_hdl) ATTR_PURE ATTR_ALWAYS_INLINE;
STATIC_ INLINE_ dcd_qhd_t*  qhd_get_from_endpoint_handle(endpoint_handle_t edpt_hdl)
{
  return &dcd_data.qhd[edpt_hdl.index];
}

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl)
{
  dcd_qhd_t* p_qhd = qhd_get_from_endpoint_handle(edpt_hdl);

  // LPC_USB0->ENDPTSTAT & endpoint_phy2pos(edpt_hdl.index)
  return !p_qhd->qtd_overlay.halted && p_qhd->qtd_overlay.active;
}

tusb_error_t  dcd_pipe_xfer(endpoint_handle_t edpt_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  dcd_qhd_t* p_qhd = qhd_get_from_endpoint_handle(edpt_hdl);
  dcd_qtd_t* p_qtd = &dcd_data.qtd[edpt_hdl.index]; // TODO allocate qtd

  ASSERT(edpt_hdl.xfer_type != TUSB_XFER_ISOCHRONOUS, TUSB_ERROR_NOT_SUPPORTED_YET);

  //------------- Prepare qtd -------------//
  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->int_on_complete = int_on_complete;

  p_qhd->qtd_overlay.next = (uint32_t) p_qtd;

	LPC_USB0->ENDPTPRIME |= BIT_( endpoint_phy2pos(edpt_hdl.index) ) ;

	return TUSB_ERROR_NONE;
}

//------------- Device Controller Driver's Interrupt Handler -------------//
void dcd_isr(uint8_t coreid)
{
	uint32_t int_status = LPC_USB0->USBSTS_D;
	int_status &= LPC_USB0->USBINTR_D;

	LPC_USB0->USBSTS_D = int_status; // Acknowledge handled interrupt

	if (int_status == 0) return;

	if (int_status & INT_MASK_RESET)
	{
	  bus_reset(coreid);
	  usbd_bus_reset(coreid);
	}

	if (int_status & INT_MASK_USB)
	{
		if (LPC_USB0->ENDPTSETUPSTAT)
		{ // 23.10.10.2 Operational model for setup transfers
		  tusb_control_request_t control_request = dcd_data.qhd[0].setup_request;

		  LPC_USB0->ENDPTSETUPSTAT = LPC_USB0->ENDPTSETUPSTAT;
		  usbd_setup_received_isr(coreid, &control_request);
		}

		if (LPC_USB0->ENDPTCOMPLETE)
		{
//		  hal_debugger_breakpoint();
		  LPC_USB0->ENDPTCOMPLETE = LPC_USB0->ENDPTCOMPLETE;
		}
	}

	if (int_status & INT_MASK_SOF) { }
	if (int_status & INT_MASK_SUSPEND) { }
	if (int_status & INT_MASK_PORT_CHANGE) { }
	if (int_status & INT_MASK_NAK) { }
	if (int_status & INT_MASK_ERROR) ASSERT(false, VOID_RETURN);
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
#endif
