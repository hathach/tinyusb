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

// tud_msc_read10_cb() answers TUD_MSC_RET_ASYNC, for a background read completed by tud_msc_async_io_done(). The
// data (less read10_shortfall) is already in the buffer, as a DMA read in progress would put it there
static bool read10_async;

// times tud_msc_read10_cb() answers TUD_MSC_RET_BUSY before it reads
static uint32_t read10_busy;

// calls of tud_msc_read10_cb()
static uint32_t read10_calls;

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
// how read10/write10 callbacks fail: not at all, after setting MEDIUM ERROR sense, or without setting a sense
static enum { IO_OK, IO_ERROR_WITH_SENSE, IO_ERROR_NO_SENSE } io_mode;

static int32_t io_failure(uint8_t lun)
{
  if (io_mode == IO_ERROR_WITH_SENSE) {
    (void) tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x5A);
  }
  return TUD_MSC_RET_ERROR;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  if (io_mode != IO_OK) {
    return io_failure(lun);
  }

  read10_calls++;
  if (read10_busy > 0) {
    read10_busy--;
    return TUD_MSC_RET_BUSY;
  }

  bufsize -= read10_shortfall;
  uint8_t const* addr = msc_disk[lba] + offset;
  memcpy(buffer, addr, bufsize);

  if (read10_async) {
    return TUD_MSC_RET_ASYNC;
  }

  return bufsize;
}

// times tud_msc_write10_cb() answers TUD_MSC_RET_BUSY before it writes
static uint32_t write10_busy;

// bytes the next tud_msc_write10_cb() handed a larger chunk accepts, once, leaving the rest for a retry (0: all)
static uint32_t write10_accept;

// tud_msc_write10_cb() answers TUD_MSC_RET_ASYNC, for a background write completed by tud_msc_async_io_done()
static bool write10_async;

// calls of tud_msc_write10_cb(), with the lba, offset, bufsize and buffer contents of the last one
static uint32_t write10_calls;
static uint32_t write10_last_lba;
static uint32_t write10_last_offset;
static uint32_t write10_last_bufsize;
static uint8_t write10_last_buf[CFG_TUD_MSC_EP_BUFSIZE];

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  if (io_mode != IO_OK) {
    return io_failure(lun);
  }

  write10_calls++;
  write10_last_lba = lba;
  write10_last_offset = offset;
  write10_last_bufsize = bufsize;
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(sizeof(write10_last_buf), bufsize);
  memcpy(write10_last_buf, buffer, bufsize);

  if (write10_busy > 0) {
    write10_busy--;
    return TUD_MSC_RET_BUSY;
  }

  if (write10_async) {
    return TUD_MSC_RET_ASYNC;
  }

  if (write10_accept > 0 && write10_accept < bufsize) {
    bufsize = write10_accept;
    write10_accept = 0;
  }

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

// the dcd mock copies a CBW when its transfer is queued, so they must outlive the expectation
static msc_cbw_t cbw_io;
static msc_cbw_t cbw_sense;

static void init_sense_cbw(void)
{
  cbw_sense = (msc_cbw_t)
  {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xBEEFBEEF,
    .total_bytes = sizeof(scsi_sense_fixed_resp_t),
    .lun         = 0,
    .dir         = TUSB_DIR_IN_MASK,
    .cmd_len     = sizeof(scsi_request_sense_t)
  };
  scsi_request_sense_t const cmd =
  {
    .cmd_code     = SCSI_CMD_REQUEST_SENSE,
    .alloc_length = sizeof(scsi_sense_fixed_resp_t)
  };
  memcpy(cbw_sense.command, &cmd, cbw_sense.cmd_len);
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
  init_sense_cbw();

  memset(sv_calls, 0, sizeof(sv_calls));
  memset(msc_disk, 0, sizeof(msc_disk));
  read10_shortfall = 0;
  read10_async = false;
  read10_busy = 0;
  read10_calls = 0;
  write10_busy = 0;
  write10_async = false;
  write10_accept = 0;
  write10_calls = 0;
}

