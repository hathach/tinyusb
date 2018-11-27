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

#if TUSB_OPT_DEVICE_ENABLED && (CFG_TUSB_MCU == OPT_MCU_LPC175X_6X)

#include "device/dcd.h"
#include "dcd_lpc175x_6x.h"
#include "LPC17xx.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_QHD_MAX 32
#define DCD_QTD_MAX  32 // TODO scale with configure

typedef struct ATTR_ALIGNED(4)
{
	//------------- Word 0 -------------//
	uint32_t next;

	//------------- Word 1 -------------//
	uint16_t mode            : 2; // either 00 normal or 01 ATLE(auto length extraction)
	uint16_t next_valid      : 1;
	uint16_t int_on_complete : 1; ///< make use of reserved bit
	uint16_t isochronous     : 1; // is an iso endpoint
	uint16_t max_packet_size : 11;
	volatile uint16_t buffer_length;

	//------------- Word 2 -------------//
	volatile uint32_t buffer_addr;

	//------------- Word 3 -------------//
	volatile uint16_t retired                      : 1; // initialized to zero
	volatile uint16_t status                       : 4;
	volatile uint16_t iso_last_packet_valid        : 1;
	volatile uint16_t atle_lsb_extracted           : 1;	// used in ATLE mode
	volatile uint16_t atle_msb_extracted           : 1;	// used in ATLE mode
	volatile uint16_t atle_message_length_position : 6; // used in ATLE mode
	uint16_t                                       : 2;
	volatile uint16_t present_count; // The number of bytes transferred by the DMA engine. The DMA engine updates this field after completing each packet transfer.

	//------------- Word 4 -------------//
//	uint32_t iso_packet_size_addr;		// iso only, can be omitted for non-iso
}dcd_dma_descriptor_t;

TU_VERIFY_STATIC( sizeof(dcd_dma_descriptor_t) == 16, "size is not correct"); // TODO not support ISO for now

typedef struct
{
  // must be 128 byte aligned
  volatile dcd_dma_descriptor_t* udca[DCD_QHD_MAX];

  // each endpoints can have up to 2 DD queued at a time
  // TODO DMA does not support control transfer (0-1 are not used, offset to reduce memory)
  dcd_dma_descriptor_t dd[DCD_QTD_MAX][2];

  struct
  {
    uint8_t* out_buffer;
    uint8_t  out_bytes;
    volatile bool out_received; // indicate if data is already received in endpoint

    uint8_t  in_bytes;
  } control;

} dcd_data_t;

CFG_TUSB_MEM_SECTION ATTR_ALIGNED(128) static dcd_data_t _dcd;


//--------------------------------------------------------------------+
// SIE Command
//--------------------------------------------------------------------+
static void sie_cmd_code (sie_cmdphase_t phase, uint8_t code_data)
{
  LPC_USB->USBDevIntClr = (DEV_INT_COMMAND_CODE_EMPTY_MASK | DEV_INT_COMMAND_DATA_FULL_MASK);
  LPC_USB->USBCmdCode   = (phase << 8) | (code_data << 16);

  uint32_t const wait_flag = (phase == SIE_CMDPHASE_READ) ? DEV_INT_COMMAND_DATA_FULL_MASK : DEV_INT_COMMAND_CODE_EMPTY_MASK;
#ifndef _TEST_
  while ((LPC_USB->USBDevIntSt & wait_flag) == 0); // TODO blocking forever potential
#endif
  LPC_USB->USBDevIntClr = wait_flag;
}

static void sie_write (uint8_t cmd_code, uint8_t data_len, uint8_t data)
{
  sie_cmd_code(SIE_CMDPHASE_COMMAND, cmd_code);

  if (data_len)
  {
    sie_cmd_code(SIE_CMDPHASE_WRITE, data);
  }
}

static uint32_t sie_read (uint8_t cmd_code, uint8_t data_len)
{
  // TODO multiple read
  sie_cmd_code(SIE_CMDPHASE_COMMAND , cmd_code);
  sie_cmd_code(SIE_CMDPHASE_READ    , cmd_code);
  return LPC_USB->USBCmdData;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
static inline uint8_t edpt_addr2phy(uint8_t ep_addr)
{
  return 2*(ep_addr & 0x0F) + ((ep_addr & TUSB_DIR_IN_MASK) ? 1 : 0);
}

static inline void edpt_set_max_packet_size(uint8_t ep_id, uint16_t max_packet_size)
{
  // follows example in 11.10.4.2
  LPC_USB->USBReEp    |= BIT_(ep_id);
  LPC_USB->USBEpInd    = ep_id; // select index before setting packet size
  LPC_USB->USBMaxPSize = max_packet_size;

  while ((LPC_USB->USBDevIntSt & DEV_INT_ENDPOINT_REALIZED_MASK) == 0) {} // TODO can be omitted
  LPC_USB->USBDevIntClr = DEV_INT_ENDPOINT_REALIZED_MASK;
}


//--------------------------------------------------------------------+
// CONTROLLER API
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

	tu_memclr(&_dcd, sizeof(dcd_data_t));
}

