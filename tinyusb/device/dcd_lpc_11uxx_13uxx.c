/**************************************************************************/
/*!
    @file     dcd_lpc_11uxx_13uxx.c
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

#if MODE_DEVICE_SUPPORTED && (TUSB_CFG_MCU == MCU_LPC11UXX || TUSB_CFG_MCU == MCU_LPC13UXX)

#define _TINY_USB_SOURCE_FILE_

// NOTE: despite of being very the same to lpc13uxx controller, lpc11u's controller cannot queue transfer more than
// endpoint's max packet size and need some soft DMA helper

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hal/hal.h"
#include "osal/osal.h"
#include "common/timeout_timer.h"

#include "dcd.h"
#include "usbd_dcd.h"
#include "dcd_lpc_11uxx_13uxx.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_11U_13U_QHD_COUNT 10

enum {
  DCD_11U_13U_MAX_BYTE_PER_TD = (TUSB_CFG_MCU == MCU_LPC11UXX ? 64 : 1023)
};

#ifdef __CC_ARM
#pragma diag_suppress 66 // Suppress Keil warnings #66-D: enumeration value is out of "int" range
#endif

enum {
  INT_MASK_SOF           = BIT_(30),
  INT_MASK_DEVICE_STATUS = BIT_(31)
};

#ifdef __CC_ARM
#pragma diag_suppress 66 // Suppress Keil warnings #66-D: enumeration value is out of "int" range
#endif

enum {
  CMDSTAT_DEVICE_ADDR_MASK    = BIT_(7 )-1,
  CMDSTAT_DEVICE_ENABLE_MASK  = BIT_(7 ),
  CMDSTAT_SETUP_RECEIVED_MASK = BIT_(8 ),
  CMDSTAT_DEVICE_CONNECT_MASK = BIT_(16), ///< reflect the softconnect only, does not reflect the actual attached state
  CMDSTAT_DEVICE_SUSPEND_MASK = BIT_(17),
  CMDSTAT_CONNECT_CHANGE_MASK = BIT_(24),
  CMDSTAT_SUSPEND_CHANGE_MASK = BIT_(25),
  CMDSTAT_RESET_CHANGE_MASK   = BIT_(26),
  CMDSTAT_VBUS_DEBOUNCED_MASK = BIT_(28),
};

// buffer input must be 64 byte alignment
typedef struct ATTR_PACKED
{
  volatile uint16_t buff_addr_offset ; ///< The address offset is updated by hardware after each successful reception/transmission of a packet. Hardware increments the original value with the integer value when the packet size is divided by 64.

  volatile uint16_t nbytes      : 10 ; ///< For OUT endpoints this is the number of bytes that can be received in this buffer. For IN endpoints this is the number of bytes that must be transmitted. HW decrements this value with the packet size every time when a packet is successfully transferred. Note: If a short packet is received on an OUT endpoint, the active bit will be cleared and the NBytes value indicates the remaining buffer space that is not used. Software calculates the received number of bytes by subtracting the remaining NBytes from the programmed value.
  uint16_t is_isochronous       : 1  ;
  uint16_t feedback_toggle      : 1  ; ///< For bulk endpoints and isochronous endpoints this bit is reserved and must be set to zero. For the control endpoint zero this bit is used as the toggle value. When the toggle reset bit is set, the data toggle is updated with the value programmed in this bit. When the endpoint is used as an interrupt endpoint, it can be set to the following values. 0: Interrupt endpoint in ‘toggle mode’ 1: Interrupt endpoint in ‘rate feedback mode’. This means that the data toggle is fixed to zero for all data packets. When the interrupt endpoint is in ‘rate feedback mode’, the TR bit must always be set to zero.
  uint16_t toggle_reset         : 1  ; ///< When software sets this bit to one, the HW will set the toggle value equal to the value indicated in the “toggle value” (TV) bit. For the control endpoint zero, this is not needed to be used because the hardware resets the endpoint toggle to one for both directions when a setup token is received. For the other endpoints, the toggle can only be reset to zero when the endpoint is reset.
  uint16_t stall                : 1  ; ///< 0: The selected endpoint is not stalled 1: The selected endpoint is stalled The Active bit has always higher priority than the Stall bit. This means that a Stall handshake is only sent when the active bit is zero and the stall bit is one. Software can only modify this bit when the active bit is zero.
  uint16_t disable              : 1  ; ///< 0: The selected endpoint is enabled. 1: The selected endpoint is disabled. If a USB token is received for an endpoint that has the disabled bit set, hardware will ignore the token and not return any data or handshake. When a bus reset is received, software must set the disable bit of all endpoints to 1. Software can only modify this bit when the active bit is zero.
  volatile uint16_t active      : 1  ; ///< The buffer is enabled. HW can use the buffer to store received OUT data or to transmit data on the IN endpoint. Software can only set this bit to ‘1’. As long as this bit is set to one, software is not allowed to update any of the values in this 32-bit word. In case software wants to deactivate the buffer, it must write a one to the corresponding “skip” bit in the USB Endpoint skip register. Hardware can only write this bit to zero. It will do this when it receives a short packet or when the NBytes field transitions to zero or when software has written a one to the “skip” bit.
}dcd_11u_13u_qhd_t;

STATIC_ASSERT( sizeof(dcd_11u_13u_qhd_t) == 4, "size is not correct" );

// NOTE data will be transferred as soon as dcd get request by dcd_pipe(_queue)_xfer using double buffering.
// If there is another dcd_pipe_xfer request, the new request will be saved and executed when the first is done.
// next_td stored the 2nd request information
// current_td is used to keep track of number of remaining & xferred bytes of the current request.
// queued_bytes_in_buff keep track of number of bytes queued to each buffer (in case of short packet)

typedef struct {
  dcd_11u_13u_qhd_t qhd[DCD_11U_13U_QHD_COUNT][2]; ///< must be 256 byte alignment, 2 for double buffer

  // start at 80, the size should not exceed 48 (for setup_request align at 128)
  struct {
    uint16_t buff_addr_offset;
    uint16_t total_bytes;
  }next_td[DCD_11U_13U_QHD_COUNT];

  uint32_t current_ioc;          ///< interrupt on complete mask for current TD
  uint32_t next_ioc;             ///< interrupt on complete mask for next TD

  // must start from 128
  ATTR_ALIGNED(64) tusb_control_request_t setup_request;

  struct {
    uint16_t remaining_bytes;        ///< expected bytes of the queued transfer
    uint16_t xferred_total;          ///< xferred bytes of the current transfer

    uint16_t queued_bytes_in_buff[2]; ///< expected bytes that are queued for each buffer
  }current_td[DCD_11U_13U_QHD_COUNT];

  uint8_t  class_code[DCD_11U_13U_QHD_COUNT]; ///< class where the endpoints belongs to TODO no need for control endpoints

}dcd_11u_13u_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
// TUSB_CFG_ATTR_USBRAM must have ATTR_ALIGNED(64) for lpc11u & lpc13u
#ifdef __ICCARM__
ATTR_ALIGNED(256) TUSB_CFG_ATTR_USBRAM // for IAR the first ATTR_ALIGNED takes effect
#else
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(256) // GCC & Keil the last ATTR_ALIGNED takes effect
#endif
STATIC_VAR dcd_11u_13u_data_t dcd_data;

static inline uint16_t addr_offset(void const * p_buffer) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint16_t addr_offset(void const * p_buffer)
{
  ASSERT( (((uint32_t) p_buffer) & 0x3f) == 0, 0 );
  return (uint16_t) ( (((uint32_t) p_buffer) >> 6 ) & 0xFFFF) ;
}

static void queue_xfer_to_buffer(uint8_t ep_id, uint8_t buff_idx, uint16_t buff_addr_offset, uint16_t total_bytes);
static void pipe_queue_xfer(uint8_t ep_id, uint16_t buff_addr_offset, uint16_t total_bytes);
static void queue_xfer_in_next_td(uint8_t ep_id);

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void dcd_controller_connect(uint8_t coreid)
{
  (void) coreid;
  LPC_USB->DEVCMDSTAT |= CMDSTAT_DEVICE_CONNECT_MASK;
}

void dcd_controller_set_configuration(uint8_t coreid)
{

}

void dcd_controller_set_address(uint8_t coreid, uint8_t dev_addr)
{
  (void) coreid;

  LPC_USB->DEVCMDSTAT &= ~CMDSTAT_DEVICE_ADDR_MASK;
  LPC_USB->DEVCMDSTAT |= dev_addr;
}

tusb_error_t dcd_init(void)
{
  LPC_USB->EPLISTSTART  = (uint32_t) dcd_data.qhd;
  LPC_USB->DATABUFSTART = 0x20000000; // only SRAM1 & USB RAM can be used for transfer

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->INTEN        = INT_MASK_DEVICE_STATUS;
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_DEVICE_ENABLE_MASK | CMDSTAT_DEVICE_CONNECT_MASK |
                          CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;

  return TUSB_ERROR_NONE;
}

static void bus_reset(void)
{
  memclr_(&dcd_data, sizeof(dcd_11u_13u_data_t));
  for(uint8_t ep_id = 2; ep_id < DCD_11U_13U_QHD_COUNT; ep_id++)
  { // disable all non-control endpoints on bus reset
    dcd_data.qhd[ep_id][0].disable = dcd_data.qhd[ep_id][1].disable = 1;
  }

  dcd_data.qhd[0][1].buff_addr_offset = addr_offset(&dcd_data.setup_request);

  LPC_USB->EPINUSE      = 0;
  LPC_USB->EPBUFCFG     = 0; // all start with single buffer
  LPC_USB->EPSKIP       = 0xFFFFFFFF;

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_SETUP_RECEIVED_MASK; // clear setup received interrupt
  LPC_USB->INTEN        = INT_MASK_DEVICE_STATUS | BIT_(0) | BIT_(1); // enable device status & control endpoints
}

static void endpoint_non_control_isr(uint32_t int_status)
{
  for(uint8_t ep_id = 2; ep_id < DCD_11U_13U_QHD_COUNT; ep_id++ )
  {
    if ( BIT_TEST_(int_status, ep_id) )
    {
      dcd_11u_13u_qhd_t * const arr_qhd = dcd_data.qhd[ep_id];

      // when double buffering, the complete buffer is opposed to the current active buffer in EPINUSE
      uint8_t const buff_idx = LPC_USB->EPINUSE & BIT_(ep_id) ? 0 : 1;
      uint16_t const xferred_bytes = dcd_data.current_td[ep_id].queued_bytes_in_buff[buff_idx] - arr_qhd[buff_idx].nbytes;

      dcd_data.current_td[ep_id].xferred_total += xferred_bytes;

      // there are still data to transfer.
      if ( (arr_qhd[buff_idx].nbytes == 0) && (dcd_data.current_td[ep_id].remaining_bytes > 0) )
      { // NOTE although buff_addr_offset has been increased when xfer is completed
        // but we still need to increase it one more as we are using double buffering.
        queue_xfer_to_buffer(ep_id, buff_idx, arr_qhd[buff_idx].buff_addr_offset+1, dcd_data.current_td[ep_id].remaining_bytes);
      }
      // short packet or (no more byte and both buffers are finished)
      else if ( (arr_qhd[buff_idx].nbytes > 0) || !arr_qhd[1-buff_idx].active  )
      { // current TD (request) is completed
        LPC_USB->EPSKIP   = BIT_SET_(LPC_USB->EPSKIP, ep_id); // skip other endpoint in case of short-package

        dcd_data.current_td[ep_id].remaining_bytes = 0;

        if ( BIT_TEST_(dcd_data.current_ioc, ep_id) )
        {
          endpoint_handle_t edpt_hdl =
          {
              .coreid     = 0,
              .index      = ep_id,
              .class_code = dcd_data.class_code[ep_id]
          };

          dcd_data.current_ioc = BIT_CLR_(dcd_data.current_ioc, edpt_hdl.index);

          // TODO no way determine if the transfer is failed or not
          usbd_xfer_isr(edpt_hdl, TUSB_EVENT_XFER_COMPLETE, dcd_data.current_td[ep_id].xferred_total);
        }

        //------------- Next TD is available -------------//
        if ( dcd_data.next_td[ep_id].total_bytes != 0 )
        {
          queue_xfer_in_next_td(ep_id);
        }
      }else
      {
        // transfer complete, there is no more remaining bytes, but this buffer is not the last transaction (the other is)
      }
    }
  }
}

static void endpoint_control_isr(uint32_t int_status)
{
  uint8_t const ep_id = ( int_status & BIT_(0) ) ? 0 : 1;

  // there are still data to transfer.
  if ( (dcd_data.qhd[ep_id][0].nbytes == 0) && (dcd_data.current_td[ep_id].remaining_bytes > 0) )
  {
    queue_xfer_to_buffer(ep_id, 0, dcd_data.qhd[ep_id][0].buff_addr_offset, dcd_data.current_td[ep_id].remaining_bytes);
  }else
  {
    dcd_data.current_td[ep_id].remaining_bytes = 0;

    if ( BIT_TEST_(dcd_data.current_ioc, ep_id) )
    {
      endpoint_handle_t edpt_hdl = { .coreid = 0 };

      dcd_data.current_ioc = BIT_CLR_(dcd_data.current_ioc, ep_id);

      // FIXME xferred_byte for control xfer is not needed now !!!
      usbd_xfer_isr(edpt_hdl, TUSB_EVENT_XFER_COMPLETE, 0);
    }
  }
}

void dcd_isr(uint8_t coreid)
{
  (void) coreid;

  uint32_t const int_enable = LPC_USB->INTEN;
  uint32_t const int_status = LPC_USB->INTSTAT & int_enable;
  LPC_USB->INTSTAT = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  uint32_t const dev_cmd_stat = LPC_USB->DEVCMDSTAT;

  //------------- Device Status -------------//
  if ( int_status & INT_MASK_DEVICE_STATUS )
  {
    LPC_USB->DEVCMDSTAT |= CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;
    if ( dev_cmd_stat & CMDSTAT_RESET_CHANGE_MASK) // bus reset
    {
      bus_reset();
      usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_RESET);
    }

    if (dev_cmd_stat & CMDSTAT_CONNECT_CHANGE_MASK)
    { // device disconnect
      if (dev_cmd_stat & CMDSTAT_DEVICE_ADDR_MASK)
      { // debouncing as this can be set when device is powering
        usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_UNPLUGGED);
      }
    }

    // TODO support suspend & resume
    if (dev_cmd_stat & CMDSTAT_SUSPEND_CHANGE_MASK)
    {
      if (dev_cmd_stat & CMDSTAT_DEVICE_SUSPEND_MASK)
      { // suspend signal, bus idle for more than 3ms
        // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
        if (dev_cmd_stat & CMDSTAT_DEVICE_ADDR_MASK)
        {
          usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_SUSPENDED);
        }
      }
    }
//        else
//      { // resume signal
//        usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_RESUME);
//      }
//    }
  }

  //------------- Setup Received -------------//
  if ( BIT_TEST_(int_status, 0) && (dev_cmd_stat & CMDSTAT_SETUP_RECEIVED_MASK) )
  { // received control request from host
    // copy setup request & acknowledge so that the next setup can be received by hw
    usbd_setup_received_isr(coreid, &dcd_data.setup_request);

    // NXP control flowchart clear Active & Stall on both Control IN/OUT endpoints
    dcd_data.qhd[0][0].stall = dcd_data.qhd[1][0].stall = 0;

    LPC_USB->DEVCMDSTAT |= CMDSTAT_SETUP_RECEIVED_MASK;
    dcd_data.qhd[0][1].buff_addr_offset = addr_offset(&dcd_data.setup_request);
  }
  //------------- Control Endpoint -------------//
  else if ( int_status & 0x03 )
  {
    endpoint_control_isr(int_status);
  }

  //------------- Non-Control Endpoints -------------//
  if( int_status & ~(0x03UL) )
  {
    endpoint_non_control_isr(int_status);
  }
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
void dcd_pipe_control_stall(uint8_t coreid)
{
  (void) coreid;
  // TODO cannot able to STALL Control OUT endpoint !!!!! FIXME try some walk-around
  dcd_data.qhd[0][0].stall = dcd_data.qhd[1][0].stall = 1;
}

tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, uint8_t * p_buffer, uint16_t length, bool int_on_complete)
{
  (void) coreid;

  // determine Endpoint where Data & Status phase occurred (IN or OUT)
  uint8_t const ep_data   = (dir == TUSB_DIR_DEV_TO_HOST) ? 1 : 0;
  uint8_t const ep_status = 1 - ep_data;

  dcd_data.current_ioc = int_on_complete ? BIT_SET_(dcd_data.current_ioc, ep_status) : BIT_CLR_(dcd_data.current_ioc, ep_status);

  //------------- Data Phase -------------//
  if (length)
  {
    dcd_data.current_td[ep_data].remaining_bytes = length;
    dcd_data.current_td[ep_data].xferred_total   = 0;

    queue_xfer_to_buffer(ep_data, 0, addr_offset(p_buffer), length);
  }

  //------------- Status Phase -------------//
  dcd_data.current_td[ep_status].remaining_bytes = 0;
  dcd_data.current_td[ep_status].xferred_total   = 0;

  queue_xfer_to_buffer(ep_status, 0, 0, 0);

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 1 : 0);
}

#if 0
static inline uint8_t edpt_phy2log(uint8_t physical_endpoint) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_phy2log(uint8_t physical_endpoint)
{
  return physical_endpoint/2;
}
#endif

//--------------------------------------------------------------------+
// BULK/INTERRUPT/ISOCHRONOUS PIPE API
//--------------------------------------------------------------------+
tusb_error_t dcd_pipe_stall(endpoint_handle_t edpt_hdl)
{
  dcd_data.qhd[edpt_hdl.index][0].stall = dcd_data.qhd[edpt_hdl.index][1].stall = 1;

  return TUSB_ERROR_NONE;
}

bool dcd_pipe_is_stalled(endpoint_handle_t edpt_hdl)
{
  return dcd_data.qhd[edpt_hdl.index][0].stall || dcd_data.qhd[edpt_hdl.index][1].stall;
}

tusb_error_t dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr)
{
  uint8_t ep_id = edpt_addr2phy(edpt_addr);
//  uint8_t active_buffer = BIT_TEST_(LPC_USB->EPINUSE, ep_id) ? 1 : 0;

  dcd_data.qhd[ep_id][0].stall = dcd_data.qhd[ep_id][1].stall = 0;

  // since the next transfer always take place on buffer0 --> clear buffer0 toggle
  dcd_data.qhd[ep_id][0].toggle_reset    = 1;
  dcd_data.qhd[ep_id][0].feedback_toggle = 0;

  //------------- clear stall must carry on any previously queued transfer -------------//
  if ( dcd_data.next_td[ep_id].total_bytes != 0 )
  {
    queue_xfer_in_next_td(ep_id);
  }

  return TUSB_ERROR_NONE;
}

endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  (void) coreid;
  endpoint_handle_t const null_handle = { 0 };

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) return null_handle; // TODO not support ISO yet

  ASSERT (p_endpoint_desc->wMaxPacketSize.size <= 64, null_handle); // TODO ISO can be 1023, but ISO not supported now

  // TODO prevent to open if endpoint size is not 64

  //------------- Prepare Queue Head -------------//
  uint8_t ep_id = edpt_addr2phy(p_endpoint_desc->bEndpointAddress);

  ASSERT( dcd_data.qhd[ep_id][0].disable && dcd_data.qhd[ep_id][1].disable, null_handle ); // endpoint must not previously opened, normally this means running out of endpoints

  memclr_(dcd_data.qhd[ep_id], 2*sizeof(dcd_11u_13u_qhd_t));
  dcd_data.qhd[ep_id][0].is_isochronous = dcd_data.qhd[ep_id][1].is_isochronous = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  dcd_data.class_code[ep_id] = class_code;

  dcd_data.qhd[ep_id][0].disable = dcd_data.qhd[ep_id][1].disable = 0;

  LPC_USB->EPBUFCFG |= BIT_(ep_id);
  LPC_USB->INTEN    |= BIT_(ep_id);

  return (endpoint_handle_t)
      {
          .coreid     = 0,
          .index      = ep_id,
          .class_code = class_code
      };
}

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl)
{
  return dcd_data.qhd[edpt_hdl.index][0].active || dcd_data.qhd[edpt_hdl.index][1].active;
}

static void queue_xfer_to_buffer(uint8_t ep_id, uint8_t buff_idx, uint16_t buff_addr_offset, uint16_t total_bytes)
{
  uint16_t const queued_bytes = min16_of(total_bytes, DCD_11U_13U_MAX_BYTE_PER_TD);

  dcd_data.current_td[ep_id].queued_bytes_in_buff[buff_idx] = queued_bytes;
  dcd_data.current_td[ep_id].remaining_bytes               -= queued_bytes;

  dcd_data.qhd[ep_id][buff_idx].buff_addr_offset            = buff_addr_offset;
  dcd_data.qhd[ep_id][buff_idx].nbytes                      = queued_bytes;

  dcd_data.qhd[ep_id][buff_idx].active = 1;
}

static void pipe_queue_xfer(uint8_t ep_id, uint16_t buff_addr_offset, uint16_t total_bytes)
{
  dcd_data.current_td[ep_id].remaining_bytes         = total_bytes;
  dcd_data.current_td[ep_id].xferred_total           = 0;
  dcd_data.current_td[ep_id].queued_bytes_in_buff[0] = 0;
  dcd_data.current_td[ep_id].queued_bytes_in_buff[1] = 0;

  LPC_USB->EPINUSE  = BIT_CLR_(LPC_USB->EPINUSE , ep_id); // force HW to use buffer0

  // need to queue buffer1 first, as activate buffer0 can causes controller does transferring immediately
  // while buffer1 is not ready yet
  if ( total_bytes > DCD_11U_13U_MAX_BYTE_PER_TD)
  {
    queue_xfer_to_buffer(ep_id, 1, buff_addr_offset+1, total_bytes - DCD_11U_13U_MAX_BYTE_PER_TD);
  }

  queue_xfer_to_buffer(ep_id, 0, buff_addr_offset, total_bytes);
}

static void queue_xfer_in_next_td(uint8_t ep_id)
{
  dcd_data.current_ioc |= ( dcd_data.next_ioc & BIT_(ep_id) ); // copy next IOC to current IOC

  pipe_queue_xfer(ep_id, dcd_data.next_td[ep_id].buff_addr_offset, dcd_data.next_td[ep_id].total_bytes);

  dcd_data.next_td[ep_id].total_bytes = 0; // clear this field as it is used to indicate whehther next TD available
}

tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes)
{
  ASSERT( !dcd_pipe_is_busy(edpt_hdl), TUSB_ERROR_INTERFACE_IS_BUSY); // endpoint must not in transferring

  dcd_data.current_ioc = BIT_CLR_(dcd_data.current_ioc, edpt_hdl.index);

  pipe_queue_xfer(edpt_hdl.index, addr_offset(buffer), total_bytes);

  return TUSB_ERROR_NONE;
}

tusb_error_t  dcd_pipe_xfer(endpoint_handle_t edpt_hdl, uint8_t* buffer, uint16_t total_bytes, bool int_on_complete)
{
  if( dcd_pipe_is_busy(edpt_hdl) || dcd_pipe_is_stalled(edpt_hdl) )
  { // save this transfer data to next td if pipe is busy or already been stalled
    dcd_data.next_td[edpt_hdl.index].buff_addr_offset = addr_offset(buffer);
    dcd_data.next_td[edpt_hdl.index].total_bytes      = total_bytes;

    dcd_data.next_ioc = int_on_complete ? BIT_SET_(dcd_data.next_ioc, edpt_hdl.index) : BIT_CLR_(dcd_data.next_ioc, edpt_hdl.index);
  }else
  {
    dcd_data.current_ioc = int_on_complete ? BIT_SET_(dcd_data.current_ioc, edpt_hdl.index) : BIT_CLR_(dcd_data.current_ioc, edpt_hdl.index);

    pipe_queue_xfer(edpt_hdl.index, addr_offset(buffer), total_bytes);
  }

	return TUSB_ERROR_NONE;
}

#endif

