/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
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
 */

#include "unity.h"

// Files to test
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "common/tusb_private.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")

// Mock File
#include "mock_dcd.h"
#include "mock_msc_device.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint32_t tusb_time_millis_api(void) {
  return 0;
}

enum
{
  EDPT_CTRL_OUT = 0x00,
  EDPT_CTRL_IN  = 0x80
};

uint8_t const rhport = 0;

// bMaxPacketSize0 is a single byte holding the full/high-speed EP0 size (64 max); SuperSpeed
// encodes it as the exponent (9 == 512) in a separate descriptor. CFG_TUD_ENDPOINT0_SIZE is 512
// on a SuperSpeed-capable build and would truncate to 0 in this field, so clamp it the way the
// examples' descriptors do (EP0_SIZE_FSHS).
#define EP0_SIZE_FSHS   ((uint8_t)(CFG_TUD_ENDPOINT0_SIZE > 64 ? 64 : CFG_TUD_ENDPOINT0_SIZE))

tusb_desc_device_t const data_desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = EP0_SIZE_FSHS,

    .idVendor           = 0xCafe,
    .idProduct          = 0xCafe,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const data_desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, 0, 0, TUD_CONFIG_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
};

tusb_control_request_t const req_get_desc_device =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_DEVICE << 8),
  .wIndex = 0x0000,
  .wLength = 64
};

static void switch_to_superspeed(void);
static void switch_to_highspeed(void);

tusb_control_request_t const req_get_desc_configuration =
{
  .bmRequestType = 0x80,
  .bRequest = TUSB_REQ_GET_DESCRIPTOR,
  .wValue = (TUSB_DESC_CONFIGURATION << 8),
  .wIndex = 0x0000,
  .wLength = 256
};

// Vendor OUT control request (direction OUT, type Vendor, recipient Device), 8-byte data stage
tusb_control_request_t const req_vendor_out =
{
  .bmRequestType = 0x40,
  .bRequest = 0x01,
  .wValue = 0x0000,
  .wIndex = 0x0000,
  .wLength = 8
};

uint8_t const* desc_device;
uint8_t const* desc_configuration;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
uint8_t const * tud_descriptor_device_cb(void) {
  return desc_device;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;

  return NULL;
}

// Backing buffer for the vendor OUT data stage. Sized to EP0 max packet so an (untested) regression
// that drops the clamp can't corrupt memory here; the regression is caught by the expectation below.
static uint8_t vendor_out_buf[CFG_TUD_ENDPOINT0_SIZE];

bool tud_vendor_control_xfer_cb(uint8_t rhport_, uint8_t stage, tusb_control_request_t const* request) {
  (void) request;
  if (stage == CONTROL_STAGE_SETUP) {
    // Offer only an 8-byte capacity even though the data stage may receive a larger packet
    return tud_control_xfer(rhport_, request, vendor_out_buf, 8);
  }
  return true;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if ( !tud_inited() ) {
    tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };

    mscd_init_Expect();
    dcd_init_ExpectAndReturn(0, &dev_init, true);

    tusb_init(0, &dev_init);
  }
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Get Descriptor
//--------------------------------------------------------------------+

//------------- Device -------------//
void test_usbd_get_device_descriptor(void)
{
  // The expectation below is the fixture itself, so pin the one field a SuperSpeed-capable
  // build gets wrong: a USB 2.0 device descriptor must carry the full/high-speed EP0 size,
  // not a CFG_TUD_ENDPOINT0_SIZE of 512 truncated to 0 by this uint8_t field.
  TEST_ASSERT_EQUAL_UINT8(64, data_desc_device.bMaxPacketSize0);

  desc_device = (uint8_t const *) &data_desc_device;
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  // data
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*)&data_desc_device, sizeof(tusb_desc_device_t), sizeof(tusb_desc_device_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, sizeof(tusb_desc_device_t), 0, false);

  // status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_device, 1);

  tud_task();
}

void test_usbd_get_device_descriptor_null(void)
{
  desc_device = NULL;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

//------------- Configuration -------------//

void test_usbd_get_configuration_descriptor(void)
{
  desc_configuration = data_desc_configuration;
  uint16_t total_len = ((tusb_desc_configuration_t const*) data_desc_configuration)->wTotalLength;

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  // data
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*) data_desc_configuration, total_len, total_len, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, total_len, 0, false);

  // status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_configuration, 1);

  tud_task();
}

