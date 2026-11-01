/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, hathach (tinyusb.org)
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

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint32_t tusb_time_millis_api(void) {
  return 0;
}

//--------------------------------------------------------------------+
// SystemView stubs: an open span stack checks that every CALL is closed by its own RET
//--------------------------------------------------------------------+
static unsigned sv_open[8];
static unsigned sv_depth;
static unsigned sv_calls[TU_SV_ID_COUNT];

void SEGGER_SYSVIEW_RecordVoid(unsigned int Id) {
  TEST_ASSERT_LESS_THAN_UINT(TU_ARRAY_SIZE(sv_open), sv_depth);
  sv_open[sv_depth++] = Id;
  sv_calls[Id - TU_SV_EVENT_BASE]++;
}

void SEGGER_SYSVIEW_RecordEndCall(unsigned int Id) {
  TEST_ASSERT_NOT_EQUAL_UINT(0, sv_depth);
  TEST_ASSERT_EQUAL_UINT(sv_open[--sv_depth], Id);
}

void SEGGER_SYSVIEW_RecordEnterISR(void) {}
void SEGGER_SYSVIEW_RecordExitISR(void) {}
void SEGGER_SYSVIEW_Init(U32 SysFreq, U32 CPUFreq, const SEGGER_SYSVIEW_OS_API *pOSAPI,
                          SEGGER_SYSVIEW_SEND_SYS_DESC_FUNC pfSendSysDesc) {
  (void) SysFreq; (void) CPUFreq; (void) pOSAPI; (void) pfSendSysDesc;
}
void SEGGER_SYSVIEW_SetRAMBase(U32 RAMBaseAddress) { (void) RAMBaseAddress; }
void SEGGER_SYSVIEW_Start(void) {}
void SEGGER_SYSVIEW_DisableEvents(U32 DisableMask) { (void) DisableMask; }
void SEGGER_SYSVIEW_SendSysDesc(const char *sSysDesc) { (void) sSysDesc; }
void SEGGER_SYSVIEW_NameResource(U32 ResourceId, const char *sName) { (void) ResourceId; (void) sName; }
uint32_t SystemCoreClock = 48000000;

enum
{
  EDPT_CTRL_OUT = 0x00,
  EDPT_CTRL_IN  = 0x80,

  EDPT_MSC_OUT  = 0x01,
  EDPT_MSC_IN   = 0x81,
};

uint8_t const rhport = 0;

enum
{
  ITF_NUM_MSC,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

uint8_t const data_desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

tusb_control_request_t const request_set_configuration =
{
  .bmRequestType = 0x00,
  .bRequest      = TUSB_REQ_SET_CONFIGURATION,
  .wValue        = 1,
  .wIndex        = 0,
  .wLength       = 0
};

uint8_t const* desc_configuration;


enum
{
  DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];

// Invoked when received SCSI_CMD_INQUIRY, v2 with full inquiry response
// Some inquiry_resp's fields are already filled with default values, application can update them
// Return length of inquiry response, typically sizeof(scsi_inquiry_resp_t) (36 bytes), can be longer if included vendor data.
uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t* inquiry_resp, uint32_t bufsize) {
  (void) lun;
  (void) bufsize;
  const char vid[] = "TinyUSB";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";

  memcpy(inquiry_resp->vendor_id, vid, strlen(vid));
  memcpy(inquiry_resp->product_id, pid, strlen(pid));
  memcpy(inquiry_resp->product_rev, rev, strlen(rev));

  return sizeof(scsi_inquiry_resp_t); // 36 bytes
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void) lun;

  return true; // RAM disk is always ready
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void) lun;

  *block_count = DISK_BLOCK_NUM;
  *block_size  = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void) lun;
  (void) power_condition;

  return true;
}

// bytes tud_msc_read10_cb() leaves out of each chunk, for partial reads
static uint32_t read10_shortfall;

// tud_msc_read10_cb() answers TUD_MSC_RET_ASYNC, for a background read completed by tud_msc_async_io_done()
static bool read10_async;

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  if (read10_async) {
    return TUD_MSC_RET_ASYNC;
  }

  bufsize -= read10_shortfall;
  uint8_t const* addr = msc_disk[lba] + offset;
  memcpy(buffer, addr, bufsize);

  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;

  uint8_t* addr = msc_disk[lba] + offset;
  memcpy(addr, buffer, bufsize);

  return bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  uint16_t resplen = 0;

  return resplen;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
