/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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


// xfer_isr() runs synchronously at completion time with the completion's real context, and returning false
// defers the same completion to xfer_cb() exactly once.

#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")

#include "mock_dcd.h"

uint32_t tusb_time_millis_api(void) {
  return 0;
}

enum {
  EDPT_OUT = 0x01,
  EDPT_IN  = 0x81,
};

static const uint8_t const_config[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN, 0, 100),
  TUD_VENDOR_DESCRIPTOR(0, 0, EDPT_OUT, EDPT_IN, 64),
};

uint8_t const *tud_descriptor_device_cb(void) {
  return NULL;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return const_config;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index;
  (void) langid;
  return NULL;
}

//--------------------------------------------------------------------+
// Application class driver recording how its completion hooks are called
//--------------------------------------------------------------------+
static unsigned isr_calls;
static bool     isr_in_isr;
static bool     isr_handles;
static unsigned cb_calls;

static void app_init(void) {
}

static void app_reset(uint8_t rhport) {
  (void) rhport;
}

static uint16_t app_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) max_len;
  const uint8_t *p_desc = tu_desc_next(itf_desc);
  uint8_t ep_out;
  uint8_t ep_in;
  TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &ep_out, &ep_in), 0);
  return TUD_VENDOR_DESC_LEN;
}

static bool app_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  (void) rhport;
  (void) stage;
  (void) request;
  return false;
}

static bool app_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) rhport;
  (void) ep_addr;
  (void) result;
  (void) xferred_bytes;
  cb_calls++;
  return true;
}

static bool app_xfer_isr(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes,
                         bool in_isr) {
  (void) rhport;
  (void) ep_addr;
  (void) result;
  (void) xferred_bytes;
  isr_calls++;
  isr_in_isr = in_isr;
  return isr_handles;
}

static const usbd_class_driver_t app_driver = {
  .name            = "APP",
  .init            = app_init,
  .deinit          = NULL,
  .reset           = app_reset,
  .open            = app_open,
  .control_xfer_cb = app_control_xfer_cb,
  .xfer_cb         = app_xfer_cb,
  .xfer_isr        = app_xfer_isr,
  .sof             = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = 1;
  return &app_driver;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
static const tusb_control_request_t req_set_configuration = {
  .bmRequestType = 0x00,
  .bRequest      = TUSB_REQ_SET_CONFIGURATION,
  .wValue        = 1,
  .wIndex        = 0,
  .wLength       = 0,
};

static uint8_t out_buf[64];

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    dcd_init_ExpectAndReturn(0, &dev_init, true);
    tusb_init(0, &dev_init);
  }

  dcd_event_bus_reset(0, TUSB_SPEED_FULL, false);
  tud_task();

  dcd_event_setup_received(0, (const uint8_t *) &req_set_configuration, false);
  dcd_edpt_open_IgnoreAndReturn(true);
  dcd_edpt_xfer_ExpectAndReturn(0, 0x80, NULL, 0, false, true);
  dcd_event_xfer_complete(0, 0x80, 0, XFER_RESULT_SUCCESS, false);
  dcd_edpt0_status_complete_ExpectWithArray(0, &req_set_configuration, 1);
  dcd_sof_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  tud_task();

  TEST_ASSERT_TRUE(usbd_edpt_claim(0, EDPT_OUT));
  dcd_edpt_xfer_ExpectAndReturn(0, EDPT_OUT, out_buf, sizeof(out_buf), false, true);
  TEST_ASSERT_TRUE(usbd_edpt_xfer(0, EDPT_OUT, out_buf, sizeof(out_buf), false));

  isr_calls   = 0;
  isr_in_isr  = false;
  isr_handles = true;
  cb_calls    = 0;
}

void tearDown(void) {
}

void test_xfer_isr_gets_isr_context(void) {
  dcd_event_xfer_complete(0, EDPT_OUT, 8, XFER_RESULT_SUCCESS, true);
  TEST_ASSERT_EQUAL(1, isr_calls);
  TEST_ASSERT_TRUE(isr_in_isr);

  tud_task();
  TEST_ASSERT_EQUAL(0, cb_calls);
}

// e.g. a DCD that completes a transfer synchronously inside dcd_edpt_xfer() called from a task
void test_xfer_isr_gets_task_context(void) {
  dcd_event_xfer_complete(0, EDPT_OUT, 8, XFER_RESULT_SUCCESS, false);
  TEST_ASSERT_EQUAL(1, isr_calls);
  TEST_ASSERT_FALSE(isr_in_isr);

  tud_task();
  TEST_ASSERT_EQUAL(0, cb_calls);
}

void test_xfer_isr_defers_to_xfer_cb_once(void) {
  isr_handles = false;
  dcd_event_xfer_complete(0, EDPT_OUT, 8, XFER_RESULT_SUCCESS, true);
  TEST_ASSERT_EQUAL(1, isr_calls);
  TEST_ASSERT_TRUE(usbd_edpt_busy(0, EDPT_OUT));

  tud_task();
  tud_task();
  TEST_ASSERT_EQUAL(1, isr_calls);
  TEST_ASSERT_EQUAL(1, cb_calls);
}