void test_usbd_get_configuration_descriptor_null(void)
{
  desc_configuration = NULL;
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

//--------------------------------------------------------------------+
// Control ZLP
//--------------------------------------------------------------------+

void test_usbd_control_in_zlp(void)
{
  // Total length = 2 * EP0 size with request length larger still: exactly two full transactions,
  // then a ZLP must be returned. Runs at SuperSpeed so the EP0 transaction size equals
  // CFG_TUD_ENDPOINT0_SIZE (at high/full speed usbd correctly chunks EP0 at 64, covered by
  // test_usbd_control_in_zlp in the FS/HS suite test_usbd_fshs.c).
  switch_to_superspeed();
  uint8_t zlp_desc_configuration[CFG_TUD_ENDPOINT0_SIZE*2] =
  {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, CFG_TUD_ENDPOINT0_SIZE*2, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  };

  desc_configuration = zlp_desc_configuration;

  tusb_control_request_t req = req_get_desc_configuration;
  req.wLength = CFG_TUD_ENDPOINT0_SIZE * 4; // larger than the data: the ZLP signals the end

  // request, then 1st, 2nd xact + ZLP + status
  dcd_event_setup_received(rhport, (uint8_t*) &req, false);

  // 1st transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration, CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDPOINT0_SIZE, 0, false);

  // 2nd transaction
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN,
                                         zlp_desc_configuration + CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, CFG_TUD_ENDPOINT0_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, CFG_TUD_ENDPOINT0_SIZE, 0, false);

  // Expect Zero length Packet
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);

  // Status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req, 1);

  tud_task();
}

static void switch_to_highspeed(void) {
  mscd_reset_Expect(0);
  dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, false);
  tud_task();
}

// A SuperSpeed-capable build (EP0 size 512) enumerated on a USB2 link must chunk EP0 at 64
// bytes (ep0_xact_limit's non-SUPER branch) - the USB2-fallback enumeration path.
void test_usbd_control_in_zlp_highspeed(void)
{
  switch_to_highspeed();
  uint8_t zlp_desc_configuration[128] =
  {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, 128, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  };
  desc_configuration = zlp_desc_configuration;

  // wLength 256 > 128 total: exactly two full 64-byte transactions, then a ZLP
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN, zlp_desc_configuration, 64, 64, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 64, 0, false);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN, zlp_desc_configuration + 64, 64, 64, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 64, 0, false);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_configuration, 1);

  tud_task();
}

//--------------------------------------------------------------------+
// SETUP dropped by full event queue
//--------------------------------------------------------------------+

// When the event queue is full, queue_event() drops the SETUP event. The queued-setup
// counter must not keep the dropped SETUP's increment: a leaked count makes the handler
// skip every later SETUP ("other SETUP in queue") forever, leaving EP0 permanently deaf.
void test_usbd_setup_dropped_by_full_queue_recovers(void)
{
  // fillers drain through usbd_reset -> class reset
  mscd_reset_Ignore();

  // fill the queue to the brim, then post one more SETUP: queue_event() drops it
  for (unsigned i = 0; i < CFG_TUD_TASK_QUEUE_SZ; i++) {
    dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, false);
  }
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  // drain all fillers (each tud_task pass handles at most CFG_TUD_TASK_EVENTS_PER_RUN
  // events); the dropped SETUP never arrives
  for (unsigned i = 0; i < (CFG_TUD_TASK_QUEUE_SZ / CFG_TUD_TASK_EVENTS_PER_RUN) + 1; i++) {
    tud_task();
  }

  // the next SETUP must still be answered
  desc_device = (uint8_t const*) &data_desc_device;
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_device, false);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, 0x80, (uint8_t*) &data_desc_device, sizeof(tusb_desc_device_t), sizeof(tusb_desc_device_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, sizeof(tusb_desc_device_t), 0, false);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_desc_device, 1);

  tud_task();
}

//--------------------------------------------------------------------+
// Transfer completion dropped by full event queue
//--------------------------------------------------------------------+

// When the event queue is full, queue_event() drops the XFER_COMPLETE event. The endpoint's
// busy/claimed state must not survive the dropped completion: a leaked BUSY makes every later
// usbd_edpt_claim()/usbd_edpt_xfer() on that endpoint fail, so the class never re-arms it.
void test_usbd_xfer_complete_dropped_by_full_queue_recovers(void)
{
  // fillers drain through usbd_reset -> class reset
  mscd_reset_Ignore();

  // open + claim + arm a bulk OUT endpoint the way a class driver would
  tusb_desc_endpoint_t desc_ep = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 64,
    .bInterval        = 0
  };
  static uint8_t xfer_buf[64];

  dcd_edpt_open_ExpectAndReturn(rhport, &desc_ep, true);
  TEST_ASSERT_TRUE(usbd_edpt_open(rhport, &desc_ep));
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, 0x01));
  dcd_edpt_xfer_ExpectAndReturn(rhport, 0x01, xfer_buf, 64, false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(rhport, 0x01, xfer_buf, 64, false));

  // fill the queue to the brim, then complete the transfer: queue_event() drops it
  for (unsigned i = 0; i < CFG_TUD_TASK_QUEUE_SZ; i++) {
    dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, false);
  }
  dcd_event_xfer_complete(rhport, 0x01, 64, XFER_RESULT_SUCCESS, false);

  // the endpoint must be re-armable: the dropped completion must not leak busy/claimed
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, 0x01));
  dcd_edpt_xfer_ExpectAndReturn(rhport, 0x01, xfer_buf, 64, false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(rhport, 0x01, xfer_buf, 64, false));

  // drain the fillers so later tests start from an empty queue
  for (unsigned i = 0; i < (CFG_TUD_TASK_QUEUE_SZ / CFG_TUD_TASK_EVENTS_PER_RUN) + 1; i++) {
    tud_task();
  }
}

