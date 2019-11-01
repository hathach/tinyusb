/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 hathach for Adafruit Industries
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

uint8_t const data_desc_configuration[] =
{
  // Interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(ITF_NUM_TOTAL, 0, TUD_CONFIG_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : 64),
};

tusb_control_request_t const request_set_configuration =
{
  .bmRequestType = 0x80,
  .bRequest      = TUSB_REQ_SET_CONFIGURATION,
  .wValue        = 0,
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

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
    break;

    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, resplen);
    }else
    {
      // SCSI output
    }
  }

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

uint16_t const* tud_descriptor_string_cb(uint8_t index)
{
  return NULL;
}

void setUp(void)
{
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();

  if ( !tusb_inited() )
  {
    dcd_init_Expect(rhport);
    tusb_init();
  }
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void test_msc(void)
{
  desc_configuration = data_desc_configuration;

//  dcd_event_setup_received(rhport, (uint8_t*) &request_set_configuration, false);


//  tud_task();
}
