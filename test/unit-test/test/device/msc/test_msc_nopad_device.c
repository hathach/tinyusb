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

// MSC device with CFG_TUD_MSC_PAD_SHORT_REPLY=0, the default at high speed. test_msc_device.c covers the
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

#include "msc_device_test.h"

TU_VERIFY_STATIC(CFG_TUD_MSC_PAD_SHORT_REPLY == 0, "this test covers the unpadded build");
TU_VERIFY_STATIC(TUD_OPT_HIGH_SPEED == 0, "this test runs at full speed: 64-byte Bulk-In (project.yml)");

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

// bytes tud_msc_read10_cb() leaves out of each chunk, for partial reads
static uint32_t read10_shortfall;

// lba and offset of the last tud_msc_read10_cb()
static uint32_t read10_lba;
static uint32_t read10_offset;

// fill data with the len bytes tud_msc_read10_cb() returns for lba at offset: a pattern of both, so a chunk sent from
// the wrong place or buffer shows on Bulk-In. The lba term is not a multiple of 256, so it survives the uint8_t cast
static void read10_pattern(uint8_t* data, uint32_t lba, uint32_t offset, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    data[i] = (uint8_t) (lba * 0x11 + offset + i);
  }
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void) lun;
  read10_lba    = lba;
  read10_offset = offset;

  uint32_t const nbytes = bufsize - read10_shortfall;
  read10_pattern((uint8_t*) buffer, lba, offset, nbytes);
  return (int32_t) nbytes;
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

  dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, false);
  tud_task();

  read10_shortfall = 0;
  read10_lba       = UINT32_MAX;
  read10_offset    = UINT32_MAX;
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
  msc_cbw_t const cbw = data_in_cbw(SCSI_CMD_MODE_SENSE_6, total_bytes);

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

  msc_csw_t const csw = csw_with_residue(total_bytes - sizeof(reply));

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

//--------------------------------------------------------------------+
// READ10 partial chunk, Bulk-Only Mass Storage Reset
//--------------------------------------------------------------------+
static msc_cbw_t const cbw_read10_2blocks = {
  .signature   = MSC_CBW_SIGNATURE,
  .tag         = 0xCAFECAFE,
  .total_bytes = 2 * 512,
  .lun         = 0,
  .dir         = TUSB_DIR_IN_MASK,
  .cmd_len     = sizeof(scsi_read10_t),
  .command     = { SCSI_CMD_READ_10, 0, 0, 0, 0, 0, 0, 0, 2 } // LBA 0, 2 blocks
};

// SET_CONFIGURATION, then the CBW of a 2-block READ10, whose first chunk is armed on Bulk-In with chunk_len bytes
static void read10_2blocks_first_chunk(uint16_t chunk_len) {
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(data_desc_configuration));
  dcd_event_setup_received(rhport, (uint8_t const*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) tu_desc_next(desc_ep), true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) &cbw_read10_2blocks, sizeof(msc_cbw_t));
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  // the chunk: chunk_len bytes from LBA 0 offset 0
  uint8_t expected[512];
  read10_pattern(expected, 0, 0, chunk_len);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, expected, chunk_len, chunk_len, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();
}

static void clear_halt(uint8_t ep_addr) {
  tusb_control_request_t const request_clear_halt = {
    .bmRequestType = 0x02,
    .bRequest      = TUSB_REQ_CLEAR_FEATURE,
    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
    .wIndex        = ep_addr,
    .wLength       = 0
  };

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, ep_addr);
}

// A partial chunk ends in a short packet, which ends the Data Stage for the host: Bulk-In is stalled instead of armed
// with the rest (#3188), and the CSW carries the residue once the host clears the halt
void test_msc_read10_short_chunk_stalls_bulk_in(void) {
  read10_shortfall = 1;
  read10_2blocks_first_chunk(512 - 1);

  // case 5 (Hi > Di)
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512 - 1, 0, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(512 + 1);
  clear_halt(EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// A partial chunk of whole packets (448 bytes, 7 full-speed packets) keeps the Data Stage open for the host: Bulk-In is
// not stalled but armed with the next chunk, read from where the partial one ended, and the CSW carries no residue
void test_msc_read10_packet_aligned_chunk_continues(void) {
  read10_shortfall = 64;
  read10_2blocks_first_chunk(512 - 64);
  TEST_ASSERT_EQUAL_UINT32(0, read10_lba);
  TEST_ASSERT_EQUAL_UINT32(0, read10_offset);

  // the next chunk: 512 bytes from LBA 0 offset 448, read whole this time
  uint8_t expected[512];
  read10_shortfall = 0;
  read10_pattern(expected, 0, 512 - 64, 512);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, expected, 512, 512, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512 - 64, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(0, read10_lba);
  TEST_ASSERT_EQUAL_UINT32(512 - 64, read10_offset);

  // the last 64 bytes: LBA 1 offset 448
  read10_pattern(expected, 1, 512 - 64, 64);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, expected, 64, 64, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, read10_lba);
  TEST_ASSERT_EQUAL_UINT32(512 - 64, read10_offset);

  // all 2 blocks sent: CSW with no residue, no stall
  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 64, 0, true);
  tud_task();
}

