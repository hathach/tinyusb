/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// The example is compiled into this test so its private storage (fs_buf) is checkable: a
// SendObject that overruns the declared object size must not write past it, which is
// invisible over USB.

#include <string.h>
#include "unity.h"

// only the example includes tusb.h: a direct include here would make Ceedling link tusb.c
#include "../../../../examples/device/mtp/src/mtp_fs_example.c"

//--------------------------------------------------------------------+
// Driver API stubs: record what the example asked for
//--------------------------------------------------------------------+
static struct {
  uint32_t data_receive, data_send, response_send;
  uint16_t resp_code;
} api;

bool tud_mtp_data_receive(mtp_container_info_t* p_container) { (void) p_container; api.data_receive++; return true; }
bool tud_mtp_data_send(mtp_container_info_t* p_container) { (void) p_container; api.data_send++; return true; }
bool tud_mtp_response_send(mtp_container_info_t* p_container) {
  api.response_send++;
  api.resp_code = p_container->header->code;
  return true;
}
bool tud_mtp_event_send(mtp_event_t* event) { (void) event; return true; }
bool tud_mtp_mounted(void) { return true; }
size_t board_get_unique_id(uint8_t id[], size_t max_len) { (void) id; (void) max_len; return 0; }

//--------------------------------------------------------------------+
// Fixture
//--------------------------------------------------------------------+
enum { HDR = sizeof(mtp_container_header_t), BUFSIZE = CFG_TUD_MTP_EP_BUFSIZE, SENTINEL = 0xEE };

static mtp_container_command_t command;
static uint8_t epbuf[BUFSIZE];
static mtp_container_header_t io_header; // the driver's saved header for 2nd+ packets
static tud_mtp_cb_data_t cb;

static void begin_command(uint16_t code, uint32_t p0) {
  memset(&command, 0, sizeof(command));
  command.header = (mtp_container_header_t){ .len = HDR + 4, .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = code, .transaction_id = 1 };
  command.params[0] = p0;
  memset(epbuf, 0, sizeof(epbuf));
  cb = (tud_mtp_cb_data_t){
    .phase = MTP_PHASE_COMMAND,
    .session_id = 1,
    .command_container = &command,
    .io_container = { .header = (mtp_container_header_t*) epbuf, .payload = epbuf + HDR, .payload_bytes = BUFSIZE - HDR },
    .xfer_result = XFER_RESULT_SUCCESS,
  };
  cb.io_container.header->len = HDR;
  memset(&api, 0, sizeof(api));
  TEST_ASSERT_GREATER_OR_EQUAL(0, tud_mtp_command_received_cb(&cb)); // the example returns the response code
}

// deliver one data-OUT packet the way mtpd_xfer_cb() does: the 1st carries the header, later
// ones are the headerless view whose payload starts at the top of the endpoint buffer
static void deliver_out(const uint8_t* payload, uint32_t payload_bytes, uint32_t declared_payload) {
  cb.phase = MTP_PHASE_DATA;
  if (cb.total_xferred_bytes == 0) {
    io_header = (mtp_container_header_t){ .len = HDR + declared_payload, .type = MTP_CONTAINER_TYPE_DATA_BLOCK, .code = command.header.code, .transaction_id = 1 };
    *(mtp_container_header_t*) epbuf = io_header;
    memcpy(epbuf + HDR, payload, payload_bytes);
    cb.io_container = (mtp_container_info_t){ .header = (mtp_container_header_t*) epbuf, .payload = epbuf + HDR, .payload_bytes = payload_bytes };
    cb.total_xferred_bytes = HDR + payload_bytes;
  } else {
    memcpy(epbuf, payload, payload_bytes);
    cb.io_container = (mtp_container_info_t){ .header = &io_header, .payload = epbuf, .payload_bytes = payload_bytes };
    cb.total_xferred_bytes += payload_bytes;
  }
  TEST_ASSERT_EQUAL(0, tud_mtp_data_xfer_cb(&cb));
}

static void data_complete(void) {
  cb.phase = MTP_PHASE_DATA_COMPLETE;
  cb.io_container = (mtp_container_info_t){ .header = (mtp_container_header_t*) epbuf, .payload = epbuf + HDR, .payload_bytes = BUFSIZE - HDR };
  cb.io_container.header->len = HDR;
  TEST_ASSERT_EQUAL(0, tud_mtp_data_complete_cb(&cb));
}

static void open_session(void) {
  begin_command(MTP_OP_OPEN_SESSION, 1);
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_OK, api.resp_code);
}

