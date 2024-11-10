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
TEST_FILE("usbd_control.c")
TEST_FILE("msc_device.c")

// Mock File
#include "mock_dcd.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint32_t tusb_time_millis_api(void) {
  return 0;
}

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

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void) lun;

  const char vid[] = "TinyUSB";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";

  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
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

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

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
}

void tearDown(void)
{
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
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_edpt_xfer_ReturnMemThruPtr_buffer( (uint8_t*) &cbw_read10, sizeof(msc_cbw_t));

  // command received
  dcd_event_xfer_complete(rhport, EDPT_MSC_OUT, sizeof(msc_cbw_t), 0, true);

  // control status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_CTRL_IN, NULL, 0, true);

  // SCSI Data transfer
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, 512, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 512, 0, true); // complete

  // SCSI Status
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_IN, NULL, 13, true);
  dcd_edpt_xfer_IgnoreArg_buffer();
  dcd_event_xfer_complete(rhport, EDPT_MSC_IN, 13, 0, true);

  // Prepare for next command
  dcd_edpt_xfer_ExpectAndReturn(rhport, EDPT_MSC_OUT, NULL, sizeof(msc_cbw_t), true);
  dcd_edpt_xfer_IgnoreArg_buffer();

  tud_task();
}