static tusb_control_request_t const request_bot_reset = {
  .bmRequestType = 0x21,
  .bRequest      = MSC_REQ_RESET,
  .wValue        = 0,
  .wIndex        = ITF_NUM_MSC,
  .wLength       = 0
};

static void expect_cbw_receive(void) {
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
}

// The clear-halts of Reset Recovery: the Bulk-Out one re-arms it for the next CBW
static void reset_recovery_clear_halts(void) {
  clear_halt(EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  clear_halt(EDPT_MSC_OUT);
  expect_cbw_receive();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// A BOT reset with a READ10 chunk still armed aborts that stale transfer on Bulk-In, here with a DCD whose
// dcd_edpt_abort_xfer() drops it without a stall, and arms the CBW receive: Reset Recovery's clear-halts keep an armed
// transfer, so the stale chunk would reach the host's next IN (#3188). Bulk-Out, idle since the CBW, is not touched,
// and nothing is stalled
void test_msc_bot_reset_aborts_armed_data_in(void) {
  read10_2blocks_first_chunk(512);

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_abort_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, true);
  expect_cbw_receive();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  reset_recovery_clear_halts();
}

// The same reset with a DCD without dcd_edpt_abort_xfer(): usbd_edpt_abort() drops the stale chunk with a stall
// cleared at once, on Bulk-In only, leaving nothing stalled. Bulk-Out is still not touched
void test_msc_bot_reset_aborts_armed_data_in_by_stall_without_dcd_abort(void) {
  read10_2blocks_first_chunk(512);

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_abort_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, false);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_clear_stall_Expect(rhport, EDPT_MSC_IN);
  expect_cbw_receive();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  reset_recovery_clear_halts();
}

// A BOT reset keeps a stall (BOT 3.1): Bulk-In stalled by the short chunk (case 5) is neither aborted nor cleared, and
// its clear-halt, now in the CMD stage, sends no CSW. Bulk-Out, idle since the CBW, is not aborted, and is armed for
// the next CBW
void test_msc_bot_reset_keeps_bulk_in_stall(void) {
  read10_shortfall = 1;
  read10_2blocks_first_chunk(512 - 1);

  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512 - 1, 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_cbw_receive();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  clear_halt(EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// SET_CONFIGURATION, with the first CBW receive armed to receive cbw
static void configure_msc(msc_cbw_t const* cbw) {
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(data_desc_configuration));
  dcd_event_setup_received(rhport, (uint8_t const*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const*) tu_desc_next(desc_ep), true);
  expect_cbw_receive();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) (uintptr_t) cbw, sizeof(msc_cbw_t));
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// A BOT reset while a WRITE10 Data-Out receive is still armed: that stale receive is aborted, so the host's next CBW
// is not taken as WRITE10 data, and the reset arms the CBW receive in its place. Bulk-In, idle, is not touched, and
// nothing is stalled
void test_msc_bot_reset_aborts_armed_data_out(void) {
  msc_cbw_t const cbw_write10 = {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xCAFECAFE,
    .total_bytes = 512,
    .lun         = 0,
    .dir         = 0,
    .cmd_len     = sizeof(scsi_write10_t),
    .command     = { SCSI_CMD_WRITE_10, 0, 0, 0, 0, 0, 0, 0, 1 } // LBA 0, 1 block
  };
  configure_msc(&cbw_write10);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, 512, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_abort_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, true);
  expect_cbw_receive();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  reset_recovery_clear_halts();
}

// A BOT reset between commands, with the CBW receive armed again after the last CSW: neither endpoint holds a stale
// transfer, so neither is aborted, stalled or cleared, keeping both data toggles (BOT 3.1), and the CBW receive is not
// armed a second time. The host's next CBW lands in the receive still armed, with no clear-halts needed
void test_msc_bot_reset_idle_keeps_cbw_receive(void) {
  msc_cbw_t const cbw_tur = {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xCAFECAFE,
    .total_bytes = 0,
    .lun         = 0,
    .dir         = 0,
    .cmd_len     = sizeof(scsi_test_unit_ready_t),
    .command     = { SCSI_CMD_TEST_UNIT_READY }
  };
  static msc_cbw_t cbw_next;
  cbw_next = cbw_tur;
  cbw_next.tag = 0x12345678;

  configure_msc(&cbw_tur);

  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  // the CSW is sent: the CBW receive is armed again, and the host's next CBW will land in it
  expect_cbw_receive();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) &cbw_next, sizeof(msc_cbw_t));
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(msc_csw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  msc_csw_t csw_next = csw_with_residue(0);
  csw_next.tag = cbw_next.tag;
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw_next, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();
}