void tearDown(void)
{
  TEST_ASSERT_EQUAL_UINT(0, sv_depth);
  io_mode = IO_OK;
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

// A partial chunk would end the Data Stage with its short packet: it is filled with zeros up to the host's length
// instead (CFG_TUD_MSC_PAD_SHORT_REPLY), and the CSW follows with the residue and no STALL (#3188, #4316)
void test_msc_read10_short_chunk_padded(void)
{
  read10_shortfall = 1;
  memset(msc_disk[0], 0xA5, DISK_BLOCK_SIZE);
  read10_2blocks_received();

  uint8_t chunk[DISK_BLOCK_SIZE] = { 0 };
  memset(chunk, 0xA5, DISK_BLOCK_SIZE - 1);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, chunk, DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);

  uint8_t fill[DISK_BLOCK_SIZE] = { 0 };
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, fill, DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(DISK_BLOCK_SIZE + 1);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);
  tud_task();
}

// A READ10 chunk the application answers TUD_MSC_RET_BUSY is read again by the deferred retry (proc_io_retry()),
// from the same place, and the command completes with no residue
void test_msc_read10_busy_retries(void)
{
  read10_busy = 2;
  memset(msc_disk[0], 0x5A, DISK_BLOCK_SIZE);
  memset(msc_disk[1], 0xC3, DISK_BLOCK_SIZE);
  read10_2blocks_received();

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, msc_disk[0], DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(0, read10_busy);
  TEST_ASSERT_EQUAL_UINT32(3, read10_calls);

  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, msc_disk[1], DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(4, read10_calls);
}

// An async READ10 chunk completed short by tud_msc_async_io_done() takes the padded path like a synchronous one: the
// chunk is filled with zeros, fill data follows up to the host's length, and the CSW carries the residue, no STALL
void test_msc_read10_async_short_completion_padded(void)
{
  read10_async = true;
  read10_shortfall = 1;
  memset(msc_disk[0], 0xA5, DISK_BLOCK_SIZE);
  read10_2blocks_received();
  tud_task(); // tud_msc_read10_cb() answers TUD_MSC_RET_ASYNC: nothing armed on Bulk-In

  uint8_t chunk[DISK_BLOCK_SIZE] = { 0 };
  memset(chunk, 0xA5, DISK_BLOCK_SIZE - 1);
  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE - 1, false));
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, chunk, DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  tud_task();

  uint8_t fill[DISK_BLOCK_SIZE] = { 0 };
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, fill, DISK_BLOCK_SIZE, DISK_BLOCK_SIZE, false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(DISK_BLOCK_SIZE + 1);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, DISK_BLOCK_SIZE, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, read10_calls);
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

static void expect_cbw_receive(void) {
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
}

// The CBW receive, into which the host's next CBW lands
static void expect_cbw_xfer(msc_cbw_t const* cbw)
{
  expect_cbw_receive();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer((uint8_t*) (uintptr_t) cbw, sizeof(msc_cbw_t));
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

// WRITE10 of one block, LBA 0
static msc_cbw_t const cbw_write10_1block =
{
  .signature   = MSC_CBW_SIGNATURE,
  .tag         = 0xCAFECAFE,
  .total_bytes = DISK_BLOCK_SIZE,
  .lun         = 0,
  .dir         = 0,
  .cmd_len     = sizeof(scsi_write10_t),
  .command     = { SCSI_CMD_WRITE_10, 0, 0, 0, 0, 0, 0, 0, 1 }
};

// Which data endpoints a BOT reset finds with a transfer armed for the abandoned command: it aborts each of them
// (usbd_edpt_abort(), with a DCD that aborts without a stall) and no other. Unless an abandoned async I/O still owns the
// buffer, it then arms the CBW receive. Then the control status
enum {
  ABORT_NONE = 0,
  ABORT_IN   = 1,
  ABORT_OUT  = 2,
};

static void expect_bot_reset(unsigned aborts, bool cbw_receive) {
  if (aborts & ABORT_IN) {
    dcd_edpt_abort_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, true);
  }
  if (aborts & ABORT_OUT) {
    dcd_edpt_abort_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, true);
  }
  if (cbw_receive) {
    expect_cbw_receive();
  }
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
}