uint8_t const * tud_descriptor_device_cb(void)
{
  return NULL;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  return NULL;
}

void setUp(void)
{
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt0_setup_begin_IgnoreAndReturn(true);

  if ( !tud_inited() ) {
    tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };

    dcd_init_ExpectAndReturn(0, &dev_init, true);
    tusb_init(0, &dev_init);
  }

  dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, false);
  tud_task();

  memset(sv_calls, 0, sizeof(sv_calls));
  read10_shortfall = 0;
  read10_async = false;
}

void tearDown(void)
{
  TEST_ASSERT_EQUAL_UINT(0, sv_depth);
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void test_msc(void)
{
  // Read 1 LBA = 0, Block count = 1
  msc_cbw_t cbw_read10 =
  {
    .signature = MSC_CBW_SIGNATURE,
    .tag = 0xCAFECAFE,
    .total_bytes = 512,
    .lun = 0,
    .dir = TUSB_DIR_IN_MASK,
    .cmd_len = sizeof(scsi_read10_t)
  };

  scsi_read10_t cmd_read10 =
  {
      .cmd_code    = SCSI_CMD_READ_10,
      .lba         = tu_htonl(0),
      .block_count = tu_htons(1)
  };

  memcpy(cbw_read10.command, &cmd_read10, cbw_read10.cmd_len);

  desc_configuration = data_desc_configuration;
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(desc_configuration));

  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);

  // open endpoints
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep), true);

  // Prepare SCSI command
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer( (uint8_t*) &cbw_read10, sizeof(msc_cbw_t));

  // command received
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);

  // control status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);

  // SCSI Data transfer
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, 512, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512, 0, true); // complete

  // SCSI Status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, 13, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 13, 0, true);

  // Prepare for next command
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();

  tud_task();
}

// A block size computed from the CBW that does not fit 16 bits must fail, not wrap (66048 / 1 would read as 512)
void test_msc_read10_block_size_over_16bit(void)
{
  uint32_t const total_bytes = 65536 + 512;

  msc_cbw_t cbw_read10 =
  {
    .signature = MSC_CBW_SIGNATURE,
    .tag = 0xCAFECAFE,
    .total_bytes = total_bytes,
    .lun = 0,
    .dir = TUSB_DIR_IN_MASK,
    .cmd_len = sizeof(scsi_read10_t)
  };

  scsi_read10_t cmd_read10 =
  {
      .cmd_code    = SCSI_CMD_READ_10,
      .lba         = tu_htonl(0),
      .block_count = tu_htons(1)
  };

  memcpy(cbw_read10.command, &cmd_read10, cbw_read10.cmd_len);

  desc_configuration = data_desc_configuration;
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(desc_configuration));

  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);

  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep), true);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer( (uint8_t*) &cbw_read10, sizeof(msc_cbw_t));

  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);

  // case 4 (Hi > Dn): no data, STALL Bulk-In, then a failed CSW with full residue after the host clears it
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);

  tud_task();

  tusb_control_request_t const request_clear_halt =
  {
    .bmRequestType = 0x02,
    .bRequest      = TUSB_REQ_CLEAR_FEATURE,
    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
    .wIndex        = EDPT_MSC_IN,
    .wLength       = 0
  };

  msc_csw_t const csw =
  {
    .signature    = MSC_CSW_SIGNATURE,
    .tag          = 0xCAFECAFE,
    .data_residue = total_bytes,
    .status       = MSC_CSW_STATUS_FAILED
  };

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);

  tud_task();
}

//--------------------------------------------------------------------+
// READ10 Data Stage split in chunks
//--------------------------------------------------------------------+
static msc_cbw_t const cbw_read10_2blocks =
{
  .signature   = MSC_CBW_SIGNATURE,
  .tag         = 0xCAFECAFE,
  .total_bytes = 2 * DISK_BLOCK_SIZE,
  .lun         = 0,
  .dir         = TUSB_DIR_IN_MASK,
  .cmd_len     = sizeof(scsi_read10_t),
  .command     = { SCSI_CMD_READ_10, 0, 0, 0, 0, 0, 0, 0, 2 } // LBA 0, 2 blocks
};