bool dcd_init(uint8_t rhport)
{
  (void) rhport;

  //------------- user manual 11.13 usb device controller initialization -------------//  LPC_USB->USBEpInd = 0;
  // step 6 : set up control endpoint
  edpt_set_max_packet_size(0, CFG_TUD_ENDOINT0_SIZE);
  edpt_set_max_packet_size(1, CFG_TUD_ENDOINT0_SIZE);

  bus_reset();

  LPC_USB->USBDevIntEn = (DEV_INT_DEVICE_STATUS_MASK | DEV_INT_ENDPOINT_SLOW_MASK | DEV_INT_ERROR_MASK);
  LPC_USB->USBUDCAH = (uint32_t) _dcd.udca;
  LPC_USB->USBDMAIntEn = (DMA_INT_END_OF_XFER_MASK | DMA_INT_ERROR_MASK);

  sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, 1);    // connect

  NVIC_EnableIRQ(USB_IRQn);

  return TUSB_ERROR_NONE;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, 1);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  sie_write(SIE_CMDCODE_SET_ADDRESS, 1, 0x80 | dev_addr); // 7th bit is : device_enable
}

void dcd_set_config(uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  sie_write(SIE_CMDCODE_CONFIGURE_DEVICE, 1, 1);
}

//--------------------------------------------------------------------+
// CONTROL HELPER
//--------------------------------------------------------------------+
static inline uint8_t byte2dword(uint8_t bytes)
{
  // length in dwords
  return (bytes + 3) / 4;
}

static void control_ep_write(void const * buffer, uint8_t len)
{
  uint32_t const * buf32 = (uint32_t const *) buffer;

  LPC_USB->USBCtrl   = USBCTRL_WRITE_ENABLE_MASK; // logical endpoint = 0
	LPC_USB->USBTxPLen = (uint32_t) len;

	for (uint8_t count = 0; count < byte2dword(len); count++)
	{
		LPC_USB->USBTxData = *buf32; // NOTE: cortex M3 have no problem with alignment
		buf32++;
	}

	LPC_USB->USBCtrl = 0;

	// select control IN & validate the endpoint
	sie_write(SIE_CMDCODE_ENDPOINT_SELECT+1, 0, 0);
	sie_write(SIE_CMDCODE_BUFFER_VALIDATE  , 0, 0);
}

static uint8_t control_ep_read(void * buffer, uint8_t len)
{
  LPC_USB->USBCtrl = USBCTRL_READ_ENABLE_MASK; // logical endpoint = 0
  while ((LPC_USB->USBRxPLen & USBRXPLEN_PACKET_READY_MASK) == 0) {} // TODO blocking, should have timeout

  len = tu_min8(len, (uint8_t) (LPC_USB->USBRxPLen & USBRXPLEN_PACKET_LENGTH_MASK) );
  uint32_t *buf32 = (uint32_t*) buffer;

  for (uint8_t count=0; count < byte2dword(len); count++)
  {
    *buf32 = LPC_USB->USBRxData;
    buf32++;
  }

  LPC_USB->USBCtrl = 0;

  // select control OUT & clear the endpoint
  sie_write(SIE_CMDCODE_ENDPOINT_SELECT+0, 0, 0);
  sie_write(SIE_CMDCODE_BUFFER_CLEAR     , 0, 0);

  return len;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void) rhport;

  // TODO refractor to universal pipe open validation function
//  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) return null_handle; // TODO not support ISO yet
//  TU_ASSERT (p_endpoint_desc->wMaxPacketSize.size <= 64, null_handle); // TODO ISO can be 1023, but ISO not supported now

  uint8_t ep_id = edpt_addr2phy( p_endpoint_desc->bEndpointAddress );

  //------------- Realize Endpoint with Max Packet Size -------------//
  edpt_set_max_packet_size(ep_id, p_endpoint_desc->wMaxPacketSize.size);

	//------------- first DD prepare -------------//
	dcd_dma_descriptor_t* const p_dd = &_dcd.dd[ep_id][0];
	tu_memclr(p_dd, sizeof(dcd_dma_descriptor_t));

	p_dd->isochronous  = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) ? 1 : 0;
	p_dd->max_packet_size = p_endpoint_desc->wMaxPacketSize.size;
	p_dd->retired      = 1; // inactive at first

	_dcd.udca[ ep_id ] = p_dd; // hook to UDCA

	sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, 0); // clear all endpoint status

  return true;
}

