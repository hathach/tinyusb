/**************************************************************************/
/*!
    @file     dcd_lpc175x_6x.c
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

#if MODE_DEVICE_SUPPORTED && (TUSB_CFG_MCU == MCU_LPC175X_6X)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "dcd.h"
#include "dcd_lpc175x_6x.h"
#include "usbd_dcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_QHD_MAX 32
#define DCD_QTD_MAX  32 // TODO scale with configure

typedef struct {
  volatile ATTR_ALIGNED(128) dcd_dma_descriptor_t* udca[DCD_QHD_MAX];
  dcd_dma_descriptor_t dd[DCD_QTD_MAX];

  uint8_t ddat[DCD_QHD_MAX]; ///< DMA Descriptor Allocation Table. A fixed DD will be allocated for a UDCA pointer up on endpoint open
  uint8_t class_code[DCD_QHD_MAX];

  volatile uint32_t ioc_dd; ///< each bit for each DD

}dcd_data_t;

STATIC_ dcd_data_t dcd_data TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static void bus_reset(void);
static tusb_error_t pipe_control_read(void * buffer, uint16_t length);

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t edpt_addr2phy(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_DEV_TO_HOST_MASK) ? 1 : 0);
}

static inline void edpt_set_max_packet_size(uint8_t ep_id, uint16_t max_packet_size) ATTR_ALWAYS_INLINE;
static inline void edpt_set_max_packet_size(uint8_t ep_id, uint16_t max_packet_size)
{ // follows example in 11.10.4.2
  LPC_USB->USBReEp    |= BIT_(ep_id);
  LPC_USB->USBEpInd    = ep_id; // select index before setting packet size
  LPC_USB->USBMaxPSize = max_packet_size;

#ifndef _TEST_
  while ((LPC_USB->USBDevIntSt & DEV_INT_ENDPOINT_REALIZED_MASK) == 0) {} // TODO can be omitted
  LPC_USB->USBDevIntClr = DEV_INT_ENDPOINT_REALIZED_MASK;
#endif

}

// retval UINT8_MAX: invalid
static inline uint8_t dd_find_free(void) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t dd_find_free(void)
{
  for(uint8_t i=0; i<DCD_QTD_MAX; i++)
  {
    if (dcd_data.dd[i].used == 0) return i;
  }

  return UINT8_MAX;
}

static inline uint8_t dd_get_index(dcd_dma_descriptor_t const * const p_dd) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t dd_get_index(dcd_dma_descriptor_t const * const p_dd)
{
  return (p_dd - dcd_data.dd);
}

static inline dcd_dma_descriptor_t* qhd_get_fixed_dd(uint8_t ep_id) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline dcd_dma_descriptor_t* qhd_get_fixed_dd(uint8_t ep_id)
{
  return &dcd_data.dd[ dcd_data.ddat[ep_id] ];
}

//--------------------------------------------------------------------+
// USBD-DCD API
//--------------------------------------------------------------------+
static void bus_reset(void)
{
  // step 7 : slave mode set up
  LPC_USB->USBEpIntClr     = 0xFFFFFFFF;          // clear all pending interrupt
	LPC_USB->USBDevIntClr    = 0xFFFFFFFF;          // clear all pending interrupt
	LPC_USB->USBEpIntEn      = (uint32_t) BIN8(11); // control endpoint cannot use DMA, non-control all use DMA
	LPC_USB->USBEpIntPri     = 0;                   // same priority for all endpoint

	// step 8 : DMA set up
	LPC_USB->USBEpDMADis     = 0xFFFFFFFF; // firstly disable all dma
	LPC_USB->USBDMARClr      = 0xFFFFFFFF; // clear all pending interrupt
	LPC_USB->USBEoTIntClr    = 0xFFFFFFFF;
	LPC_USB->USBNDDRIntClr   = 0xFFFFFFFF;
	LPC_USB->USBSysErrIntClr = 0xFFFFFFFF;

	memclr_(&dcd_data, sizeof(dcd_data_t));
}

tusb_error_t dcd_init(void)
{
  //------------- user manual 11.13 usb device controller initialization -------------//  LPC_USB->USBEpInd = 0;
  // step 6 : set up control endpoint
  edpt_set_max_packet_size(0, TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE);
  edpt_set_max_packet_size(1, TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE);

  bus_reset();

  LPC_USB->USBDevIntEn = (DEV_INT_DEVICE_STATUS_MASK | DEV_INT_ENDPOINT_SLOW_MASK | DEV_INT_ERROR_MASK);
	LPC_USB->USBUDCAH    = (uint32_t) dcd_data.udca;
	LPC_USB->USBDMAIntEn = (DMA_INT_END_OF_XFER_MASK | DMA_INT_ERROR_MASK );

	// clear all stall on control endpoint IN & OUT if any
//	sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS  , 1, 0);
//	sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+1, 1, 0);

	sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, 1); // connect

  return TUSB_ERROR_NONE;
}

void endpoint_control_isr(void)
{
  uint32_t const endpoint_int_status = LPC_USB->USBEpIntSt & LPC_USB->USBEpIntEn;

  //------------- control OUT -------------//
  if (endpoint_int_status & BIT_(0))
  {
    uint32_t const endpoint_status = sie_read(SIE_CMDCODE_ENDPOINT_SELECT+0, 1);
    if (endpoint_status & SIE_SELECT_ENDPOINT_SETUP_RECEIVED_MASK)
    {
      tusb_control_request_t control_request;

      (void) sie_read(SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT+0, 1); // clear setup bit, can be omitted ???

      pipe_control_read(&control_request, 8);

      usbd_setup_received_isr(0, &control_request);
    }else
    {
      // Current not support any out control with data yet
    }
  }

  //------------- control IN -------------//
  if (endpoint_int_status & BIT_(1))
  {
    endpoint_handle_t edpt_hdl =
    {
        .index      = 1 //BIT_TEST_(int_status, 1) ? 1 : 0
    };

    // FIXME xferred_byte for control xfer is not needed now !!!
    usbd_xfer_isr(edpt_hdl, TUSB_EVENT_XFER_COMPLETE, 0);
  }

  LPC_USB->USBEpIntClr = endpoint_int_status; // acknowledge interrupt TODO cannot immediately acknowledge setup packet
}

void dcd_isr(uint8_t coreid)
{
  (void) coreid;
  uint32_t const device_int_status = LPC_USB->USBDevIntSt & LPC_USB->USBDevIntEn;
  LPC_USB->USBDevIntClr = device_int_status;// Acknowledge handled interrupt

  //------------- usb bus event -------------//
  if (device_int_status & DEV_INT_DEVICE_STATUS_MASK)
  {
    uint8_t const dev_status_reg = sie_read(SIE_CMDCODE_DEVICE_STATUS, 1);
    if (dev_status_reg & SIE_DEV_STATUS_RESET_MASK)
    {
      bus_reset();
      usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_RESET);
    }

    if (dev_status_reg & SIE_DEV_STATUS_CONNECT_CHANGE_MASK)
    { // device is disconnected, require using VBUS (P1_30)
      usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_UNPLUGGED);
    }

    if (dev_status_reg & SIE_DEV_STATUS_SUSPEND_CHANGE_MASK)
    {
      if (dev_status_reg & SIE_DEV_STATUS_SUSPEND_MASK)
      {
        usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_SUSPENDED);
      }
//      else
//      {
//        usbd_dcd_bus_event_isr(0, USBD_BUS_EVENT_RESUME);
//      }
    }
  }

  //------------- Control Endpoint (Slave Mode) -------------//
  if (device_int_status & DEV_INT_ENDPOINT_SLOW_MASK)
  {
    endpoint_control_isr();
  }

  //------------- Non-Control Endpoint (DMA Mode) -------------//
  uint32_t dma_int_status = LPC_USB->USBDMAIntSt & LPC_USB->USBDMAIntEn;

  if (dma_int_status & DMA_INT_END_OF_XFER_MASK)
  {
    uint32_t eot_int = LPC_USB->USBEoTIntSt;
    LPC_USB->USBEoTIntClr = eot_int; // acknowledge interrupt source

    for(uint8_t ep_id = 2; ep_id < DCD_QHD_MAX; ep_id++ )
    {
      if ( BIT_TEST_(eot_int, ep_id) )
      {
        dcd_dma_descriptor_t* const p_fixed_dd = qhd_get_fixed_dd(ep_id);
        // Maximum is 2 QTD are queued in an endpoint
        dcd_dma_descriptor_t* const p_last_dd = (p_fixed_dd->is_next_valid) ? ((dcd_dma_descriptor_t*) p_fixed_dd->next) : p_fixed_dd;


        // only handle when Controller already finished the last DD
        if ( dcd_data.udca[ep_id] == p_last_dd )
        {
          dcd_data.udca[ep_id] = p_fixed_dd; // UDCA currently points to the last DD, change to the fixed DD
          p_fixed_dd->buffer_length = 0; // buffer length is used to determined if fixed dd is queued in pipe xfer function

          if (p_fixed_dd->is_next_valid)
          { // last_dd is not fixed_dd --> need to free
            p_last_dd->used = 0;
          }

          if ( BIT_TEST_(dcd_data.ioc_dd, dd_get_index(p_last_dd) ) )
          {
            dcd_data.ioc_dd = BIT_CLR_(dcd_data.ioc_dd, dd_get_index(p_last_dd) );

            endpoint_handle_t edpt_hdl =
            {
                .coreid     = 0,
                .index      = ep_id,
                .class_code = dcd_data.class_code[ep_id]
            };
            tusb_event_t event = (p_last_dd->status == DD_STATUS_NORMAL || p_last_dd->status == DD_STATUS_DATA_UNDERUN) ? TUSB_EVENT_XFER_COMPLETE : TUSB_EVENT_XFER_ERROR;

            usbd_xfer_isr(edpt_hdl, event, p_last_dd->present_count); // only number of bytes in the IOC qtd
          }
        }
      }
    }
  }

  if (device_int_status & DEV_INT_ERROR_MASK || dma_int_status & DMA_INT_ERROR_MASK)
  {
    uint32_t error_status = sie_read(SIE_CMDCODE_READ_ERROR_STATUS, 1);
    (void) error_status;
//    ASSERT(false, (void) 0);
  }
}

//--------------------------------------------------------------------+
// USBD API - CONTROLLER
//--------------------------------------------------------------------+
void dcd_controller_connect(uint8_t coreid)
{
  (void) coreid;
  sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, 1);
}

void dcd_controller_set_address(uint8_t coreid, uint8_t dev_addr)
{
  (void) coreid;
  sie_write(SIE_CMDCODE_SET_ADDRESS, 1, 0x80 | dev_addr); // 7th bit is : device_enable
}

void dcd_controller_set_configuration(uint8_t coreid)
{
  (void) coreid;
  sie_write(SIE_CMDCODE_CONFIGURE_DEVICE, 1, 1);
}

//--------------------------------------------------------------------+
// PIPE CONTROL HELPER
//--------------------------------------------------------------------+
static inline uint16_t length_byte2dword(uint16_t length_in_bytes) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint16_t length_byte2dword(uint16_t length_in_bytes)
{
  return (length_in_bytes + 3) / 4; // length_in_dword
}

static tusb_error_t pipe_control_write(void const * buffer, uint16_t length)
{
  ASSERT( length !=0 || buffer == NULL, TUSB_ERROR_INVALID_PARA);

  LPC_USB->USBCtrl   = USBCTRL_WRITE_ENABLE_MASK; // logical endpoint = 0
	LPC_USB->USBTxPLen = length;

	for (uint16_t count = 0; count < length_byte2dword(length); count++)
	{
		LPC_USB->USBTxData = *((uint32_t *)buffer); // NOTE: cortex M3 have no problem with alignment
		buffer += 4;
	}

    LPC_USB->USBCtrl   = 0;

	// select control IN & validate the endpoint
	sie_write(SIE_CMDCODE_ENDPOINT_SELECT+1, 0, 0);
	sie_write(SIE_CMDCODE_BUFFER_VALIDATE  , 0, 0);

  return TUSB_ERROR_NONE;
}

static tusb_error_t pipe_control_read(void * buffer, uint16_t length)
{
  LPC_USB->USBCtrl = USBCTRL_READ_ENABLE_MASK; // logical endpoint = 0
  while ((LPC_USB->USBRxPLen & USBRXPLEN_PACKET_READY_MASK) == 0) {} // TODO blocking, should have timeout

  uint16_t actual_length = min16_of(length, (uint16_t) (LPC_USB->USBRxPLen & USBRXPLEN_PACKET_LENGTH_MASK) );
  uint32_t *p_read_data = (uint32_t*) buffer;
  for( uint16_t count=0; count < length_byte2dword(actual_length); count++)
  {
    *p_read_data = LPC_USB->USBRxData;
    p_read_data++; // increase by 4 ( sizeof(uint32_t) )
  }
  
  LPC_USB->USBCtrl = 0;

  // select control OUT & clear the endpoint
  sie_write(SIE_CMDCODE_ENDPOINT_SELECT+0, 0, 0);
  sie_write(SIE_CMDCODE_BUFFER_CLEAR     , 0, 0);

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
void dcd_pipe_control_stall(uint8_t coreid)
{
  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+0, 1, SIE_SET_ENDPOINT_STALLED_MASK | SIE_SET_ENDPOINT_CONDITION_STALLED_MASK);
}

tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, void * p_buffer, uint16_t length)
{
  (void) coreid;

  if ( dir )
  {
    ASSERT_STATUS ( pipe_control_write(p_buffer, length) );
  }else
  {
    ASSERT_STATUS ( pipe_control_read(p_buffer, length) );
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INTERRUPT/ISO PIPE API
//--------------------------------------------------------------------+
endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  (void) coreid;

  endpoint_handle_t const null_handle = { 0 };

  // TODO refractor to universal pipe open validation function
  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) return null_handle; // TODO not support ISO yet
  ASSERT (p_endpoint_desc->wMaxPacketSize.size <= 64, null_handle); // TODO ISO can be 1023, but ISO not supported now

  uint8_t ep_id = edpt_addr2phy( p_endpoint_desc->bEndpointAddress );

  //------------- Realize Endpoint with Max Packet Size -------------//
  edpt_set_max_packet_size(ep_id, p_endpoint_desc->wMaxPacketSize.size);

	//------------- fixed DD prepare -------------//
	uint8_t const dd_idx = dd_find_free();
	ASSERT(dd_idx != UINT8_MAX, null_handle);

	dcd_data.ddat[ep_id]       = dd_idx; // fixed this DD to UDCA for this endpoint
	dcd_data.class_code[ep_id] = class_code;

	dcd_dma_descriptor_t* const p_dd = &dcd_data.dd[dd_idx];
	memclr_(p_dd, sizeof(dcd_dma_descriptor_t));

	p_dd->used            = 1;
	p_dd->is_isochronous  = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) ? 1 : 0;
	p_dd->max_packet_size = p_endpoint_desc->wMaxPacketSize.size;
	p_dd->is_retired      = 1; // inactive at first

	dcd_data.udca[ ep_id ] = p_dd; // hook to UDCA

	sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, 0); // clear all endpoint status

  return (endpoint_handle_t)
      {
          .coreid     = 0,
          .index      = ep_id,
          .class_code = class_code
      };
}

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl)
{
  return (dcd_data.udca[edpt_hdl.index] != NULL && !dcd_data.udca[edpt_hdl.index]->is_retired);
}

tusb_error_t dcd_pipe_stall(endpoint_handle_t edpt_hdl)
{
  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+edpt_hdl.index, 1, SIE_SET_ENDPOINT_STALLED_MASK);
  return TUSB_ERROR_NONE;
}

tusb_error_t dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr)
{
  uint8_t ep_id = edpt_addr2phy(edpt_addr);

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, 0);

  return TUSB_ERROR_FAILED;
}

void dd_xfer_init(dcd_dma_descriptor_t* p_dd, void* buffer, uint16_t total_bytes)
{
  p_dd->next                  = 0;
  p_dd->is_next_valid         = 0;
  p_dd->buffer_addr           = (uint32_t) buffer;
  p_dd->buffer_length         = total_bytes;
  p_dd->status                = DD_STATUS_NOT_SERVICED;
  p_dd->iso_last_packet_valid = 0;
  p_dd->present_count         = 0;
}

tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, void * buffer, uint16_t total_bytes)
{ // NOTE for sure the qhd has no dds
  dcd_dma_descriptor_t* const p_fixed_dd = qhd_get_fixed_dd(edpt_hdl.index); // always queue with the fixed DD

  dd_xfer_init(p_fixed_dd, buffer, total_bytes);
  p_fixed_dd->is_retired  = 1;

  dcd_data.ioc_dd = BIT_CLR_(dcd_data.ioc_dd, dcd_data.ddat[edpt_hdl.index] ); // fixed index is stored in ddat

  return TUSB_ERROR_NONE;
}

tusb_error_t dcd_pipe_xfer(endpoint_handle_t edpt_hdl, void * buffer, uint16_t total_bytes, bool int_on_complete)
{
  dcd_dma_descriptor_t* const p_fixed_dd = qhd_get_fixed_dd(edpt_hdl.index);

  if ( p_fixed_dd->buffer_length )
  { // fixed DD is already queued a xfer
    //------------- setup new dd -------------//
    uint8_t dd_idx = dd_find_free();
    ASSERT( dd_idx != UINT8_MAX, TUSB_ERROR_DCD_NOT_ENOUGH_QTD);

    dcd_dma_descriptor_t* const p_dd = &dcd_data.dd[ dd_idx ];
    memclr_(p_dd, sizeof(dcd_dma_descriptor_t));

    dd_xfer_init(p_dd, buffer, total_bytes);

    p_dd->used            = 1;
    p_dd->max_packet_size = p_fixed_dd->max_packet_size;
    p_dd->is_isochronous  = p_fixed_dd->is_isochronous;
    dcd_data.ioc_dd       = int_on_complete ? BIT_SET_(dcd_data.ioc_dd, dd_idx) : BIT_CLR_(dcd_data.ioc_dd, dd_idx);

    //------------- hook to fixed dd -------------//
    p_fixed_dd->next          = (uint32_t) p_dd;
    p_fixed_dd->is_next_valid = 1;
  } else
  { // fixed DD is free
    dd_xfer_init(p_fixed_dd, buffer, total_bytes);
    dcd_data.ioc_dd     = int_on_complete ? BIT_SET_(dcd_data.ioc_dd, dcd_data.ddat[edpt_hdl.index]) :
                                            BIT_CLR_(dcd_data.ioc_dd, dcd_data.ddat[edpt_hdl.index]);
  }

  p_fixed_dd->is_retired = 0;

  dcd_data.udca[edpt_hdl.index] = p_fixed_dd;

  LPC_USB->USBEpDMAEn = BIT_(edpt_hdl.index);

  if ( edpt_hdl.index % 2 )
  { // endpoint IN need to actively raise DMA request
    LPC_USB->USBDMARSet = BIT_(edpt_hdl.index);
  }

  return TUSB_ERROR_NONE;
}

#endif