// SET_CONFIGURATION, then the CBW of a 2-block READ10
static void read10_2blocks_received(void) {
  desc_configuration = data_desc_configuration;
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(desc_configuration));

  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep), true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) &cbw_read10_2blocks, sizeof(msc_cbw_t));
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
}

// A whole chunk ends in a full packet: the Data Stage goes on with the next chunk
void test_msc_read10_full_chunk_continues(void)
{
  read10_2blocks_received();

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();

  tud_task();
}

// A partial chunk ends in a short packet, which ends the Data Stage for the host: Bulk-In is stalled instead of armed
// with the rest (#3188), and the CSW carries the residue once the host clears the halt
void test_msc_read10_short_chunk_stalls_bulk_in(void)
{
  read10_shortfall = 1;
  read10_2blocks_received();

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, DISK_BLOCK_SIZE - 1, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE - 1, 0, true);

  // case 5 (Hi > Di)
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);

  tud_task();

  tusb_control_request_t const request_clear_halt =
  {
    .bmRequestType = 0x02,
    .bRequest      = TUSB_REQ_CLEAR_FEATURE,
    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
    .wIndex        = EDPT_MSC_IN,
    .wLength       = 0
  };

  msc_csw_t const csw =
  {
    .signature    = MSC_CSW_SIGNATURE,
    .tag          = 0xCAFECAFE,
    .data_residue = DISK_BLOCK_SIZE + 1,
    .status       = MSC_CSW_STATUS_PASSED
  };

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);

  tud_task();
}

//--------------------------------------------------------------------+
// Reset Recovery (BOT 5.3.4)
//--------------------------------------------------------------------+
static tusb_control_request_t const request_bot_reset =
{
  .bmRequestType = 0x21,
  .bRequest      = MSC_REQ_RESET,
  .wValue        = 0,
  .wIndex        = ITF_NUM_MSC,
  .wLength       = 0
};

static void clear_halt(uint8_t ep_addr) {
  tusb_control_request_t const request_clear_halt =
  {
    .bmRequestType = 0x02,
    .bRequest      = TUSB_REQ_CLEAR_FEATURE,
    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
    .wIndex        = ep_addr,
    .wLength       = 0
  };

  dcd_event_setup_received(rhport, (uint8_t const*) &request_clear_halt, false);
  dcd_edpt_clear_stall_Expect(rhport, ep_addr);
}

// The clear-halts of Reset Recovery: the Bulk-Out one re-arms it for the next CBW
static void reset_recovery_clear_halts(void) {
  clear_halt(EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  clear_halt(EDPT_MSC_OUT);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// A Reset Recovery while a READ10 chunk is still armed (the host gave up on the Data Stage): the BOT reset stalls both
// data endpoints, which drops the stale transfer in the DCD, and the Bulk-Out clear re-arms it for the next CBW. A
// transfer left armed would carry the next command's data or CSW (#3188)
void test_msc_bot_reset_stalls_armed_data_endpoints(void)
{
  read10_2blocks_received();

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_OUT);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  reset_recovery_clear_halts();
}

// BOT reset while an async READ10 chunk is still pending in the application
static void bot_reset_during_async_read10(bool queue_completion) {
  read10_async = true;
  read10_2blocks_received();
  tud_task(); // tud_msc_read10_cb() answers TUD_MSC_RET_ASYNC: nothing armed on Bulk-In

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  if (queue_completion) {
    TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  }
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_OUT);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

// The BOT reset abandons the pending async READ10: its late completion before the clear-halts is discarded instead of
// arming the stalled (busy) Bulk-In
void test_msc_bot_reset_discards_late_async_read10_before_clear_halt(void)
{
  bot_reset_during_async_read10(false);

  TEST_ASSERT_FALSE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  tud_task(); // no Bulk-In transfer expected

  reset_recovery_clear_halts();
}

// A late completion after Reset Recovery is discarded too: stale data must not go out on the cleared Bulk-In
void test_msc_bot_reset_discards_late_async_read10_after_reset_recovery(void)
{
  bot_reset_during_async_read10(false);
  reset_recovery_clear_halts();

  TEST_ASSERT_FALSE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  tud_task(); // no Bulk-In transfer expected
}

// A completion accepted while the BOT reset is still queued passes the precheck in tud_msc_async_io_done(), but its
// deferred call runs after the reset: proc_async_io_done() finds no pending I/O and drops it instead of arming Bulk-In
void test_msc_bot_reset_drops_completion_queued_before_reset(void)
{
  bot_reset_during_async_read10(true);
  reset_recovery_clear_halts();
}

//--------------------------------------------------------------------+
// SystemView spans on the early returns
//--------------------------------------------------------------------+
// SET_CONFIGURATION, with the first CBW buffer armed to receive cbw
static void configure_msc(msc_cbw_t const* cbw) {
  desc_configuration = data_desc_configuration;
  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(desc_configuration));

  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep), true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) cbw, sizeof(msc_cbw_t));
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

