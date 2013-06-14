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

#if MODE_DEVICE_SUPPORTED && (MCU == MCU_LPC175X_6X)

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
STATIC_ dcd_dma_descriptor_t* dcd_udca[32] ATTR_ALIGNED(128) TUSB_CFG_ATTR_USBRAM;
STATIC_ dcd_dma_descriptor_t  dcd_dd[DCD_MAX_DD];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static inline void endpoint_set_max_packet_size(uint8_t endpoint_idx, uint16_t max_packet_size) ATTR_ALWAYS_INLINE;
static inline void endpoint_set_max_packet_size(uint8_t endpoint_idx, uint16_t max_packet_size)
{
  LPC_USB->USBEpInd    = endpoint_idx; // select index before setting packet size
  LPC_USB->USBMaxPSize = max_packet_size;
}

static inline void sie_commamd_code (uint8_t phase, uint8_t code_data) ATTR_ALWAYS_INLINE;
static inline void sie_commamd_code (uint8_t phase, uint8_t code_data)
{
  LPC_USB->USBDevIntClr = (DEV_INT_COMMAND_CODE_EMPTY_MASK | DEV_INT_COMMAND_DATA_FULL_MASK);
  LPC_USB->USBCmdCode = (phase << 8) | (code_data << 16);

  uint32_t const wait_flag = (phase == SIE_CMDPHASE_READ) ? DEV_INT_COMMAND_DATA_FULL_MASK : DEV_INT_COMMAND_CODE_EMPTY_MASK;
#ifndef _TEST_
  while ((LPC_USB->USBDevIntSt & wait_flag) == 0); // TODO blocking forever potential
#endif
  LPC_USB->USBDevIntClr = wait_flag;
}

static inline void sie_command_write (uint8_t cmd_code, uint8_t data_len, uint8_t data) ATTR_ALWAYS_INLINE;
static inline void sie_command_write (uint8_t cmd_code, uint8_t data_len, uint8_t data)
{
  sie_commamd_code(SIE_CMDPHASE_COMMAND, cmd_code);

  if (data_len)
  {
    sie_commamd_code(SIE_CMDPHASE_WRITE, data);
  }
}

