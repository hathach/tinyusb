#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "class/msc/msc_device.c"

static uint8_t queued_ep;
static uint8_t *queued_buffer;
static uint16_t queued_length;

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
  (void) ep_addr;
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return false;
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return false;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

void usbd_defer_func(osal_task_func_t func, void *param, bool in_isr) {
  (void) in_isr;
  func(param);
}

void dcd_event_handler(dcd_event_t const *event, bool in_isr) {
  (void) in_isr;
  mscd_xfer_cb(event->rhport, event->xfer_complete.ep_addr,
              (xfer_result_t) event->xfer_complete.result, event->xfer_complete.len);
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

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void) lun;
  (void) lba;
  (void) offset;
  memset(buffer, 0, bufsize);
  return (int32_t) bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                            uint8_t *buffer, uint32_t bufsize) {
  (void) lun;
  (void) lba;
  (void) offset;
  (void) buffer;
  return (int32_t) bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const command[16], void *buffer, uint16_t bufsize) {
  (void) lun;
  (void) command;
  (void) buffer;
  (void) bufsize;
  return -1;
}

int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size) {
  if (size < sizeof(msc_cbw_t)) return 0;

  mscd_init();
  _mscd_itf.rhport = 0;
  _mscd_itf.ep_out = 0x01;
  _mscd_itf.ep_in = 0x81;
  prepare_cbw(&_mscd_itf);

  msc_cbw_t cbw;
  memcpy(&cbw, data, sizeof(cbw));
  cbw.signature = MSC_CBW_SIGNATURE;
  memcpy(queued_buffer, &cbw, sizeof(cbw));
  mscd_xfer_cb(0, 0x01, XFER_RESULT_SUCCESS, sizeof(cbw));

  size_t offset = sizeof(cbw);
  for (unsigned step = 0; step < 8 && _mscd_itf.stage != MSC_STAGE_CMD &&
                          _mscd_itf.stage != MSC_STAGE_NEED_RESET; ++step) {
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