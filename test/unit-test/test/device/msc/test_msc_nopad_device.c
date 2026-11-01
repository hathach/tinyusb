/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, hathach (tinyusb.org)
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

// MSC device with CFG_TUD_MSC_PAD_SHORT_REPLY=0, the default on every MCU but RP2040. test_msc_device.c covers the
// padded build; the file name must not contain "test_msc_device", or project.yml's matcher for it applies here too.

#include "unity.h"

// Files to test
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("msc_device.c")

// Mock File
#include "mock_dcd.h"

TU_VERIFY_STATIC(CFG_TUD_MSC_PAD_SHORT_REPLY == 0, "this test covers the unpadded build");

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
uint32_t tusb_time_millis_api(void) {
  return 0;
}

enum {
  EDPT_CTRL_IN = 0x80,
  EDPT_MSC_OUT = 0x01,
  EDPT_MSC_IN  = 0x81,
};

uint8_t const rhport = 0;

enum {
  ITF_NUM_MSC,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

uint8_t const data_desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

tusb_control_request_t const request_set_configuration = {
  .bmRequestType = 0x00,
  .bRequest      = TUSB_REQ_SET_CONFIGURATION,
  .wValue        = 1,
  .wIndex        = 0,
  .wLength       = 0
};

//--------------------------------------------------------------------+
// Application callbacks
//--------------------------------------------------------------------+
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void) lun;
  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  (void) lun;
  *block_count = 16;
  *block_size  = 512;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void) lun; (void) lba; (void) offset; (void) buffer;
  return (int32_t) bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  (void) lun; (void) lba; (void) offset; (void) buffer;
  return (int32_t) bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
  return 0;
}

uint8_t const* tud_descriptor_device_cb(void) {
  return NULL;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return data_desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index; (void) langid;
  return NULL;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt0_setup_begin_IgnoreAndReturn(true);

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = {
      .role  = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };

    dcd_init_ExpectAndReturn(0, &dev_init, true);
    tusb_init(0, &dev_init);
  }

  dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, false);
  tud_task();
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Data-In reply shorter than the host's length: BOT 6.7.2 case 5 (Hi > Di)
//--------------------------------------------------------------------+
// The 4-byte MODE SENSE(6) reply fits the endpoint buffer but is not padded: it goes out short, Bulk-In is stalled,
// and the CSW carries the residue after the host clears the halt
void test_msc_short_reply_stalls_without_padding(void) {
  uint32_t const total_bytes = 192;
  msc_cbw_t const cbw = {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xCAFECAFE,
    .total_bytes = total_bytes,
    .lun         = 0,
    .dir         = TUSB_DIR_IN_MASK,
    .cmd_len     = 6,
    .command     = { SCSI_CMD_MODE_SENSE_6, 0, 0x3F, 0, (uint8_t) total_bytes, 0 }
  };

  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(data_desc_configuration));
  dcd_event_setup_received(rhport, (uint8_t const*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) tu_desc_next(desc_ep), true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) &cbw, sizeof(msc_cbw_t));
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  uint8_t const reply[4] = { 3, 0, 0, 0 }; // mode data length 3, writable, no block descriptor
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) reply, sizeof(reply), sizeof(reply), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(reply), 0, true);
  tud_task();

  tusb_control_request_t const request_clear_halt = {
    .bmRequestType = 0x02,
    .bRequest      = TUSB_REQ_CLEAR_FEATURE,
    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
    .wIndex        = EDPT_MSC_IN,
    .wLength       = 0
  };

  msc_csw_t const csw = {
    .signature    = MSC_CSW_SIGNATURE,
    .tag          = 0xCAFECAFE,
    .data_residue = total_bytes - sizeof(reply),
    .status       = MSC_CSW_STATUS_PASSED
  };

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}
