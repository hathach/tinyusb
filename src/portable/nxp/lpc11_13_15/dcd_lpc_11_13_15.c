/**************************************************************************/
/*!
    @file     dcd_lpc11_13_15.c
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

#if TUSB_OPT_DEVICE_ENABLED && (CFG_TUSB_MCU == OPT_MCU_LPC11UXX || CFG_TUSB_MCU == OPT_MCU_LPC13XX)

// NOTE: despite of being very the same to lpc13uxx controller, lpc11u's controller cannot queue transfer more than
// endpoint's max packet size and need some soft DMA helper

#include "chip.h"
#include "device/dcd.h"
#include "dcd_lpc11_13_15.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Number of endpoints
#define EP_COUNT 10

// only SRAM1 & USB RAM can be used for transfer
#define SRAM_REGION   0x20000000

enum {
  DCD_11U_13U_MAX_BYTE_PER_TD = (CFG_TUSB_MCU == OPT_MCU_LPC11UXX ? 64 : 1023)
};

enum {
  INT_SOF_MASK           = BIT_(30),
  INT_DEVICE_STATUS_MASK = BIT_(31)
};

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

typedef struct ATTR_PACKED
{
  // Bits 21:6 (aligned 64) used in conjunction with bit 31:22 of DATABUFSTART
  volatile uint16_t buffer_offset ;

  volatile uint16_t nbytes : 10 ;
  uint16_t is_iso          : 1  ;
  uint16_t toggle_mode     : 1  ;
  uint16_t toggle_reset    : 1  ;
  uint16_t stall           : 1  ;
  uint16_t disable         : 1  ;
  volatile uint16_t active : 1  ;
}ep_cmd_sts_t;

TU_VERIFY_STATIC( sizeof(ep_cmd_sts_t) == 4, "size is not correct" );

// NOTE data will be transferred as soon as dcd get request by dcd_pipe(_queue)_xfer using double buffering.
// If there is another dcd_edpt_xfer request, the new request will be saved and executed when the first is done.
// next_td stored the 2nd request information
// current_td is used to keep track of number of remaining & xferred bytes of the current request.
// queued_bytes_in_buff keep track of number of bytes queued to each buffer (in case of short packet)

typedef struct {
  ep_cmd_sts_t qhd[EP_COUNT][2]; ///< 256 byte aligned, 2 for double buffer

  uint16_t expected_len[EP_COUNT];

  // start at 80, the size should not exceed 48 (for setup_request align at 128)
  struct {
    uint16_t buff_addr_offset;
    uint16_t total_bytes;
  }next_td[EP_COUNT];

  uint32_t current_ioc;          ///< interrupt on complete mask for current TD
  uint32_t next_ioc;             ///< interrupt on complete mask for next TD

  // must start from 128
  ATTR_ALIGNED(64) uint8_t setup_packet[8];

  struct {
    uint16_t remaining_bytes;        ///< expected bytes of the queued transfer
    uint16_t xferred_total;          ///< xferred bytes of the current transfer

    uint16_t queued_bytes_in_buff[2]; ///< expected bytes that are queued for each buffer
  }current_td[EP_COUNT];

}dcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

// EP list must be 256-byte aligned
CFG_TUSB_MEM_SECTION ATTR_ALIGNED(256) static dcd_data_t _dcd;

static inline uint16_t addr_offset(void const * buffer)
{
  uint32_t addr = (uint32_t) buffer;
  TU_ASSERT( (addr & 0x3f) == 0, 0 );
  return ( (addr >> 6) & 0xFFFFUL ) ;
}

static void queue_xfer_to_buffer(uint8_t ep_id, uint8_t buff_idx, uint16_t buff_addr_offset, uint16_t total_bytes);
static void pipe_queue_xfer(uint8_t ep_id, uint16_t buff_addr_offset, uint16_t total_bytes);
static void queue_xfer_in_next_td(uint8_t ep_id);

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void tusb_hal_int_enable(uint8_t rhport)
{
  (void) rhport; // discard compiler's warning
  NVIC_EnableIRQ(USB0_IRQn);
}

void tusb_hal_int_disable(uint8_t rhport)
{
  (void) rhport; // discard compiler's warning
  NVIC_DisableIRQ(USB0_IRQn);
}

bool tusb_hal_init(void)
{
  // TODO remove
  return true;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  LPC_USB->DEVCMDSTAT |= CMDSTAT_DEVICE_CONNECT_MASK;
}

void dcd_set_config(uint8_t rhport, uint8_t config_num)
{

}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;

  LPC_USB->DEVCMDSTAT &= ~CMDSTAT_DEVICE_ADDR_MASK;
  LPC_USB->DEVCMDSTAT |= dev_addr;
}

bool dcd_init(uint8_t rhport)
{
  (void) rhport;

  // Setup PLL clock, and power
  Chip_USB_Init();

  LPC_USB->EPLISTSTART  = (uint32_t) _dcd.qhd;
  LPC_USB->DATABUFSTART = SRAM_REGION;

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->INTEN        = INT_DEVICE_STATUS_MASK;
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_DEVICE_ENABLE_MASK | CMDSTAT_DEVICE_CONNECT_MASK |
                          CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;

  NVIC_EnableIRQ(USB0_IRQn);

  return true;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_IN_MASK) ? 1 : 0);
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
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  if ( edpt_number(ep_addr) == 0 )
  {
    // TODO cannot able to STALL Control OUT endpoint !!!!! FIXME try some walk-around
    _dcd.qhd[0][0].stall = _dcd.qhd[1][0].stall = 1;
  }
  else
  {
    uint8_t const ep_id = edpt_addr2phy(edpt_addr);
    _dcd.qhd[ep_id][0].stall = _dcd.qhd[ep_id][1].stall = 1;
  }
}

bool dcd_edpt_stalled(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const ep_id = edpt_addr2phy(edpt_addr);
  return _dcd.qhd[ep_id][0].stall || _dcd.qhd[ep_id][1].stall;
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t edpt_addr)
{
  uint8_t const ep_id = edpt_addr2phy(edpt_addr);
//  uint8_t active_buffer = BIT_TEST_(LPC_USB->EPINUSE, ep_id) ? 1 : 0;

  _dcd.qhd[ep_id][0].stall = _dcd.qhd[ep_id][1].stall = 0;

  // since the next transfer always take place on buffer0 --> clear buffer0 toggle
  _dcd.qhd[ep_id][0].toggle_reset    = 1;
  _dcd.qhd[ep_id][0].toggle_mode = 0;

  //------------- clear stall must carry on any previously queued transfer -------------//
  if ( _dcd.next_td[ep_id].total_bytes != 0 )
  {
    queue_xfer_in_next_td(ep_id);
  }
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void) rhport;

  // TODO not support ISO yet
  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) return false;

  //------------- Prepare Queue Head -------------//
  uint8_t ep_id = edpt_addr2phy(p_endpoint_desc->bEndpointAddress);

  // endpoint must not previously opened, normally this means running out of endpoints
  TU_ASSERT( _dcd.qhd[ep_id][0].disable && _dcd.qhd[ep_id][1].disable );

  tu_memclr(_dcd.qhd[ep_id], 2*sizeof(ep_cmd_sts_t));
  _dcd.qhd[ep_id][0].is_iso = _dcd.qhd[ep_id][1].is_iso = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  _dcd.qhd[ep_id][0].disable = _dcd.qhd[ep_id][1].disable = 0;

  LPC_USB->EPBUFCFG |= BIT_(ep_id);
  LPC_USB->INTEN    |= BIT_(ep_id);

  return true;
}

bool dcd_edpt_busy(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const ep_id = edpt_addr2phy(ep_addr);
  return _dcd.qhd[ep_id][0].active || _dcd.qhd[ep_id][1].active;
}

static void queue_xfer_to_buffer(uint8_t ep_id, uint8_t buff_idx, uint16_t buff_addr_offset, uint16_t total_bytes)
{
  uint16_t const queued_bytes = tu_min16(total_bytes, DCD_11U_13U_MAX_BYTE_PER_TD);

  _dcd.current_td[ep_id].queued_bytes_in_buff[buff_idx] = queued_bytes;
  _dcd.current_td[ep_id].remaining_bytes               -= queued_bytes;

  _dcd.expected_len[ep_id] = total_bytes;

  _dcd.qhd[ep_id][buff_idx].buffer_offset = buff_addr_offset;
  _dcd.qhd[ep_id][buff_idx].nbytes        = queued_bytes;

  _dcd.qhd[ep_id][buff_idx].active        = 1;
}

static void pipe_queue_xfer(uint8_t ep_id, uint16_t buff_addr_offset, uint16_t total_bytes)
{
  _dcd.current_td[ep_id].remaining_bytes         = total_bytes;
  _dcd.current_td[ep_id].xferred_total           = 0;
  _dcd.current_td[ep_id].queued_bytes_in_buff[0] = 0;
  _dcd.current_td[ep_id].queued_bytes_in_buff[1] = 0;

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
  _dcd.current_ioc |= ( _dcd.next_ioc & BIT_(ep_id) ); // copy next IOC to current IOC

  pipe_queue_xfer(ep_id, _dcd.next_td[ep_id].buff_addr_offset, _dcd.next_td[ep_id].total_bytes);

  _dcd.next_td[ep_id].total_bytes = 0; // clear this field as it is used to indicate whehther next TD available
}

tusb_error_t dcd_edpt_queue_xfer(uint8_t ep_id , uint8_t * buffer, uint16_t total_bytes)
{
  _dcd.current_ioc = BIT_CLR_(_dcd.current_ioc, ep_id);
  pipe_queue_xfer(ep_id, addr_offset(buffer), total_bytes);
  return TUSB_ERROR_NONE;
}


bool dcd_control_xfer(uint8_t rhport, uint8_t ep_id, uint8_t * p_buffer, uint16_t length)
{
  (void) rhport;

  _dcd.current_ioc = BIT_SET_(_dcd.current_ioc, ep_id);

  _dcd.current_td[ep_id].remaining_bytes = length;
  _dcd.current_td[ep_id].xferred_total   = 0;

  queue_xfer_to_buffer(ep_id, 0, addr_offset(p_buffer), length);

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  uint8_t const ep_id = edpt_addr2phy(ep_addr);

  if ( edpt_number(ep_addr) == 0 )
  {
    return dcd_control_xfer(rhport, ep_id, buffer, (uint8_t) total_bytes);
  }

//  if( dcd_edpt_busy(ep_addr) || dcd_edpt_stalled(ep_addr) )
//  { // save this transfer data to next td if pipe is busy or already been stalled
//    dcd_data.next_td[ep_id].buff_addr_offset = addr_offset(buffer);
//    dcd_data.next_td[ep_id].total_bytes      = total_bytes;
//
//    dcd_data.next_ioc = BIT_SET_(dcd_data.next_ioc, ep_id);
//  }else
  {
    _dcd.current_ioc = BIT_SET_(_dcd.current_ioc, ep_id);

    pipe_queue_xfer(ep_id, addr_offset(buffer), total_bytes);
  }

	return true;
}

//--------------------------------------------------------------------+
// IRQ
//--------------------------------------------------------------------+
static void bus_reset(void)
{
  tu_memclr(&_dcd, sizeof(dcd_data_t));

  // disable all non-control endpoints on bus reset
  for(uint8_t ep_id = 2; ep_id < EP_COUNT; ep_id++)
  {
    _dcd.qhd[ep_id][0].disable = _dcd.qhd[ep_id][1].disable = 1;
  }

  _dcd.qhd[0][1].buffer_offset = addr_offset(_dcd.setup_packet);

  LPC_USB->EPINUSE      = 0;
  LPC_USB->EPBUFCFG     = 0; // all start with single buffer
  LPC_USB->EPSKIP       = 0xFFFFFFFF;

  LPC_USB->INTSTAT      = LPC_USB->INTSTAT; // clear all pending interrupt
  LPC_USB->DEVCMDSTAT  |= CMDSTAT_SETUP_RECEIVED_MASK; // clear setup received interrupt
  LPC_USB->INTEN        = INT_DEVICE_STATUS_MASK | BIT_(0) | BIT_(1); // enable device status & control endpoints
}

static void endpoint_non_control_isr(uint32_t int_status)
{
  for(uint8_t ep_id = 2; ep_id < EP_COUNT; ep_id++ )
  {
    if ( BIT_TEST_(int_status, ep_id) )
    {
      ep_cmd_sts_t * const arr_qhd = _dcd.qhd[ep_id];

      // when double buffering, the complete buffer is opposed to the current active buffer in EPINUSE
      uint8_t const buff_idx = LPC_USB->EPINUSE & BIT_(ep_id) ? 0 : 1;
      uint16_t const xferred_bytes = _dcd.current_td[ep_id].queued_bytes_in_buff[buff_idx] - arr_qhd[buff_idx].nbytes;

      _dcd.current_td[ep_id].xferred_total += xferred_bytes;

      // there are still data to transfer.
      if ( (arr_qhd[buff_idx].nbytes == 0) && (_dcd.current_td[ep_id].remaining_bytes > 0) )
      {
        // NOTE although buff_addr_offset has been increased when xfer is completed
        // but we still need to increase it one more as we are using double buffering.
        queue_xfer_to_buffer(ep_id, buff_idx, arr_qhd[buff_idx].buffer_offset+1, _dcd.current_td[ep_id].remaining_bytes);
      }
      else if ( (arr_qhd[buff_idx].nbytes > 0) || !arr_qhd[1-buff_idx].active  )
      {
        // short packet or (no more byte and both buffers are finished)
        // current TD (request) is completed
        LPC_USB->EPSKIP   = BIT_SET_(LPC_USB->EPSKIP, ep_id); // skip other endpoint in case of short-package

        _dcd.current_td[ep_id].remaining_bytes = 0;

        if ( BIT_TEST_(_dcd.current_ioc, ep_id) )
        {
          _dcd.current_ioc = BIT_CLR_(_dcd.current_ioc, ep_id);

          uint8_t const ep_addr = (ep_id / 2) | ((ep_id & 0x01) ? TUSB_DIR_IN_MASK : 0);

          // TODO no way determine if the transfer is failed or not
          dcd_event_xfer_complete(0, ep_addr, _dcd.current_td[ep_id].xferred_total, XFER_RESULT_SUCCESS, true);
        }

        //------------- Next TD is available -------------//
        if ( _dcd.next_td[ep_id].total_bytes != 0 )
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

  dcd_event_xfer_complete(0, ep_id ? TUSB_DIR_IN_MASK : 0, _dcd.expected_len[ep_id] - _dcd.qhd[ep_id][0].nbytes, XFER_RESULT_SUCCESS, true);
}

void USB_IRQHandler(void)
{
  uint32_t const int_enable = LPC_USB->INTEN;
  uint32_t const int_status = LPC_USB->INTSTAT & int_enable;
  LPC_USB->INTSTAT = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  uint32_t const dev_cmd_stat = LPC_USB->DEVCMDSTAT;

  //------------- Device Status -------------//
  if ( int_status & INT_DEVICE_STATUS_MASK )
  {
    LPC_USB->DEVCMDSTAT |= CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;
    if ( dev_cmd_stat & CMDSTAT_RESET_CHANGE_MASK) // bus reset
    {
      bus_reset();
      dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
    }

    if (dev_cmd_stat & CMDSTAT_CONNECT_CHANGE_MASK)
    {
      // device disconnect
      if (dev_cmd_stat & CMDSTAT_DEVICE_ADDR_MASK)
      {
        // debouncing as this can be set when device is powering
        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
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
          dcd_event_bus_signal(0, DCD_EVENT_SUSPENDED, true);
        }
      }
    }
//        else
//      { // resume signal
//    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
//      }
//    }
  }

  //------------- Setup Received -------------//
  if ( BIT_TEST_(int_status, 0) && (dev_cmd_stat & CMDSTAT_SETUP_RECEIVED_MASK) )
  {
    // received control request from host
    // copy setup request & acknowledge so that the next setup can be received by hw
    dcd_event_setup_received(0, _dcd.setup_packet, true);

    // NXP control flowchart clear Active & Stall on both Control IN/OUT endpoints
    _dcd.qhd[0][0].stall = _dcd.qhd[1][0].stall = 0;

    LPC_USB->DEVCMDSTAT |= CMDSTAT_SETUP_RECEIVED_MASK;
    _dcd.qhd[0][1].buffer_offset = addr_offset(_dcd.setup_packet);
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

#endif