static inline uint32_t sie_command_read (uint8_t cmd_code, uint8_t data_len) ATTR_ALWAYS_INLINE;
static inline uint32_t sie_command_read (uint8_t cmd_code, uint8_t data_len)
{
  // TODO multiple read
  sie_commamd_code(SIE_CMDPHASE_COMMAND, cmd_code);
  sie_commamd_code(SIE_CMDPHASE_READ, cmd_code);
  return LPC_USB->USBCmdData;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void endpoint_control_isr(uint8_t coreid)
{
  (void) coreid; // suppress compiler warning
  uint32_t const endpoint_int_status = LPC_USB->USBEpIntSt & LPC_USB->USBEpIntEn;

  // control OUT
  if (endpoint_int_status & BIT_(0))
  {
    uint32_t const endpoint_status = sie_command_read(SIE_CMDCODE_ENDPOINT_SELECT+0, 1);
    if (endpoint_status & SIE_ENDPOINT_STATUS_SETUP_RECEIVED_MASK)
    {
      (void) sie_command_read(SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT+0, 1); // clear setup bit

      // read endpoint
      LPC_USB->USBCtrl = SLAVE_CONTROL_READ_ENABLE_MASK; // logical endpoint = 0
      while ((LPC_USB->USBRxPLen & SLAVE_RXPLEN_PACKET_READY_MASK) == 0) {}

      ASSERT_INT(8, LPC_USB->USBRxPLen & SLAVE_RXPLEN_PACKET_LENGTH_MASK, (void) 0);

      uint32_t *p_setup = (uint32_t*) &usbd_devices[0].setup_packet;
      *p_setup = LPC_USB->USBRxData;
      p_setup++; // increase by 4 ( sizeof(uint32_t) )
      *p_setup = LPC_USB->USBRxData;

      LPC_USB->USBCtrl = 0;

      // TODO setup received callback
      usbd_isr(0, TUSB_EVENT_SETUP_RECEIVED);
    }else
    {
       // RxPLen should be zero for zero-length status phase. Current not support any out control with data yet
      ASSERT(LPC_USB->USBRxPLen == 0, (void) 0);
    }
    sie_command_write(SIE_CMDCODE_ENDPOINT_SELECT+0, 0, 0);
    sie_command_write(SIE_CMDCODE_BUFFER_CLEAR, 0, 0);
  }

  // control IN
  if (endpoint_int_status & BIT_(1))
  {
    (void) endpoint_int_status;
  }

  LPC_USB->USBEpIntClr = endpoint_int_status; // acknowledge interrupt
}

void dcd_isr(uint8_t coreid)
{
  uint32_t const device_int_status = LPC_USB->USBDevIntSt & LPC_USB->USBDevIntEn & DEV_INT_ALL_MASK;
  LPC_USB->USBDevIntClr = device_int_status;// Acknowledge handled interrupt

  //------------- usb bus event -------------//
  if (device_int_status & DEV_INT_DEVICE_STATUS_MASK)
  {
    uint32_t const dev_status_reg = sie_command_read(SIE_CMDCODE_DEVICE_STATUS, 1);
    if (dev_status_reg & SIE_DEV_STATUS_RESET_MASK)
    {
      usbd_isr(coreid, TUSB_EVENT_BUS_RESET);
    }

    // TODO invoke some callbacks
    if (dev_status_reg & SIE_DEV_STATUS_CONNECT_CHANGE_MASK)
    {

    }
    if (dev_status_reg & SIE_DEV_STATUS_SUSPEND_CHANGE_MASK)
    {

    }
  }

  //------------- slave mode, control endpoint -------------//
  if (device_int_status & DEV_INT_ENDPOINT_SLOW_MASK)
  {
    // only occur on control endpoint, all other use DMA
    endpoint_control_isr(coreid);
  }

  if (device_int_status & DEV_INT_ERROR_MASK)
  {
    uint32_t error_status = sie_command_read(SIE_CMDCODE_READ_ERROR_STATUS, 1);
    (void) error_status;
//    ASSERT(false, (void) 0);
  }

}

//--------------------------------------------------------------------+
// USBD-DCD API
//--------------------------------------------------------------------+
tusb_error_t dcd_init(void)
{
  //------------- user manual 11.13 usb device controller initialization -------------//  LPC_USB->USBEpInd = 0;
  // step 6 : set up control endpoint
  endpoint_set_max_packet_size(0, TUSB_CFG_DEVICE_CONTROL_PACKET_SIZE);
  endpoint_set_max_packet_size(1, TUSB_CFG_DEVICE_CONTROL_PACKET_SIZE);

#ifndef _TEST_
	while ((LPC_USB->USBDevIntSt & DEV_INT_ENDPOINT_REALIZED_MASK) == 0);
#endif

	// step 7 : slave mode set up
	LPC_USB->USBEpIntEn   = (uint32_t) BIN8(11); // control endpoint cannot use DMA, non-control all use DMA

	LPC_USB->USBDevIntEn	= (DEV_INT_DEVICE_STATUS_MASK | DEV_INT_ENDPOINT_SLOW_MASK | DEV_INT_ERROR_MASK);
	LPC_USB->USBDevIntClr	= 0xFFFFFFFF; // clear all pending interrupt

	LPC_USB->USBEpIntClr	= 0xFFFFFFFF; // clear all pending interrupt
	LPC_USB->USBEpIntPri	= 0; // same priority for all endpoint

	// step 8 : DMA set up
	LPC_USB->USBEpDMADis     = 0xFFFFFFFF; // firstly disable all dma
	LPC_USB->USBDMARClr      = 0xFFFFFFFF; // clear all pending interrupt
	LPC_USB->USBEoTIntClr    = 0xFFFFFFFF;
	LPC_USB->USBNDDRIntClr   = 0xFFFFFFFF;
	LPC_USB->USBSysErrIntClr = 0xFFFFFFFF;

	for (uint8_t index = 0; index < DCD_MAX_DD; index++)
	{
		dcd_udca[index] = dcd_dd + index;
	}
	LPC_USB->USBUDCAH    = (uint32_t) dcd_udca;
	LPC_USB->USBDMAIntEn = (DMA_INT_END_OF_XFER_MASK | DMA_INT_NEW_DD_REQUEST_MASK | DMA_INT_ERROR_MASK );

	// clear all stall on control endpoint IN & OUT if any
	sie_command_write(SIE_CMDCODE_ENDPOINT_SET_STATUS    , 1, 0);
  sie_command_write(SIE_CMDCODE_ENDPOINT_SET_STATUS + 1, 1, 0);

  return TUSB_ERROR_NONE;
}

static inline uint8_t endpoint_address_to_physical_index(uint8_t ep_address) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint8_t endpoint_address_to_physical_index(uint8_t ep_address)
{
  return (ep_address << 1) + (ep_address & 0x80 ? 1 : 0 );
}

tusb_error_t dcd_endpoint_configure(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc)
{
  uint8_t phy_ep = endpoint_address_to_physical_index( p_endpoint_desc->bEndpointAddress );

  //------------- Realize Endpoint with Max Packet Size -------------//
  LPC_USB->USBReEp |= BIT_(phy_ep);
  endpoint_set_max_packet_size(phy_ep, p_endpoint_desc->wMaxPacketSize.size);

#ifndef _TEST_
	while ((LPC_USB->USBDevIntSt & DEV_INT_ENDPOINT_REALIZED_MASK) == 0) {} // TODO can be omitted, or move to set max packet size
	LPC_USB->USBDevIntClr = DEV_INT_ENDPOINT_REALIZED_MASK;
#endif

	//------------- DMA set up -------------//
	memclr_(dcd_dd + phy_ep, sizeof(dcd_dma_descriptor_t));
	dcd_dd[phy_ep].is_isochronous  = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) ? 1 : 0;
	dcd_dd[phy_ep].max_packet_size = p_endpoint_desc->wMaxPacketSize.size;
	dcd_dd[phy_ep].is_retired      = 1; // dd is not active at first

	LPC_USB->USBEpDMAEn = BIT_(phy_ep);

	sie_command_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+phy_ep, 1, 0); // clear all endpoint status

  return TUSB_ERROR_NONE;
}