// A Reset Recovery while a READ10 chunk is still armed (the host gave up on the Data Stage): the BOT reset aborts the
// stale transfer, which Reset Recovery's clear-halts would keep, so it can't carry the next command's data or CSW
// (#3188). Bulk-Out, idle since the CBW, is not touched; the reset arms the CBW receive on it. Nothing is stalled
void test_msc_bot_reset_aborts_armed_data_in(void)
{
  read10_2blocks_received();

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_IN, true);
  tud_task();

  reset_recovery_clear_halts();
}

// A BOT reset while a WRITE10 Data-Out receive is still armed: that stale receive is aborted, so the host's next CBW
// is not taken as WRITE10 data, and the reset arms the CBW receive in its place. Bulk-In, idle, is not touched, and
// nothing is stalled
void test_msc_bot_reset_aborts_armed_data_out(void)
{
  configure_msc(&cbw_write10_1block);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_OUT, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(0, write10_calls);

  reset_recovery_clear_halts();
}

// TEST UNIT READY: no Data Stage, a CSW with tag 0xCAFECAFE
static msc_cbw_t const cbw_test_unit_ready =
{
  .signature   = MSC_CBW_SIGNATURE,
  .tag         = 0xCAFECAFE,
  .total_bytes = 0,
  .lun         = 0,
  .dir         = 0,
  .cmd_len     = sizeof(scsi_test_unit_ready_t),
  .command     = { SCSI_CMD_TEST_UNIT_READY }
};

// A BOT reset while the CSW of the abandoned command is still armed on Bulk-In: the stale CSW is aborted, so it can't
// answer the next command's status read, and the reset arms the CBW receive, which the CSW still held up
void test_msc_bot_reset_aborts_armed_csw(void)
{
  configure_msc(&cbw_test_unit_ready);

  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_IN, true);
  tud_task();

  reset_recovery_clear_halts();
}

// A BOT reset between commands, with the CBW receive armed again after the last CSW: neither endpoint holds a stale
// transfer, so neither is aborted, stalled or cleared, keeping both data toggles (BOT 3.1), and the CBW receive is not
// armed a second time. The host's next CBW lands in the receive still armed, with no clear-halts needed
void test_msc_bot_reset_idle_keeps_cbw_receive(void)
{
  static msc_cbw_t cbw_next;
  cbw_next = cbw_test_unit_ready;
  cbw_next.tag = 0x12345678;

  configure_msc(&cbw_test_unit_ready);

  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  // the CSW is sent: the CBW receive is armed again, and the host's next CBW will land in it
  expect_cbw_xfer(&cbw_next);
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(msc_csw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_NONE, false);
  tud_task();

  msc_csw_t csw_next = csw_with_residue(0);
  csw_next.tag = cbw_next.tag;
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw_next, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();
}

// A BOT reset keeps a stall (BOT 3.1): after an invalid CBW both endpoints stay stalled until Reset Recovery's
// clear-halts, which receive the next CBW (BOT 6.6.1)
void test_msc_bot_reset_keeps_stalls_after_invalid_cbw(void)
{
  msc_cbw_t const cbw_bad = { .signature = 0xDEADBEEF };
  configure_msc(&cbw_bad);

  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_OUT);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  reset_recovery_clear_halts();
}

// BOT reset while an async READ10 chunk is still pending in the application. With queue_failed_completion, the
// application fails the read (TUD_MSC_RET_ERROR) after the reset is queued but before it runs
static void bot_reset_during_async_read10(bool queue_failed_completion) {
  read10_async = true;
  read10_2blocks_received();
  tud_task(); // tud_msc_read10_cb() answers TUD_MSC_RET_ASYNC: nothing armed on Bulk-In

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  if (queue_failed_completion) {
    TEST_ASSERT_TRUE(tud_msc_async_io_done(TUD_MSC_RET_ERROR, false));
  }
  expect_bot_reset(ABORT_NONE, false); // nothing armed on either endpoint
  if (queue_failed_completion) {
    expect_cbw_receive(); // the completion, discarded, frees the buffer for the next CBW
  }
  tud_task();
}

// The BOT reset abandons the pending async READ10: its late completion before the clear-halts is accepted and discarded
// instead of sending its data on Bulk-In, and arms the CBW receive the abandoned I/O held up
void test_msc_bot_reset_discards_late_async_read10_before_clear_halt(void)
{
  bot_reset_during_async_read10(false);

  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  expect_cbw_receive();
  tud_task();

  reset_recovery_clear_halts();
}