//--------------------------------------------------------------------+
// Control OUT data stage host overrun
//--------------------------------------------------------------------+

// A non-compliant host sends an OUT data packet larger than the buffer the class offered:
// wLength = 8, but the DCD reports a full CFG_TUD_ENDPOINT0_SIZE packet. usbd must clamp the
// copy/accounting to the 8-byte capacity so total_xferred reaches wLength, ends the data stage,
// and queues the IN status stage. Without the clamp total_xferred overshoots wLength and usbd
// re-arms an OUT data packet (EDPT_CTRL_OUT) instead, failing the EDPT_CTRL_IN expectation below.
void test_usbd_control_out_overrun_clamp(void)
{
  dcd_event_setup_received(rhport, (uint8_t*) &req_vendor_out, false);

  // Data stage: usbd arms an 8-byte OUT into its internal bounce buffer (buffer ptr is internal)
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 8, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  // Host overrun: DCD reports a full max packet, larger than the 8-byte capacity
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, CFG_TUD_ENDPOINT0_SIZE, XFER_RESULT_SUCCESS, false);

  // Clamp -> total_xferred == wLength -> data stage done -> IN status stage queued
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_vendor_out, 1);

  tud_task();
}

//--------------------------------------------------------------------+
// SuperSpeed
//--------------------------------------------------------------------+

static void switch_to_superspeed(void) {
  mscd_reset_Expect(0);
  dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, false);
  tud_task();
}

void test_usbd_edpt_validate_superspeed(void)
{
  tusb_desc_endpoint_t desc_ep = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 1024,
    .bInterval        = 0
  };

  // SuperSpeed bulk must be exactly 1024
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));
  TEST_ASSERT_FALSE(tu_edpt_validate(&desc_ep, TUSB_SPEED_HIGH));

  desc_ep.wMaxPacketSize = 512;
  TEST_ASSERT_FALSE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_HIGH));

  // SuperSpeed interrupt and isochronous can be up to 1024, and no more: assert the rejecting
  // side too, otherwise dropping the size check entirely still passes this test
  desc_ep.bmAttributes.xfer = TUSB_XFER_INTERRUPT;
  desc_ep.wMaxPacketSize = 1024;
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));
  desc_ep.wMaxPacketSize = 1025;
  TEST_ASSERT_FALSE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));

  desc_ep.bmAttributes.xfer = TUSB_XFER_ISOCHRONOUS;
  desc_ep.wMaxPacketSize = 1024;
  TEST_ASSERT_TRUE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));
  desc_ep.wMaxPacketSize = 1025;
  TEST_ASSERT_FALSE(tu_edpt_validate(&desc_ep, TUSB_SPEED_SUPER));
}

// SuperSpeed configuration interleaves an endpoint companion descriptor after each
// endpoint descriptor; usbd_open_edpt_pair must skip them
void test_usbd_open_edpt_pair_ss_companion(void)
{
  switch_to_superspeed();

  uint8_t const desc_ep_pair[] = {
    // EP Out (bulk 1024) + companion
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_BULK, U16_TO_U8S_LE(1024), 0,
    TUD_SS_EP_COMP_DESCRIPTOR(0, 0, 0),
    // EP In (bulk 1024) + companion
    7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_BULK, U16_TO_U8S_LE(1024), 0,
    TUD_SS_EP_COMP_DESCRIPTOR(0, 0, 0),
  };

  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) &desc_ep_pair[0], true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) &desc_ep_pair[13], true);

  uint8_t ep_out = 0, ep_in = 0;
  TEST_ASSERT_TRUE(usbd_open_edpt_pair(rhport, desc_ep_pair, desc_ep_pair + sizeof(desc_ep_pair), 2, TUSB_XFER_BULK, &ep_out, &ep_in));
  TEST_ASSERT_EQUAL_HEX8(0x02, ep_out);
  TEST_ASSERT_EQUAL_HEX8(0x82, ep_in);
}