// SendObjectInfo declaring `size` bytes; returns the new handle
static uint32_t send_object_info(uint32_t size) {
  begin_command(MTP_OP_SEND_OBJECT_INFO, SUPPORTED_STORAGE_ID);
  TEST_ASSERT_EQUAL(1, api.data_receive);
  uint8_t dataset[sizeof(mtp_object_info_header_t) + 1 + 2 * 8] = { 0 };
  mtp_object_info_header_t* oi = (mtp_object_info_header_t*) dataset;
  oi->storage_id = SUPPORTED_STORAGE_ID;
  oi->object_format = MTP_OBJ_FORMAT_TEXT;
  oi->object_compressed_size = size;
  oi->parent_object = 0xFFFFFFFFu;
  uint8_t* name = dataset + sizeof(mtp_object_info_header_t);
  name[0] = 4;
  const uint16_t utf16[4] = { 'a', '.', 't', 0 };
  memcpy(name + 1, utf16, sizeof(utf16));
  deliver_out(dataset, sizeof(dataset), sizeof(dataset));
  data_complete();
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_OK, api.resp_code);
  return send_obj_handle;
}

static fs_file_t fs_objects_initial[FS_MAX_FILE_COUNT];
static bool fs_objects_saved;

void setUp(void) {
  memset(&api, 0, sizeof(api));
  is_session_opened = false;
  send_obj_handle = 0;
  memset(fs_buf, SENTINEL, sizeof(fs_buf));
  // the example's file table as built: frees the one RAM slot a previous test created a file in
  if (!fs_objects_saved) {
    memcpy(fs_objects_initial, fs_objects, sizeof(fs_objects));
    fs_objects_saved = true;
  }
  memcpy(fs_objects, fs_objects_initial, sizeof(fs_objects));
}
void tearDown(void) {}

static void check_sentinels_from(uint32_t from) {
  for (uint32_t i = from; i < sizeof(fs_buf); i++) {
    if (fs_buf[i] != SENTINEL) {
      TEST_FAIL_MESSAGE("write past the declared object size");
    }
  }
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// the 1st packet crosses the end of a 100-byte object, the 2nd is entirely past it
void test_send_object_excess_is_discarded(void) {
  open_session();
  const uint32_t handle = send_object_info(100);
  TEST_ASSERT_NOT_EQUAL(0, handle);
  uint8_t pkt[BUFSIZE];
  for (uint32_t i = 0; i < BUFSIZE; i++) pkt[i] = (uint8_t) (i + 1);

  begin_command(MTP_OP_SEND_OBJECT, 0);
  TEST_ASSERT_EQUAL(1, api.data_receive);
  deliver_out(pkt, BUFSIZE - HDR, 2 * BUFSIZE - HDR); // host declares 1012 bytes
  TEST_ASSERT_EQUAL(2, api.data_receive);               // keeps draining
  TEST_ASSERT_EQUAL_MEMORY(pkt, fs_buf, 100);
  check_sentinels_from(100);

  deliver_out(pkt, BUFSIZE, 0); // wholly excess
  TEST_ASSERT_EQUAL(2, api.data_receive); // declared length reached
  TEST_ASSERT_EQUAL_MEMORY(pkt, fs_buf, 100);
  check_sentinels_from(100);

  data_complete();
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_OK, api.resp_code);
  TEST_ASSERT_EQUAL(100, fs_get_file(handle)->size);
}

// a packet whose end lands inside the object: only the bytes up to the size are written
void test_send_object_packet_crossing_end_is_clamped(void) {
  open_session();
  send_object_info(700);
  uint8_t pkt[BUFSIZE];
  for (uint32_t i = 0; i < BUFSIZE; i++) pkt[i] = (uint8_t) (i + 1);

  begin_command(MTP_OP_SEND_OBJECT, 0);
  deliver_out(pkt, BUFSIZE - HDR, 3 * BUFSIZE - HDR); // host declares 1524
  deliver_out(pkt, BUFSIZE, 0);                        // bytes 500..1011: crosses 700
  TEST_ASSERT_EQUAL_MEMORY(pkt, fs_buf, 500);
  TEST_ASSERT_EQUAL_MEMORY(pkt, fs_buf + 500, 200);
  check_sentinels_from(700);
  deliver_out(pkt, BUFSIZE, 0);                        // wholly excess
  check_sentinels_from(700);
  TEST_ASSERT_EQUAL(3, api.data_receive);
}

void test_send_object_without_session_is_refused(void) {
  begin_command(MTP_OP_SEND_OBJECT, 0);
  TEST_ASSERT_EQUAL(0, api.data_receive);
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_SESSION_NOT_OPEN, api.resp_code);
  check_sentinels_from(0);
}

void test_cancel_drops_the_staged_handle_but_keeps_the_session(void) {
  open_session();
  send_object_info(100);
  tud_mtp_request_cb_data_t req = { .buf = (uint8_t*) &command };
  TEST_ASSERT_TRUE(tud_mtp_request_cancel_cb(&req));
  TEST_ASSERT_TRUE(is_session_opened);
  begin_command(MTP_OP_SEND_OBJECT, 0);
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_INVALID_OBJECT_HANDLE, api.resp_code);
}

void test_device_reset_closes_the_session(void) {
  open_session();
  send_object_info(100);
  tud_mtp_request_cb_data_t req = { 0 };
  TEST_ASSERT_TRUE(tud_mtp_request_device_reset_cb(&req));
  TEST_ASSERT_FALSE(is_session_opened);
  TEST_ASSERT_EQUAL(0, send_obj_handle);
}
