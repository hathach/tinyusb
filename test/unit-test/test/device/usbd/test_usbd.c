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

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

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
  // 128 byte total len, with EP0 size = 64, and request length = 256
  // ZLP must be return
  uint8_t zlp_desc_configuration[CFG_TUD_ENDPOINT0_SIZE*2] =
  {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, CFG_TUD_ENDPOINT0_SIZE*2, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  };

  desc_configuration = zlp_desc_configuration;

  // request, then 1st, 2nd xact + ZLP + status
  dcd_event_setup_received(rhport, (uint8_t*) &req_get_desc_configuration, false);

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
// OUT endpoint held by RX_PENDING until its buffer is consumed (#1292)
//--------------------------------------------------------------------+
enum {
  EDPT_MSC_OUT = 0x01,
  EDPT_MSC_IN  = 0x81
};

uint8_t const msc_desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN, 0, 100),
  TUD_MSC_DESCRIPTOR(0, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

tusb_control_request_t const req_set_configuration = {
  .bmRequestType = 0x00,
  .bRequest      = TUSB_REQ_SET_CONFIGURATION,
  .wValue        = 1,
  .wIndex        = 0,
  .wLength       = 0
};

static uint8_t msc_out_buf[64];

// Bind the bulk endpoints to the (mocked) MSC driver, then claim and arm the OUT endpoint
static void msc_out_armed(void) {
  mscd_reset_Ignore();
  dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, false);
  tud_task();

  desc_configuration = msc_desc_configuration;
  dcd_event_setup_received(rhport, (uint8_t*) &req_set_configuration, false);
  mscd_open_ExpectAndReturn(rhport, (tusb_desc_interface_t const*) (msc_desc_configuration + TUD_CONFIG_DESC_LEN),
                            TUD_MSC_DESC_LEN, TUD_MSC_DESC_LEN);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  dcd_event_xfer_complete(rhport, EDPT_CTRL_IN, 0, 0, false);
  dcd_edpt0_status_complete_ExpectWithArray(rhport, &req_set_configuration, 1);
  tud_task();

  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, msc_out_buf, sizeof(msc_out_buf), false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(rhport, EDPT_MSC_OUT, msc_out_buf, sizeof(msc_out_buf), false));
}

static void msc_out_complete(CMOCK_mscd_xfer_cb_CALLBACK xfer_cb) {
  mscd_xfer_cb_Stub(xfer_cb);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_out_buf), XFER_RESULT_SUCCESS, false);
  tud_task();
}

static bool xfer_cb_claim_after_consume(uint8_t rhport_, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                                        int num_calls) {
  (void) result; (void) xferred_bytes; (void) num_calls;
  TEST_ASSERT_FALSE(usbd_edpt_busy(rhport_, ep_addr));
  TEST_ASSERT_FALSE(usbd_edpt_claim(rhport_, ep_addr)); // buffer not yet consumed

  usbd_edpt_rx_consume(rhport_, ep_addr);
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport_, ep_addr));
  TEST_ASSERT_TRUE(usbd_edpt_release(rhport_, ep_addr));
  return true;
}

void test_usbd_out_complete_refuses_claim_until_consumed(void) {
  msc_out_armed();
  msc_out_complete(xfer_cb_claim_after_consume);
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
}

static bool xfer_cb_no_consume(uint8_t rhport_, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                               int num_calls) {
  (void) result; (void) xferred_bytes; (void) num_calls;
  TEST_ASSERT_FALSE(usbd_edpt_claim(rhport_, ep_addr));
  return true;
}

// xfer_cb that neither consumes nor re-arms: the hold outlives xfer_cb until the class consumes
void test_usbd_out_complete_held_after_xfer_cb_until_consumed(void) {
  msc_out_armed();
  msc_out_complete(xfer_cb_no_consume);
  TEST_ASSERT_FALSE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
  usbd_edpt_rx_consume(rhport, EDPT_MSC_OUT);
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
}

static bool xfer_cb_consume_rearm(uint8_t rhport_, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                                  int num_calls) {
  (void) result; (void) xferred_bytes; (void) num_calls;
  usbd_edpt_rx_consume(rhport_, ep_addr);
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport_, ep_addr));
  dcd_edpt_xfer_ExpectAndReturn(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false));
  return true;
}