// Until the abandoned read completes, it owns the buffer: Reset Recovery's Bulk-Out clear-halt receives no CBW, and the
// late completion then arms the CBW receive instead of sending stale data on the cleared Bulk-In (#4390)
void test_msc_bot_reset_discards_late_async_read10_after_reset_recovery(void)
{
  bot_reset_during_async_read10(false);

  clear_halt(EDPT_MSC_IN);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  clear_halt(EDPT_MSC_OUT);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task(); // no CBW receive armed yet

  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  expect_cbw_receive();
  tud_task();
}

// A completion accepted while the BOT reset is still queued runs after the reset: proc_async_io_done() finds the CMD
// stage and discards it. A failed completion is used because one applied to the reset state would stall Bulk-In again
// and leave a failed CSW for the Bulk-In clear-halt to send: either is an unexpected DCD call
void test_msc_bot_reset_drops_completion_queued_before_reset(void)
{
  bot_reset_during_async_read10(true);
  reset_recovery_clear_halts();
}

// A bus reset while an async I/O is still pending in the application, then re-enumeration: SET_CONFIGURATION opens the
// endpoints but, with the abandoned I/O still owning the buffer, receives no CBW
static void bus_reset_reconfigure_without_cbw(void) {
  dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, false);
  tud_task();

  uint8_t const* desc_ep = tu_desc_next(tu_desc_next(desc_configuration));
  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) desc_ep, true);
  dcd_edpt_open_ExpectAndReturn(rhport, (tusb_desc_endpoint_t const *) tu_desc_next(desc_ep), true);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task(); // no CBW receive armed yet
}

// A bus reset abandons a pending async read the same way: re-enumeration opens the endpoints but receives no CBW until
// the late completion, which is discarded
void test_msc_bus_reset_waits_for_abandoned_async_read10(void)
{
  read10_async = true;
  read10_2blocks_received();
  tud_task();

  bus_reset_reconfigure_without_cbw();

  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  expect_cbw_receive();
  tud_task();
}

// tud_msc_read10_cb() answers TUD_MSC_RET_BUSY while the BOT reset is queued: the retry runs after the reset and is
// dropped, so no stale chunk is read or armed on Bulk-In after Reset Recovery
void test_msc_bot_reset_drops_read10_busy_retry(void)
{
  read10_busy = 1;
  configure_msc(&cbw_read10_2blocks);

  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_NONE, true); // the CBW is consumed and the busy chunk armed nothing
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, read10_calls);

  reset_recovery_clear_halts();
  TEST_ASSERT_EQUAL_UINT32(1, read10_calls);
}

// tud_msc_write10_cb() answers TUD_MSC_RET_BUSY while the BOT reset is queued: the retry runs after the reset and is
// dropped. Taken as a CBW of the wrong length, it would stall both endpoints for another Reset Recovery and fail this
// one's clear-halts
void test_msc_bot_reset_drops_write10_busy_retry(void)
{
  write10_busy = 1;
  configure_msc(&cbw_write10_1block);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, DISK_BLOCK_SIZE, 0, true);
  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_NONE, true); // the CBW and the host data are consumed: nothing armed
  tud_task();

  reset_recovery_clear_halts();
}

// A WRITE10 chunk the application answers TUD_MSC_RET_BUSY is offered again, and the command completes
void test_msc_write10_busy_retries(void)
{
  write10_busy = 2;
  configure_msc(&cbw_write10_1block);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, DISK_BLOCK_SIZE, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(0, write10_busy);
}

// A WRITE10 chunk the application accepts only in part: the unaccepted bytes move to the front of the buffer and the
// deferred retry offers exactly them, at the offset after the accepted prefix, and the command completes
void test_msc_write10_partial_accept_retries_left_over(void)
{
  uint32_t const accepted = 100;
  write10_accept = accepted;

  uint8_t payload[DISK_BLOCK_SIZE];
  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t) i;
  }

  configure_msc(&cbw_write10_1block);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer(payload, sizeof(payload));
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  // the retry consumes the rest: with the accepted prefix counted, the Data Stage is complete and the CSW follows
  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, DISK_BLOCK_SIZE, 0, true);
  tud_task();

  TEST_ASSERT_EQUAL_UINT32(2, write10_calls);
  TEST_ASSERT_EQUAL_UINT32(0, write10_last_lba);
  TEST_ASSERT_EQUAL_UINT32(accepted, write10_last_offset);
  TEST_ASSERT_EQUAL_UINT32(DISK_BLOCK_SIZE - accepted, write10_last_bufsize);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload + accepted, write10_last_buf, DISK_BLOCK_SIZE - accepted);
}

