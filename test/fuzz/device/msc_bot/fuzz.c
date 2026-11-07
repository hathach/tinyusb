#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "class/msc/msc_device.c"

static uint8_t queued_ep;
static uint8_t *queued_buffer;
static uint16_t queued_length;

// Minimal per-endpoint stall state (index: 0 = EP 0x01 OUT, 1 = EP 0x81 IN),
// so the driver's stall / clear-stall recovery branches are exercised.
// Reset at the start of every input.
static bool ep_stalled[2];

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t length, bool is_isr) {
  (void) rhport;
  (void) is_isr;
  queued_ep = ep_addr;
  queued_buffer = buffer;
  queued_length = length;
  return true;
}

bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const *desc, uint8_t count, uint8_t type,
                         uint8_t *ep_out, uint8_t *ep_in) {
  (void) rhport;
  (void) desc;
  (void) count;
  (void) type;
  *ep_out = 0x01;
  *ep_in = 0x81;
  return true;
}

void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  ep_stalled[ep_addr >> 7] = true;
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  return ep_stalled[ep_addr >> 7];
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return false;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  ep_stalled[ep_addr >> 7] = false;
}

void usbd_defer_func(osal_task_func_t func, void *param, bool in_isr) {
  (void) in_isr;
  func(param);
}

bool tud_control_status(uint8_t rhport, tusb_control_request_t const *request) {
  (void) rhport;
  (void) request;
  return true;
}

bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const *request, void *buffer, uint16_t length) {
  (void) rhport;
  (void) request;
  (void) buffer;
  (void) length;
  return true;
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void) lun;
  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
  (void) lun;
  *block_count = 1024;
  *block_size = 512;
}

// READ10/WRITE10 callback results chosen by fuzz input: each callback takes two control bytes (op, arg) from cb_ctl
// and, once those run out, transfers everything. usbd_defer_func() above runs a BUSY or partial-write retry at once,
// nested in the callback's caller, so cb_retries bounds that recursion. An ASYNC result is completed by the main loop.
static uint8_t const *cb_ctl;
static size_t cb_ctl_len;
static unsigned cb_retries;
static bool async_pending;
static int32_t async_result;

enum {
  CB_ALL = 0,
  CB_PARTIAL,
  CB_BUSY,
  CB_ERROR,
  CB_ASYNC,
  CB_OP_COUNT
};

static int32_t cb_result(uint32_t bufsize, bool is_write) {
  if (cb_ctl_len < 2 || bufsize == 0) {
    return (int32_t) bufsize;
  }
  uint8_t const op = cb_ctl[0] % CB_OP_COUNT;
  uint8_t const arg = cb_ctl[1];
  cb_ctl += 2;
  cb_ctl_len -= 2;

  // 1..bufsize bytes; a write retries what it did not consume
  int32_t const partial = (int32_t) (1u + arg % bufsize);
  switch (op) {
    case CB_PARTIAL:
      if (is_write) {
        if (cb_retries == 0) {
          return (int32_t) bufsize;
        }
        cb_retries--;
      }
      return partial;

    case CB_BUSY:
      if (cb_retries == 0) {
        return (int32_t) bufsize;
      }
      cb_retries--;
      return TUD_MSC_RET_BUSY;

    case CB_ERROR:
      return TUD_MSC_RET_ERROR;

    case CB_ASYNC:
      // completed with everything, a partial count (arg bit 7) or an error (arg 0)
      async_result = (arg == 0) ? TUD_MSC_RET_ERROR : ((arg & 0x80u) ? partial : (int32_t) bufsize);
      async_pending = true;
      return TUD_MSC_RET_ASYNC;

    default:
      return (int32_t) bufsize;
  }
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void) lun;
  (void) lba;
  (void) offset;
  memset(buffer, 0, bufsize);
  return cb_result(bufsize, false);
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                            uint8_t *buffer, uint32_t bufsize) {
  (void) lun;
  (void) lba;
  (void) offset;
  (void) buffer;
  return cb_result(bufsize, true);
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const command[16], void *buffer, uint16_t bufsize) {
  (void) lun;
  (void) command;
  (void) buffer;
  (void) bufsize;
  return -1;
}

// Input: the CBW, a count n, n callback control bytes (cb_result()), then the Bulk-Out data
int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size) {
  if (size < sizeof(msc_cbw_t)) return 0;

  ep_stalled[0] = ep_stalled[1] = false;

  size_t offset = sizeof(msc_cbw_t);
  cb_ctl_len = 0;
  if (offset < size) {
    cb_ctl_len = data[offset++];
    if (cb_ctl_len > size - offset) cb_ctl_len = size - offset;
  }
  cb_ctl = data + offset;
  offset += cb_ctl_len;
  cb_retries = 16;
  async_pending = false;

  mscd_init();
  _mscd_itf.rhport = 0;
  _mscd_itf.ep_out = 0x01;
  _mscd_itf.ep_in = 0x81;
  _mscd_itf.ep_in_mps = 64; // FS bulk size: stands in for what mscd_open() takes from the Bulk-In descriptor
  prepare_cbw(&_mscd_itf);

  msc_cbw_t cbw;
  memcpy(&cbw, data, sizeof(cbw));
  cbw.signature = MSC_CBW_SIGNATURE;
  memcpy(queued_buffer, &cbw, sizeof(cbw));
  queued_length = 0;  // don't re-complete the CBW if nothing new was queued
  mscd_xfer_cb(0, 0x01, XFER_RESULT_SUCCESS, sizeof(cbw));

  // Allow enough steps for multi-block READ10/WRITE10 commands to reach their
  // CSW (one CFG_TUD_MSC_EP_BUFSIZE chunk per step, or less after a partial
  // read or write, plus async completions), with a hard cap to bound run time
  // for fuzz-controlled total_bytes. The driver ends the loop by itself once
  // the stage is back to MSC_STAGE_CMD or MSC_STAGE_NEED_RESET.
  unsigned max_steps = cbw.total_bytes / CFG_TUD_MSC_EP_BUFSIZE + 4 + (unsigned) cb_ctl_len;
  if (max_steps > 1024) max_steps = 1024;

  for (unsigned step = 0; step < max_steps && _mscd_itf.stage != MSC_STAGE_CMD &&
                          _mscd_itf.stage != MSC_STAGE_NEED_RESET; ++step) {
    if (async_pending) {
      // the application finishes the I/O its callback answered TUD_MSC_RET_ASYNC
      async_pending = false;
      (void) tud_msc_async_io_done(async_result, false);
      continue;
    }
    uint8_t ep = queued_ep;
    uint16_t length = queued_length;
    if (length == 0 || length > CFG_TUD_MSC_EP_BUFSIZE) break;
    if (_mscd_itf.stage == MSC_STAGE_DATA && ep == 0x01) {
      if (offset + length > size) break;
      memcpy(queued_buffer, data + offset, length);
      offset += length;
    }
    queued_length = 0;
    mscd_xfer_cb(0, ep, XFER_RESULT_SUCCESS, length);
  }
  return 0;
}