// a transfer re-armed inside xfer_cb stays BUSY and is not held by RX_PENDING
void test_usbd_out_rearmed_in_xfer_cb_stays_busy(void) {
  msc_out_armed();
  msc_out_complete(xfer_cb_consume_rearm);
  TEST_ASSERT_TRUE(usbd_edpt_busy(rhport, EDPT_MSC_OUT));
  TEST_ASSERT_FALSE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
}

static bool xfer_cb_rearm_refused(uint8_t rhport_, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                                  int num_calls) {
  (void) result; (void) xferred_bytes; (void) num_calls;
  // direct re-arm without claim, as MSC does; the DCD refuses it
  dcd_edpt_xfer_ExpectAndReturn(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false, false);
  TEST_ASSERT_FALSE(usbd_edpt_xfer(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false));
  return true;
}

// a failed re-arm leaves the endpoint idle: neither BUSY nor still held by RX_PENDING
void test_usbd_out_failed_rearm_in_xfer_cb_stays_idle(void) {
  msc_out_armed();
  msc_out_complete(xfer_cb_rearm_refused);
  TEST_ASSERT_FALSE(usbd_edpt_busy(rhport, EDPT_MSC_OUT));
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
}

static bool xfer_cb_rearm_dropped(uint8_t rhport_, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                                  int num_calls) {
  (void) result; (void) xferred_bytes; (void) num_calls;
  dcd_edpt_xfer_ExpectAndReturn(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(rhport_, ep_addr, msc_out_buf, sizeof(msc_out_buf), false));

  // fill the queue with events that leave endpoints alone, so the re-armed completion is dropped
  for (unsigned i = 0; i < CFG_TUD_TASK_QUEUE_SZ; i++) {
    dcd_event_bus_signal(rhport_, DCD_EVENT_SUSPEND, false);
  }
  dcd_event_xfer_complete(rhport_, ep_addr, sizeof(msc_out_buf), XFER_RESULT_SUCCESS, false);
  return true;
}

// a re-armed transfer whose completion is dropped leaves the endpoint idle
void test_usbd_out_dropped_rearm_in_xfer_cb_stays_idle(void) {
  msc_out_armed();
  msc_out_complete(xfer_cb_rearm_dropped);
  for (unsigned i = 0; i < (CFG_TUD_TASK_QUEUE_SZ / CFG_TUD_TASK_EVENTS_PER_RUN) + 1; i++) {
    tud_task();
  }
  TEST_ASSERT_FALSE(usbd_edpt_busy(rhport, EDPT_MSC_OUT));
  TEST_ASSERT_TRUE(usbd_edpt_claim(rhport, EDPT_MSC_OUT));
}

//--------------------------------------------------------------------+
// Endpoint stream ZLP
//--------------------------------------------------------------------+

// A host read asking for more than the stream has sent only ends on a short packet, so a
// stream whose last transfer was a non-zero multiple of mps must follow it with a ZLP. With a
// one-packet ep buffer every transfer is at most mps, so the condition must not exclude mps
// itself: excluding it leaves such a read waiting after every 64-byte write.
void test_usbd_stream_write_zlp_after_full_packet(void)
{
  uint8_t ff_buf[64];
  uint8_t ep_buf[64];
  tu_edpt_stream_t stream;
  tusb_desc_endpoint_t desc_ep = {
    .bLength          = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType  = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x82,
    .bmAttributes     = { .xfer = TUSB_XFER_BULK },
    .wMaxPacketSize   = 64,
    .bInterval        = 0
  };

  TEST_ASSERT_TRUE(tu_edpt_stream_init(&stream, false, true, false, ff_buf, sizeof(ff_buf), ep_buf));
  tu_edpt_stream_open(&stream, rhport, &desc_ep, sizeof(ep_buf));

  // nothing sent, or a short last packet: the host already saw the end of the transfer
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, 0));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, 63));

  // data still pending: the next data transfer terminates it, not a ZLP
  TEST_ASSERT_EQUAL(1, tu_edpt_stream_write(&stream, "x", 1));
  TEST_ASSERT_FALSE(tu_edpt_stream_write_zlp_if_needed(&stream, 64));
  tu_fifo_clear(&stream.ff);

  // one full packet and nothing left to send
  dcd_edpt_xfer_ExpectAndReturn(rhport, 0x82, NULL, 0, false, true);
  TEST_ASSERT_TRUE(tu_edpt_stream_write_zlp_if_needed(&stream, 64));
}