// SET_CONFIGURATION, the CBW of a 1-block WRITE10 and its host data (payload, if not NULL): tud_msc_write10_cb()
// answers TUD_MSC_RET_ASYNC, the write stays pending in the application
static void write10_1block_async_pending(uint8_t* payload) {
  write10_async = true;
  configure_msc(&cbw_write10_1block);

  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, DISK_BLOCK_SIZE, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  if (payload != NULL) {
    dcd_edpt_xfer_ReturnMemThruPtr_buffer(payload, DISK_BLOCK_SIZE);
  }
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  tud_task();

  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, DISK_BLOCK_SIZE, 0, true);
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, write10_calls);
}

// An async WRITE10 completed by tud_msc_async_io_done() with fewer bytes than handed over: the rest moves to the
// front of the buffer and the deferred retry offers exactly it, at the offset after the written prefix, and the
// command completes
void test_msc_write10_async_partial_completion_retries_left_over(void)
{
  uint32_t const written = 100;
  uint8_t payload[DISK_BLOCK_SIZE];
  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t) (i + 7);
  }

  write10_1block_async_pending(payload);

  write10_async = false; // the retry writes at once
  TEST_ASSERT_TRUE(tud_msc_async_io_done((int32_t) written, false));
  msc_csw_t const csw = csw_with_residue(0);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(msc_csw_t), sizeof(msc_csw_t), false, true);
  tud_task();

  TEST_ASSERT_EQUAL_UINT32(2, write10_calls);
  TEST_ASSERT_EQUAL_UINT32(0, write10_last_lba);
  TEST_ASSERT_EQUAL_UINT32(written, write10_last_offset);
  TEST_ASSERT_EQUAL_UINT32(DISK_BLOCK_SIZE - written, write10_last_bufsize);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload + written, write10_last_buf, DISK_BLOCK_SIZE - written);
}

// The BOT reset abandons a pending async WRITE10 as it does a READ10: no CBW is received until its late completion,
// which is discarded (no retry, no CSW) and arms the CBW receive
void test_msc_bot_reset_discards_late_async_write10(void)
{
  write10_1block_async_pending(NULL);

  dcd_event_setup_received(rhport, (uint8_t const*) &request_bot_reset, false);
  expect_bot_reset(ABORT_NONE, false); // the host data is consumed: nothing armed
  tud_task();

  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE / 2, false));
  expect_cbw_receive();
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, write10_calls);

  reset_recovery_clear_halts();
}

// A bus reset abandons a pending async WRITE10 the same way: re-enumeration opens the endpoints but receives no CBW
// until the late completion, which is discarded
void test_msc_bus_reset_waits_for_abandoned_async_write10(void)
{
  write10_1block_async_pending(NULL);

  bus_reset_reconfigure_without_cbw();

  TEST_ASSERT_TRUE(tud_msc_async_io_done(DISK_BLOCK_SIZE, false));
  expect_cbw_receive();
  tud_task();
  TEST_ASSERT_EQUAL_UINT32(1, write10_calls);
}

//--------------------------------------------------------------------+
// read10/write10 error sense
//--------------------------------------------------------------------+
// SET_CONFIGURATION, then the host sends a one-block READ10 or WRITE10 of LBA 0
static void configure_and_send_io_cbw(uint8_t cmd_code)
{
  cbw_io = (msc_cbw_t)
  {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xCAFECAFE,
    .total_bytes = 512,
    .lun         = 0,
    .dir         = (cmd_code == SCSI_CMD_READ_10) ? TUSB_DIR_IN_MASK : 0,
    .cmd_len     = sizeof(scsi_read10_t)
  };
  scsi_read10_t const cmd =
  {
    .cmd_code    = cmd_code,
    .lba         = tu_htonl(0),
    .block_count = tu_htons(1)
  };
  memcpy(cbw_io.command, &cmd, cbw_io.cmd_len);

  configure_msc(&cbw_io);
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
}