// SET_SEL (0x30): 6-byte OUT data stage, received and discarded
void test_usbd_set_sel(void)
{
  tusb_control_request_t const req_set_sel = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_SEL,
    .wValue = 0,
    .wIndex = 0,
    .wLength = 6
  };

  dcd_event_setup_received(rhport, (uint8_t*) &req_set_sel, false);

  // data stage into usbd's internal control buffer
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 6, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 6, XFER_RESULT_SUCCESS, false);

  // status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_set_sel, 1);

  tud_task();
}

// SET_ISOCH_DELAY (0x31): no data stage, just ACK
void test_usbd_set_isoch_delay(void)
{
  tusb_control_request_t const req_isoch_delay = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_ISOCH_DELAY,
    .wValue = 1000,
    .wIndex = 0,
    .wLength = 0
  };

  dcd_event_setup_received(rhport, (uint8_t*) &req_isoch_delay, false);

  // status only
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_isoch_delay, 1);

  tud_task();
}

// SET_SEL with a spoofed IN direction must stall: an IN request would otherwise transmit
// stale _ctrl_epbuf bytes to the host instead of receiving the 6-byte SEL payload.
void test_usbd_set_sel_malformed(void)
{
  tusb_control_request_t const req_set_sel_in = {
    .bmRequestType = 0x80, // IN (spoofed) - must be OUT
    .bRequest = TUSB_REQ_SET_SEL,
    .wValue = 0,
    .wIndex = 0,
    .wLength = 6
  };

  dcd_event_setup_received(rhport, (uint8_t*) &req_set_sel_in, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

// SET_ISOCH_DELAY carries no data stage; a non-zero wLength is malformed and must stall.
void test_usbd_set_isoch_delay_malformed(void)
{
  tusb_control_request_t const req_isoch_bad = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_ISOCH_DELAY,
    .wValue = 1000,
    .wIndex = 0,
    .wLength = 4 // must be 0
  };

  dcd_event_setup_received(rhport, (uint8_t*) &req_isoch_bad, false);

  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

// SET_CONFIGURATION(1): U1/U2_ENABLE is a Configured-state-only feature (USB 3.2 §9.4.9)
static void switch_to_configured(void) {
  desc_configuration = data_desc_configuration;
  tusb_control_request_t const req_set_config = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_CONFIGURATION,
    .wValue = 1,
    .wIndex = 0,
    .wLength = 0
  };
  dcd_event_setup_received(rhport, (uint8_t*) &req_set_config, false);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_set_config, 1);
  tud_task();
}

// SET_FEATURE(U1_ENABLE) outside the Configured state is a Request Error (USB 3.2 §9.4.9)
void test_usbd_set_feature_u1_not_configured(void)
{
  switch_to_superspeed();

  tusb_control_request_t const req_set_feat_u1 = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_FEATURE,
    .wValue = TUSB_REQ_FEATURE_U1_ENABLE,
    .wIndex = 0,
    .wLength = 0
  };
  dcd_event_setup_received(rhport, (uint8_t*) &req_set_feat_u1, false);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_OUT);
  dcd_edpt_stall_Expect(rhport, EDPT_CTRL_IN);

  tud_task();
}

// GET_STATUS(Device) must reflect U1 Enable (USB 3.2 bit 2) after SET_FEATURE(U1_ENABLE).
void test_usbd_get_status_u1_enable(void)
{
  // Bus reset to a known clean device state (clears dev_state_bm) and SuperSpeed operation
  switch_to_superspeed();
  switch_to_configured();

  // SET_FEATURE(U1_ENABLE): OUT, no data stage -> ACK with IN ZLP status
  tusb_control_request_t const req_set_feat_u1 = {
    .bmRequestType = 0x00,
    .bRequest = TUSB_REQ_SET_FEATURE,
    .wValue = TUSB_REQ_FEATURE_U1_ENABLE,
    .wIndex = 0,
    .wLength = 0
  };
  dcd_event_setup_received(rhport, (uint8_t*) &req_set_feat_u1, false);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_set_feat_u1, 1);
  tud_task();

  // GET_STATUS(Device): 2-byte IN data stage with bit 2 (U1 Enable) set
  tusb_control_request_t const req_get_status = {
    .bmRequestType = 0x80,
    .bRequest = TUSB_REQ_GET_STATUS,
    .wValue = 0,
    .wIndex = 0,
    .wLength = 2
  };
  uint8_t const expected_status[2] = { 0x04, 0x00 }; // little-endian: bit 2 = U1 Enable

  dcd_event_setup_received(rhport, (uint8_t*) &req_get_status, false);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_CTRL_IN, (uint8_t*) expected_status, 2, 2, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 2, 0, false);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_OUT, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_OUT, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_get_status, 1);

  tud_task();
}
