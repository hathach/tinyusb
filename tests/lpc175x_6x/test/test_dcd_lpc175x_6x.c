/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include <stdlib.h>
#include "unity.h"
#include "tusb_errors.h"
#include "type_helper.h"

#include "mock_usbd_dcd.h"
#include "dcd_lpc175x_6x.h"

LPC_USB_TypeDef lpc_usb;
usbd_device_info_t usbd_devices[CONTROLLER_DEVICE_NUMBER];
extern dcd_dma_descriptor_t* dcd_udca[32];
extern dcd_dma_descriptor_t  dcd_dd[DCD_MAX_DD];

void setUp(void)
{
  tu_memclr(dcd_udca, 32*4);
  tu_memclr(dcd_dd, sizeof(dcd_dma_descriptor_t)*DCD_MAX_DD);
  tu_memclr(&lpc_usb, sizeof(LPC_USB_TypeDef));
}

void tearDown(void)
{
}

void test_dd_udca_align(void)
{
  TEST_ASSERT_BITS_LOW(128-1, (uint32_t) dcd_udca);
}

void test_dd_structure(void)
{
  //------------- word 0 -------------//
  TEST_ASSERT_EQUAL( 0, offsetof(dcd_dma_descriptor_t, next));

  //------------- word 1 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, mode) );
  TEST_ASSERT_EQUAL( 2, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, is_next_valid) );
  TEST_ASSERT_EQUAL( 4, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, is_isochronous) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, max_packet_size) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 1, buffer_length) );

  //------------- word 2 -------------//
  TEST_ASSERT_EQUAL( 8, offsetof(dcd_dma_descriptor_t, buffer_start_addr) );

  //------------- word 3 -------------//
  TEST_ASSERT_EQUAL( 0, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, is_retired) );
  TEST_ASSERT_EQUAL( 1, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, status) );
  TEST_ASSERT_EQUAL( 5, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, iso_last_packet_valid) );
  TEST_ASSERT_EQUAL( 6, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_is_lsb_extracted) );
  TEST_ASSERT_EQUAL( 7, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_is_msb_extracted) );
  TEST_ASSERT_EQUAL( 8, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, atle_message_length_position) );
  TEST_ASSERT_EQUAL( 16, BITFIELD_OFFSET_OF_UINT32(dcd_dma_descriptor_t, 3, present_count) );

  //------------- word 4 -------------//
}

void test_dcd_init(void)
{

  //------------- Code Under Test -------------//
  dcd_init();

  //------------- slave check -------------//
  TEST_ASSERT_EQUAL_HEX( TU_BIN8(11), LPC_USB->USBEpIntEn );
  TEST_ASSERT_EQUAL_HEX( DEV_INT_DEVICE_STATUS_MASK | DEV_INT_ENDPOINT_SLOW_MASK | DEV_INT_ERROR_MASK,
                         LPC_USB->USBDevIntEn );
  TEST_ASSERT_EQUAL_HEX( 0, LPC_USB->USBEpIntPri);

  //------------- DMA check -------------//
  for (uint32_t i=0; i<32; i++)
  {
    TEST_ASSERT_EQUAL_HEX(dcd_dd+i, dcd_udca[i]);
  }

  TEST_ASSERT_EQUAL_HEX(dcd_udca, LPC_USB->USBUDCAH);
  TEST_ASSERT_EQUAL_HEX(DMA_INT_END_OF_XFER_MASK | DMA_INT_NEW_DD_REQUEST_MASK | DMA_INT_ERROR_MASK,
                        LPC_USB->USBDMAIntEn);

}

void test_dcd_configure_endpoint_in(void)
{
  tusb_desc_endpoint_t const desc_endpoint =
  {
      .bLength          = sizeof(tusb_desc_endpoint_t),
      .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
      .bEndpointAddress = 0x83,
      .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
      .wMaxPacketSize   = { .size = 0x08 },
      .bInterval        = 0x0A
  };

  dcd_init();
  tu_memclr(&lpc_usb, sizeof(LPC_USB_TypeDef)); // clear to examine register after CUT

  //------------- Code Under Test -------------//
  dcd_pipe_open(0, &desc_endpoint);

  uint8_t const phy_ep = 2*3 + 1;
  TEST_ASSERT_EQUAL_HEX( TU_BIT(phy_ep), LPC_USB->USBReEp);
  TEST_ASSERT_EQUAL_HEX( phy_ep, LPC_USB->USBEpInd);
  TEST_ASSERT_EQUAL( desc_endpoint.wMaxPacketSize.size, LPC_USB->USBMaxPSize);

  TEST_ASSERT_FALSE( dcd_dd[phy_ep].is_next_valid );
  TEST_ASSERT_EQUAL_HEX( 0, dcd_dd[phy_ep].next );
  TEST_ASSERT_EQUAL( 0, dcd_dd[phy_ep].mode);
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].is_isochronous );
  TEST_ASSERT_EQUAL( 0, dcd_dd[phy_ep].buffer_length);
  TEST_ASSERT_EQUAL_HEX( 0, dcd_dd[phy_ep].buffer_start_addr);
  TEST_ASSERT_TRUE( dcd_dd[phy_ep].is_retired );
  TEST_ASSERT_EQUAL( DD_STATUS_NOT_SERVICED, dcd_dd[phy_ep].status);
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].iso_last_packet_valid );
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].atle_is_lsb_extracted );
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].atle_is_msb_extracted );
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].atle_message_length_position );
  TEST_ASSERT_FALSE( dcd_dd[phy_ep].present_count );
}

void test_dcd_configure_endpoint_out(void)
{
  TEST_IGNORE();
}