// The REQUEST SENSE CBW transfer is already expected: the host sends it and must read `sense_key`/`asc`/`ascq`
static void host_request_sense(uint8_t sense_key, uint8_t asc, uint8_t ascq)
{
  static scsi_sense_fixed_resp_t sense;
  sense = (scsi_sense_fixed_resp_t)
  {
    .response_code  = 0x70,
    .valid          = 1,
    .sense_key      = sense_key,
    .add_sense_len  = sizeof(scsi_sense_fixed_resp_t) - 8,
    .add_sense_code = asc,
    .add_sense_qualifier = ascq,
  };
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &sense, sizeof(sense), sizeof(sense), false, true);
}

// No data moved before the failure, so the CSW reports the whole transfer as residue
static void expect_failed_io_csw(void)
{
  static msc_csw_t csw;
  csw = csw_with_residue(512);
  csw.status = MSC_CSW_STATUS_FAILED;
  dcd_edpt_xfer_ExpectWithArrayAndReturn(rhport, EDPT_MSC_IN, (uint8_t*) &csw, sizeof(csw), sizeof(csw), false, true);
}

static void check_read10_error_sense(uint8_t sense_key, uint8_t asc, uint8_t ascq)
{
  configure_and_send_io_cbw(SCSI_CMD_READ_10);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_IN);
  tud_task();

  clear_halt(EDPT_MSC_IN);
  expect_failed_io_csw();
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  tud_task();

  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(msc_csw_t), 0, true);
  expect_cbw_xfer(&cbw_sense);
  host_request_sense(sense_key, asc, ascq);
  tud_task();
}

static void check_write10_error_sense(uint8_t sense_key, uint8_t asc, uint8_t ascq)
{
  configure_and_send_io_cbw(SCSI_CMD_WRITE_10);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, 512, false, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  tud_task();

  // the callback fails the data: Bulk-Out stalls and the failed CSW goes out at once
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, 512, 0, true);
  dcd_edpt_stall_Expect(rhport, EDPT_MSC_OUT);
  expect_failed_io_csw();
  tud_task();

  // no CBW is queued while Bulk-Out is halted
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, sizeof(msc_csw_t), 0, true);
  tud_task();

  clear_halt(EDPT_MSC_OUT);
  expect_cbw_xfer(&cbw_sense);
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, false, true);
  host_request_sense(sense_key, asc, ascq);
  tud_task();
}

// A read10/write10 error keeps the sense the callback set instead of reporting MEDIUM NOT PRESENT
void test_msc_read10_error_keeps_callback_sense(void)
{
  io_mode = IO_ERROR_WITH_SENSE;
  check_read10_error_sense(SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x5A);
}

void test_msc_write10_error_keeps_callback_sense(void)
{
  io_mode = IO_ERROR_WITH_SENSE;
  check_write10_error_sense(SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x5A);
}

// Without a sense from the callback, a read10/write10 error defaults to NOT READY / MEDIUM NOT PRESENT
void test_msc_read10_error_default_sense(void)
{
  io_mode = IO_ERROR_NO_SENSE;
  check_read10_error_sense(SCSI_SENSE_NOT_READY, 0x3A, 0x00);
}

void test_msc_write10_error_default_sense(void)
{
  io_mode = IO_ERROR_NO_SENSE;
  check_write10_error_sense(SCSI_SENSE_NOT_READY, 0x3A, 0x00);
}

//--------------------------------------------------------------------+
// SystemView spans on the early returns
//--------------------------------------------------------------------+
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

  // the stale completion is dropped with its span closed, and the run goes on to the next event
  tud_task();
  TEST_ASSERT_EQUAL_UINT(2, sv_calls[TU_SV_ID_TUD_TASK]);
  TEST_ASSERT_EQUAL_UINT(0, sv_depth);
  TEST_ASSERT_EQUAL_UINT(1, deferred_count);
}

void test_tud_task_records_no_span_while_waiting_for_an_event(void)
{
  tud_task();
  TEST_ASSERT_EQUAL_UINT(0, sv_calls[TU_SV_ID_TUD_TASK]);
}