bool dcd_edpt_busy(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t ep_id = edpt_addr2phy( ep_addr );
  return (_dcd.udca[ep_id] != NULL && !_dcd.udca[ep_id]->retired);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  if ( edpt_number(ep_addr) == 0 )
  {
    sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+0, 1, SIE_SET_ENDPOINT_STALLED_MASK | SIE_SET_ENDPOINT_CONDITION_STALLED_MASK);
  }else
  {
    uint8_t ep_id = edpt_addr2phy( ep_addr );
    sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, SIE_SET_ENDPOINT_STALLED_MASK);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t ep_id = edpt_addr2phy(ep_addr);

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, 0);
}

bool dcd_edpt_stalled (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  // TODO implement later
  return false;
}

void dd_xfer_init(dcd_dma_descriptor_t* p_dd, void* buffer, uint16_t total_bytes)
{
  p_dd->next                  = 0;
  p_dd->next_valid         = 0;
  p_dd->buffer_addr           = (uint32_t) buffer;
  p_dd->buffer_length         = total_bytes;
  p_dd->status                = DD_STATUS_NOT_SERVICED;
  p_dd->iso_last_packet_valid = 0;
  p_dd->present_count         = 0;
}

//tusb_error_t dcd_edpt_queue_xfer(edpt_hdl_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes)
//{ // NOTE for sure the qhd has no dds
//  dcd_dma_descriptor_t* const p_fixed_dd = &dcd_data.dd[edpt_hdl.index][0]; // always queue with the fixed DD
//
//  dd_xfer_init(p_fixed_dd, buffer, total_bytes);
//  p_fixed_dd->is_retired      = 1;
//  p_fixed_dd->int_on_complete = 0;
//
//  return TUSB_ERROR_NONE;
//}