void dcd_controller_connect(uint8_t coreid)
{
  sie_command_write(SIE_CMDCODE_DEVICE_STATUS, 1, 1);
}

void dcd_device_set_address(uint8_t coreid, uint8_t dev_addr)
{
  sie_command_write(SIE_CMDCODE_SET_ADDRESS, 1, 0x80 | dev_addr); // 7th bit is : device_enable
}

void dcd_device_set_configuration(uint8_t coreid, uint8_t config_num)
{
  (void) config_num; // supress compiler's warnings
  sie_command_write(SIE_CMDCODE_CONFIGURE_DEVICE, 1, 1);
}

static inline uint16_t length_unit_byte2dword(uint16_t length_in_bytes) ATTR_ALWAYS_INLINE ATTR_CONST;
static inline uint16_t length_unit_byte2dword(uint16_t length_in_bytes)
{
  return (length_in_bytes + 3) / 4; // length_in_dword
}

tusb_error_t dcd_pipe_control_write(uint8_t coreid, void const * buffer, uint16_t length)
{
  (void) coreid; // suppress compiler warning

  ASSERT( length !=0 || buffer == NULL, TUSB_ERROR_INVALID_PARA);

  LPC_USB->USBCtrl   = SLAVE_CONTROL_WRITE_ENABLE_MASK; // logical endpoint = 0
	LPC_USB->USBTxPLen = length;

	for (uint16_t count = 0; count < length_unit_byte2dword(length); count++)
	{
		LPC_USB->USBTxData = *((uint32_t *)buffer); // NOTE: cortex M3 have no problem with alignment
		buffer += 4;
	}

	LPC_USB->USBCtrl   = 0;

	sie_command_write(SIE_CMDCODE_ENDPOINT_SELECT+1, 0, 0); // select control IN endpoint
	sie_command_write(SIE_CMDCODE_BUFFER_VALIDATE, 0, 0);

  return TUSB_ERROR_NONE;
}

tusb_error_t dcd_pipe_control_read(uint8_t coreid, void * buffer, uint16_t length)
{
  LPC_USB->USBCtrl = SLAVE_CONTROL_READ_ENABLE_MASK; // logical endpoint = 0
  while ((LPC_USB->USBRxPLen & SLAVE_RXPLEN_PACKET_READY_MASK) == 0) {}

  uint16_t actual_length = min16_of(length, (uint16_t) (LPC_USB->USBRxPLen & SLAVE_RXPLEN_PACKET_LENGTH_MASK) );
  uint32_t *p_read_data = (uint32_t*) buffer;
  for( uint16_t count=0; count < length_unit_byte2dword(actual_length); count++)
  {
    *p_read_data = LPC_USB->USBRxData;
    p_read_data++; // increase by 4 ( sizeof(uint32_t) )
  }
  LPC_USB->USBCtrl = 0;

  return TUSB_ERROR_NONE;
}

// TODO inline function
void dcd_pipe_control_write_zero_length(uint8_t coreid)
{
  dcd_pipe_control_write(coreid, NULL, 0);
}

#endif
