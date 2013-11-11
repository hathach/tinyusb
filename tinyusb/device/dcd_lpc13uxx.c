/**************************************************************************/
/*!
    @file     dcd_lpc13xx_12adc.c
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

#if MODE_DEVICE_SUPPORTED && MCU == MCU_LPC13UXX

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
#include "dcd_lpc13uxx.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_LPC13XX_QHD 10

enum {
  INT_MASK_SOF           = BIT_(30),
  INT_MASK_DEVICE_STATUS = BIT_(31)
};

enum {
  CMDSTAT_MASK_DEVICE_ENABLE  = BIT_(7 ),
  CMDSTAT_MASK_SETUP_RECEIVED = BIT_(8 ),
  CMDSTAT_MASK_CMD_CONNECT    = BIT_(16),
  CMDSTAT_MASK_CMD_SUSPEND    = BIT_(17),
  CMDSTAT_MASK_CONNECT_CHANGE = BIT_(24),
  CMDSTAT_MASK_SUSPEND_CHANGE = BIT_(25),
  CMDSTAT_MASK_RESET_CHANGE   = BIT_(26),
};

typedef struct {
  union {
    struct {
      uint32_t dev_addr           : 7;
      uint32_t dev_enable         : 1;
      uint32_t setup_received     : 1;
      uint32_t pll_on             : 1;
      uint32_t                    : 1;
      uint32_t lpm_support        : 1;
      uint32_t                    : 4; // not use interrupt on NAK
      uint32_t dev_connect        : 1;
      uint32_t dev_suspend        : 1;
      uint32_t                    : 1;
      uint32_t lpm_suspend        : 1;
      uint32_t lpm_remote_wakeup  : 1;
      uint32_t                    : 3;
      uint32_t dev_connect_change : 1;
      uint32_t dev_suspend_change : 1;
      uint32_t dev_reset_change   : 1;
      uint32_t                    : 1;
      uint32_t vbus_debounced     : 1;
      uint32_t                    : 3;
    }bits;
    uint32_t value;
  };
} lpc13xx_dev_cmd_stat;

STATIC_ASSERT( sizeof(lpc13xx_dev_cmd_stat) == 4, "size is not correct" );


// buffer input must be 64 byte alignment
typedef struct {
  volatile uint16_t buff_addr_offset ; ///< The address offset is updated by hardware after each successful reception/transmission of a packet. Hardware increments the original value with the integer value when the packet size is divided by 64.

  volatile uint16_t total_bytes : 10 ; ///< For OUT endpoints this is the number of bytes that can be received in this buffer. For IN endpoints this is the number of bytes that must be transmitted. HW decrements this value with the packet size every time when a packet is successfully transferred. Note: If a short packet is received on an OUT endpoint, the active bit will be cleared and the NBytes value indicates the remaining buffer space that is not used. Software calculates the received number of bytes by subtracting the remaining NBytes from the programmed value.
  uint16_t is_isochronous       : 1  ;
  uint16_t feedback_toggle      : 1  ; ///< For bulk endpoints and isochronous endpoints this bit is reserved and must be set to zero. For the control endpoint zero this bit is used as the toggle value. When the toggle reset bit is set, the data toggle is updated with the value programmed in this bit. When the endpoint is used as an interrupt endpoint, it can be set to the following values. 0: Interrupt endpoint in ‘toggle mode’ 1: Interrupt endpoint in ‘rate feedback mode’. This means that the data toggle is fixed to zero for all data packets. When the interrupt endpoint is in ‘rate feedback mode’, the TR bit must always be set to zero.
  uint16_t toggle_reset         : 1  ; ///< When software sets this bit to one, the HW will set the toggle value equal to the value indicated in the “toggle value” (TV) bit. For the control endpoint zero, this is not needed to be used because the hardware resets the endpoint toggle to one for both directions when a setup token is received. For the other endpoints, the toggle can only be reset to zero when the endpoint is reset.
  uint16_t stall                : 1  ; ///< 0: The selected endpoint is not stalled 1: The selected endpoint is stalled The Active bit has always higher priority than the Stall bit. This means that a Stall handshake is only sent when the active bit is zero and the stall bit is one. Software can only modify this bit when the active bit is zero.
  uint16_t disable              : 1  ; ///< 0: The selected endpoint is enabled. 1: The selected endpoint is disabled. If a USB token is received for an endpoint that has the disabled bit set, hardware will ignore the token and not return any data or handshake. When a bus reset is received, software must set the disable bit of all endpoints to 1. Software can only modify this bit when the active bit is zero.
  volatile uint16_t active      : 1  ; ///< The buffer is enabled. HW can use the buffer to store received OUT data or to transmit data on the IN endpoint. Software can only set this bit to ‘1’. As long as this bit is set to one, software is not allowed to update any of the values in this 32-bit word. In case software wants to deactivate the buffer, it must write a one to the corresponding “skip” bit in the USB Endpoint skip register. Hardware can only write this bit to zero. It will do this when it receives a short packet or when the NBytes field transitions to zero or when software has written a one to the “skip” bit.
}dcd_lpc13xx_qhd_t;

STATIC_ASSERT( sizeof(dcd_lpc13xx_qhd_t) == 4, "size is not correct" );

typedef struct {
  dcd_lpc13xx_qhd_t qhd[DCD_LPC13XX_QHD][2]; ///< must be 256 byte alignment, 2 for double buffer

  // there is padding from 80 --> 128 = 48 bytes

  // should start from 128
  ATTR_ALIGNED(64) tusb_control_request_t setup_request;

}dcd_lpc13xx_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
ATTR_ALIGNED(256) static dcd_lpc13xx_data_t dcd_data TUSB_CFG_ATTR_USBRAM;

static inline uint16_t addr_offset(void const * p_buffer) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint16_t addr_offset(void const * p_buffer)
{
  ASSERT( (((uint32_t) p_buffer) & 0x3f) == 0, 0 );
  return (uint16_t) ( (((uint32_t) p_buffer) >> 6 ) & 0xFFFF) ;
}

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void dcd_controller_connect(uint8_t coreid)
{
  (void) coreid;
  LPC_USB->DEVCMDSTAT |= CMDSTAT_MASK_CMD_CONNECT;
}

void dcd_controller_set_configuration(uint8_t coreid, uint8_t config_num)
{

}

void dcd_controller_set_address(uint8_t coreid, uint8_t dev_addr)
{
  (void) coreid;

  LPC_USB->DEVCMDSTAT &= ~0x7F;
  LPC_USB->DEVCMDSTAT |= dev_addr;
}


tusb_error_t dcd_init(void)
{
  LPC_USB->EPLISTSTART  = (uint32_t) dcd_data.qhd;
  LPC_USB->DATABUFSTART = 0x20000000; // only SRAM1 & USB RAM can be used for transfer
  LPC_USB->EPBUFCFG     = 0; // TODO double buffering

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->INTEN        = INT_MASK_DEVICE_STATUS;
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_MASK_DEVICE_ENABLE;

  return TUSB_ERROR_NONE;
}

static void bus_reset(void)
{
  memclr_(&dcd_data, sizeof(dcd_lpc13xx_data_t));

  dcd_data.qhd[0][1].buff_addr_offset = addr_offset(&dcd_data.setup_request);

  LPC_USB->EPINUSE      = 0;
  LPC_USB->EPSKIP       = 0xFFFFFFFF;

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_MASK_SETUP_RECEIVED; // clear setup received interrupt
  LPC_USB->INTEN        = INT_MASK_DEVICE_STATUS | BIT_(0) | BIT_(1); // enable device status & control endpoints
}

void dcd_isr(uint8_t coreid)
{
  (void) coreid;

  uint32_t int_status = LPC_USB->INTSTAT;
  int_status &= LPC_USB->INTEN;

  LPC_USB->INTSTAT = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  uint32_t dev_cmd_stat = LPC_USB->DEVCMDSTAT;

  //------------- Device Status -------------//
  if ( int_status & INT_MASK_DEVICE_STATUS )
  {
    if ( dev_cmd_stat & CMDSTAT_MASK_RESET_CHANGE)
    { // bus reset
      bus_reset();
      usbd_bus_reset(0);
    }

    // TODO not support suspend yet
    if ( dev_cmd_stat & CMDSTAT_MASK_SUSPEND_CHANGE) { }

    if ( dev_cmd_stat & CMDSTAT_MASK_CONNECT_CHANGE)
    { // device disconnect ?

    }

    LPC_USB->DEVCMDSTAT |= CMDSTAT_MASK_RESET_CHANGE | CMDSTAT_MASK_SUSPEND_CHANGE | CMDSTAT_MASK_CONNECT_CHANGE;
  }

  //------------- Endpoint Interrupt -------------//
  if ( int_status & BIT_(0) )
  {
    if ( dev_cmd_stat & CMDSTAT_MASK_SETUP_RECEIVED )
    { // received control request from host
      // copy setup request & acknowledge so that the next setup can be received by hw
      tusb_control_request_t control_request =  dcd_data.setup_request;
      LPC_USB->DEVCMDSTAT |= CMDSTAT_MASK_SETUP_RECEIVED;
      dcd_data.qhd[0][1].buff_addr_offset = addr_offset(&dcd_data.setup_request);

      usbd_setup_received_isr(coreid, &control_request);
    }
  }

}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
void dcd_pipe_control_stall(uint8_t coreid)
{ // only need to stall IN Control endpoint
  (void) coreid;
  dcd_data.qhd[1][0].stall = 1;
}

// control transfer does not need to use qtd find function
tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, void * buffer, uint16_t length)
{
  (void) coreid;

  // determine Endpoint where Data & Status phase occurred (IN or OUT)
  uint8_t const endpoint_data   = (dir == TUSB_DIR_DEV_TO_HOST) ? 1 : 0;
  uint8_t const endpoint_status = 1 - endpoint_data;

  //------------- Data Phase -------------//
  if (length)
  {
    dcd_data.qhd[endpoint_data][0].buff_addr_offset = addr_offset(buffer);
    dcd_data.qhd[endpoint_data][0].total_bytes      = length;
    dcd_data.qhd[endpoint_data][0].active           = 1 ;
  }

  //------------- Status Phase -------------//
  dcd_data.qhd[endpoint_status][0].buff_addr_offset = 0;
  dcd_data.qhd[endpoint_status][0].total_bytes      = 0;
  dcd_data.qhd[endpoint_status][0].active           = 1 ;


  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
//static inline uint8_t edpt_pos2phy(uint8_t pos) ATTR_CONST ATTR_ALWAYS_INLINE;
//static inline uint8_t edpt_pos2phy(uint8_t pos)
//{ // 0-5 --> OUT, 16-21 IN
//  return (pos < DCD_QHD_MAX/2) ? (2*pos) : (2*(pos-16)+1);
//}

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

//--------------------------------------------------------------------+
// BULK/INTERRUPT/ISOCHRONOUS PIPE API
//--------------------------------------------------------------------+
tusb_error_t dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr)
{
  return TUSB_ERROR_NONE;
}

endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  (void) coreid;

  endpoint_handle_t const null_handle = { .coreid = 0, .xfer_type = 0, .index = 0 };

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return null_handle; // TODO not support ISO yet

  //------------- Prepare Queue Head -------------//
  uint8_t ep_idx = edpt_addr2phy(p_endpoint_desc->bEndpointAddress);

  memclr_(dcd_data.qhd[ep_idx], 2*sizeof(dcd_lpc13xx_qhd_t));
  dcd_data.qhd[ep_idx][0].is_isochronous = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);

  LPC_USB->INTEN |= BIT_(ep_idx);

  return (endpoint_handle_t)
      {
          .coreid     = 0,
          .xfer_type  = p_endpoint_desc->bmAttributes.xfer,
          .index      = ep_idx,
          .class_code = class_code
      };
}

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl)
{
  return dcd_data.qhd[edpt_hdl.index][0].active;
}

// add only, controller virtually cannot know
static tusb_error_t pipe_add_xfer(endpoint_handle_t edpt_hdl, void * buffer, uint16_t total_bytes, bool int_on_complete)
{


  return TUSB_ERROR_NONE;
}

tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, void * buffer, uint16_t total_bytes)
{

}

tusb_error_t  dcd_pipe_xfer(endpoint_handle_t edpt_hdl, void* buffer, uint16_t total_bytes, bool int_on_complete)
{


	return TUSB_ERROR_NONE;
}
#endif