static bool control_xact(uint8_t rhport, uint8_t dir, uint8_t * buffer, uint8_t len)
{
  (void) rhport;

  if ( dir )
  {
    _dcd.control.in_bytes = len;
    control_ep_write(buffer, len);
  }else
  {
    if ( _dcd.control.out_received )
    {
      // Already received the DATA OUT packet
      _dcd.control.out_received = false;
      _dcd.control.out_buffer = NULL;
      _dcd.control.out_bytes  = 0;

      uint8_t received = control_ep_read(buffer, len);
      dcd_event_xfer_complete(0, 0, received, XFER_RESULT_SUCCESS, true);
    }else
    {
      _dcd.control.out_buffer = buffer;
      _dcd.control.out_bytes  = len;
    }
  }

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  // Control transfer is not DMA support, and must be done in slave mode
  if ( epnum == 0 )
  {
    return control_xact(rhport, dir, buffer, (uint8_t) total_bytes);
  }

  uint8_t ep_id = edpt_addr2phy(ep_addr);
  dcd_dma_descriptor_t* const p_first_dd = &_dcd.dd[ep_id][0];

  //------------- fixed DD is already queued a xfer -------------//
  if ( p_first_dd->buffer_length )
  {
    // setup new dd
    dcd_dma_descriptor_t* const p_dd = &_dcd.dd[ ep_id ][1];
    tu_memclr(p_dd, sizeof(dcd_dma_descriptor_t));

    dd_xfer_init(p_dd, buffer, total_bytes);

    p_dd->max_packet_size  = p_first_dd->max_packet_size;
    p_dd->isochronous      = p_first_dd->isochronous;
    p_dd->int_on_complete  = true;

    // hook to fixed dd
    p_first_dd->next       = (uint32_t) p_dd;
    p_first_dd->next_valid = 1;
  }
  //------------- fixed DD is free -------------//
  else
  {
    dd_xfer_init(p_first_dd, buffer, total_bytes);
    p_first_dd->int_on_complete = true;
  }

  p_first_dd->retired = 0; // activate xfer
  _dcd.udca[ep_id] = p_first_dd;
  LPC_USB->USBEpDMAEn = BIT_(ep_id);

  if ( ep_id % 2 )
  {
    // endpoint IN need to actively raise DMA request
    LPC_USB->USBDMARSet = BIT_(ep_id);
  }

  return true;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
static void endpoint_non_control_isr(uint32_t eot_int)
{
  for(uint8_t ep_id = 2; ep_id < DCD_QHD_MAX; ep_id++ )
  {
    if ( BIT_TEST_(eot_int, ep_id) )
    {
      dcd_dma_descriptor_t* const p_first_dd = &_dcd.dd[ep_id][0];
      dcd_dma_descriptor_t* const p_last_dd  = _dcd.dd[ep_id] + (p_first_dd->next_valid ? 1 : 0); // Maximum is 2 QTD are queued in an endpoint

      // only handle when Controller already finished the last DD
      if ( _dcd.udca[ep_id] == p_last_dd )
      {
        _dcd.udca[ep_id] = p_first_dd; // UDCA currently points to the last DD, change to the fixed DD
        p_first_dd->buffer_length = 0; // buffer length is used to determined if first dd is queued in pipe xfer function

        if ( p_last_dd->int_on_complete )
        {
          uint8_t result = (p_last_dd->status == DD_STATUS_NORMAL || p_last_dd->status == DD_STATUS_DATA_UNDERUN) ? XFER_RESULT_SUCCESS : XFER_RESULT_FAILED;

          // report only xferred bytes in the IOC qtd
          uint8_t const ep_addr = (ep_id/2) | ( (ep_id & 0x01) ? TUSB_DIR_IN_MASK : 0 );
          dcd_event_xfer_complete(0, ep_addr, p_last_dd->present_count, result, true);
        }
      }
    }
  }
}

static void control_xfer_isr(uint8_t rhport)
{
  uint32_t const ep_int_status = LPC_USB->USBEpIntSt & LPC_USB->USBEpIntEn;
//  LPC_USB->USBEpIntClr = ep_int_status; // acknowledge interrupt TODO cannot immediately acknowledge setup packet

  //------------- Setup Received-------------//
  if ( (ep_int_status & BIT_(0)) &&
       (sie_read(SIE_CMDCODE_ENDPOINT_SELECT+0, 1) & SIE_SELECT_ENDPOINT_SETUP_RECEIVED_MASK) )
  {
    (void) sie_read(SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT+0, 1); // clear setup bit

    uint8_t setup_packet[8];
    control_ep_read(setup_packet, 8); // TODO read before clear setup above

    dcd_event_setup_received(rhport, setup_packet, true);
  }
  else if (ep_int_status & 0x03)
  {
    // Control out complete
    if ( ep_int_status & BIT_(0) )
    {
      if ( _dcd.control.out_buffer )
      {
        // software queued transfer previously
        uint8_t received = control_ep_read(_dcd.control.out_buffer, _dcd.control.out_bytes);

        _dcd.control.out_buffer = NULL;
        _dcd.control.out_bytes = 0;

        dcd_event_xfer_complete(rhport, 0, received, XFER_RESULT_SUCCESS, true);
      }else
      {
        // mark as received
        _dcd.control.out_received = true;
      }
    }

    // Control In complete
    if ( ep_int_status & BIT_(1) )
    {
      dcd_event_xfer_complete(rhport, TUSB_DIR_IN_MASK, _dcd.control.in_bytes, XFER_RESULT_SUCCESS, true);
    }
  }

  LPC_USB->USBEpIntClr = ep_int_status; // acknowledge interrupt TODO cannot immediately acknowledge setup packet
}

void hal_dcd_isr(uint8_t rhport)
{
  (void) rhport;

  uint32_t const device_int_enable = LPC_USB->USBDevIntEn;
  uint32_t const device_int_status = LPC_USB->USBDevIntSt & device_int_enable;
  LPC_USB->USBDevIntClr = device_int_status;// Acknowledge handled interrupt

  //------------- usb bus event -------------//
  if (device_int_status & DEV_INT_DEVICE_STATUS_MASK)
  {
    uint8_t const dev_status_reg = sie_read(SIE_CMDCODE_DEVICE_STATUS, 1);
    if (dev_status_reg & SIE_DEV_STATUS_RESET_MASK)
    {
      bus_reset();
      dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET, true);
    }

    if (dev_status_reg & SIE_DEV_STATUS_CONNECT_CHANGE_MASK)
    { // device is disconnected, require using VBUS (P1_30)
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
    }

    if (dev_status_reg & SIE_DEV_STATUS_SUSPEND_CHANGE_MASK)
    {
      if (dev_status_reg & SIE_DEV_STATUS_SUSPEND_MASK)
      {
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPENDED, true);
      }
//        else
//      { // resume signal
//        dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
//      }
//    }
    }
  }

  //------------- Control Endpoint (Slave Mode) -------------//
  if (device_int_status & DEV_INT_ENDPOINT_SLOW_MASK)
  {
    control_xfer_isr(rhport);
  }

  //------------- Non-Control Endpoint (DMA Mode) -------------//
  uint32_t const dma_int_enable = LPC_USB->USBDMAIntEn;
  uint32_t const dma_int_status = LPC_USB->USBDMAIntSt & dma_int_enable;

  if (dma_int_status & DMA_INT_END_OF_XFER_MASK)
  {
    uint32_t eot_int = LPC_USB->USBEoTIntSt;
    LPC_USB->USBEoTIntClr = eot_int; // acknowledge interrupt source

    endpoint_non_control_isr(eot_int);
  }

  if (device_int_status & DEV_INT_ERROR_MASK || dma_int_status & DMA_INT_ERROR_MASK)
  {
    uint32_t error_status = sie_read(SIE_CMDCODE_READ_ERROR_STATUS, 1);
    (void) error_status;
//    TU_ASSERT(false, (void) 0);
  }
}

#endif
