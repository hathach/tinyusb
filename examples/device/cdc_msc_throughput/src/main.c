/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"

// cdc_msc_throughput: minimal CDC+MSC device aimed at measuring pure USB bulk throughput.
// MSC read/write callbacks don't touch any backing storage - write discards the
// data and read only zero-fills the low LBAs the host scans during enumeration
// (partition table, GPT header). Higher LBAs return whatever is already in the
// transfer buffer, so `dd` numbers reflect the USB/driver ceiling, not any
// simulated storage or per-byte memset cost.
// CDC path drains RX in tud_cdc_rx_cb and sources TX from a static filler in the
// main loop so `dd` can target /dev/ttyACMx in either direction.

static void cdc_throughput_task(void);

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+
int main(void) {
  board_init();

  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  while (1) {
    tud_task();
    cdc_throughput_task();
  }
}

//--------------------------------------------------------------------+
// CDC callbacks + tasks
//--------------------------------------------------------------------+
void tud_cdc_rx_cb(uint8_t itf) {
  (void) itf;
  tud_cdc_read_flush(); // Drain RX
}

static void cdc_throughput_task(void) {
  if (!tud_cdc_connected()) return;

  // Source TX: fill whatever write room is free.
  static uint8_t const filler[CFG_TUD_CDC_TX_EPSIZE] = {0};
  uint32_t room = tud_cdc_write_available();
  while (room > 0) {
    uint32_t n = tud_cdc_write(filler, tu_min32(room, sizeof(filler)));
    if (n == 0) {
      break;
    }
    room -= n;
  }
  tud_cdc_write_flush();
}

//--------------------------------------------------------------------+
// MSC callbacks
//--------------------------------------------------------------------+

// 1 GiB logical capacity so `dd` can run long enough for stable numbers.
// No real backing store - block content is synthesised on read, discarded on write.
enum {
  DISK_BLOCK_SIZE  = 512,
  DISK_BLOCK_COUNT = 0x00200000u, // 2 Mi blocks = 1 GiB
  // Kernel probes partition-table / filesystem-superblock locations near the
  // start of the disk during enumeration. Zero-fill only this head range so the
  // block layer sees "no partition, no filesystem" and leaves us alone; higher
  // LBAs skip the memset so `dd` measures pure USB/driver throughput.
  DISK_ZEROFILL_LBA = 64, // 32 KiB
};

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  (void) lun;
  const char vid[] = "TinyUSB";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";
  (void) strncpy((char*) vendor_id, vid, 8);
  (void) strncpy((char*) product_id, pid, 16);
  (void) strncpy((char*) product_rev, rev, 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void) lun;
  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
  (void) lun;
  *block_count = DISK_BLOCK_COUNT;
  *block_size  = DISK_BLOCK_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun; (void) power_condition; (void) start; (void) load_eject;
  return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  (void) lun;
  return true;
}

// READ10: zero-fill only the head range the kernel inspects, skip memset everywhere
// else so we measure the USB / driver path rather than memset cost.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void) lun; (void) offset;
  if (lba < DISK_ZEROFILL_LBA) {
    memset(buffer, 0, bufsize);
  } else {
    (void) buffer;
  }
  return (int32_t) bufsize;
}

// WRITE10: discard the received data entirely - this is the pure USB-speed test.
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  (void) lun; (void) lba; (void) offset; (void) buffer;
  return (int32_t) bufsize;
}

// Unknown SCSI commands: stall with Invalid Command sense.
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
  (void) scsi_cmd; (void) buffer; (void) bufsize;
  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
  return -1;
}