void test_msc_invalid_cbw_closes_its_span(void)
{
  msc_cbw_t const cbw_bad = { .signature = 0xDEADBEEF };
  configure_msc(&cbw_bad);
  memset(sv_calls, 0, sizeof(sv_calls));

  // BOT 6.6.1: an invalid CBW stalls both endpoints, and mscd_xfer_cb() returns false
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_OUT);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  TEST_ASSERT_EQUAL_UINT(1, sv_calls[TU_SV_ID_MSC_XFER]);
}

//--------------------------------------------------------------------+
// Data-In reply shorter than the host's length: BOT 6.7.2 case 5 (Hi > Di)
//--------------------------------------------------------------------+
// The 4-byte MODE SENSE(6) reply goes out padded with fill data to the host's 192 bytes in one transfer, and Bulk-In is
// not stalled: behind a hub a STALL makes the host clear the hub's TT buffer (#4316). The residue counts only the reply
void test_msc_short_reply_padded_to_host_length(void)
{
  msc_cbw_t const cbw = data_in_cbw(SCSI_CMD_MODE_SENSE_6, 192);
  configure_msc(&cbw);

  uint8_t reply[192] = { 3, 0, 0, 0 }; // mode data length 3, writable, no block descriptor; the rest is fill data
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, reply, sizeof(reply), sizeof(reply), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(sizeof(reply) - 4);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(reply), 0, true);
  tud_task();
}

// A host length beyond the endpoint buffer cannot be padded in one transfer: the reply goes out short, Bulk-In is
// stalled as before, and the CSW follows the clear-halt
void test_msc_short_reply_beyond_buffer_stalls(void)
{
  uint32_t const total_bytes = CFG_TUD_MSC_EP_BUFSIZE + 64;
  msc_cbw_t const cbw = data_in_cbw(SCSI_CMD_INQUIRY, total_bytes);
  configure_msc(&cbw);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, sizeof(scsi_inquiry_resp_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(scsi_inquiry_resp_t), 0, true);
  tud_task();

  clear_halt(EDPT_MSC_IN);
  msc_csw_t const csw = csw_with_residue(total_bytes - sizeof(scsi_inquiry_resp_t));
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();
}

static unsigned deferred_count;
static void deferred_func(void* param) {
  (void) param;
  deferred_count++;
}

void test_tud_task_closes_its_span_on_an_endpoint_without_driver(void)
{
  deferred_count = 0;
  dcd_event_xfer_complete(rhport, 0x02, 8, XFER_RESULT_SUCCESS, true); // EP 2 is not opened
  usbd_defer_func(deferred_func, NULL, true);

  // the failed assertion stops this run with its span closed, leaving the next event queued
  tud_task();
  TEST_ASSERT_EQUAL_UINT(1, sv_calls[TU_SV_ID_TUD_TASK]);
  TEST_ASSERT_EQUAL_UINT(0, sv_depth);
  TEST_ASSERT_EQUAL_UINT(0, deferred_count);

  tud_task();
  TEST_ASSERT_EQUAL_UINT(2, sv_calls[TU_SV_ID_TUD_TASK]);
  TEST_ASSERT_EQUAL_UINT(1, deferred_count);
}

void test_tud_task_records_no_span_while_waiting_for_an_event(void)
{
  tud_task();
  TEST_ASSERT_EQUAL_UINT(0, sv_calls[TU_SV_ID_TUD_TASK]);
}
