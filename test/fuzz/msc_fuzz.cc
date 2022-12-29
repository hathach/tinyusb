#include "fuzz/fuzz_private.h"
#include "tusb.h"
#include <cassert>
#include <array>
#include <limits>

#if CFG_TUD_MSC==1

// Whether host does safe eject.
// tud_msc_get_maxlun_cb returns a uint8_t so the max logical units that are
// allowed is 255, so we need to keep track of 255 fuzzed logical units.
static std::array<bool, std::numeric_limits<uint8_t>::max()> ejected = {false};

extern "C" {
// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16,
// 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  (void)lun;
  assert(_fuzz_data_provider.has_value());

  std::string vid = _fuzz_data_provider->ConsumeBytesAsString(8);
  std::string pid = _fuzz_data_provider->ConsumeBytesAsString(16);
  std::string rev = _fuzz_data_provider->ConsumeBytesAsString(4);

  memcpy(vendor_id, vid.c_str(), strlen(vid.c_str()));
  memcpy(product_id, pid.c_str(), strlen(pid.c_str()));
  memcpy(product_rev, rev.c_str(), strlen(rev.c_str()));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  // RAM disk is ready until ejected
  if (ejected[lun]) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return _fuzz_data_provider->ConsumeBool();
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and
// SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size Application update
// block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  (void)lun;
  *block_count = _fuzz_data_provider->ConsumeIntegral<uint32_t>();
  *block_size = _fuzz_data_provider->ConsumeIntegral<uint16_t>();
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
  (void)power_condition;
  assert(_fuzz_data_provider.has_value());

  if (load_eject) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      ejected[lun] = true;
    }
  }

  return _fuzz_data_provider->ConsumeBool();
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  assert(_fuzz_data_provider.has_value());
  (void)lun;
  (void)lba;
  (void)offset;

  std::vector<uint8_t> consumed_buffer = _fuzz_data_provider->ConsumeBytes<uint8_t>(
      _fuzz_data_provider->ConsumeIntegralInRange<uint32_t>(0, bufsize));
  memcpy(buffer, consumed_buffer.data(), consumed_buffer.size());

  // Sometimes return an error code;
  if (_fuzz_data_provider->ConsumeBool()) {
    return _fuzz_data_provider->ConsumeIntegralInRange(
        std::numeric_limits<int32_t>::min(), -1);
  }

  return consumed_buffer.size();
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  assert(_fuzz_data_provider.has_value());
  (void)lun;
  return _fuzz_data_provider->ConsumeBool();
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  // Ignore these as they are outputs and don't affect the return value.
  (void)lun;
  (void)lba;
  (void)offset;
  (void)buffer;
  assert(_fuzz_data_provider.has_value());

  // -ve error codes -> bufsize.
  return _fuzz_data_provider->ConsumeIntegralInRange<int32_t>(
      std::numeric_limits<int32_t>::min(), bufsize);
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize) {
  (void)buffer;
  (void)bufsize;
  assert(_fuzz_data_provider.has_value());

  switch (scsi_cmd[0]) {
  case SCSI_CMD_TEST_UNIT_READY:
    break;
  case SCSI_CMD_INQUIRY:
    break;
  case SCSI_CMD_MODE_SELECT_6:
    break;
  case SCSI_CMD_MODE_SENSE_6:
    break;
  case SCSI_CMD_START_STOP_UNIT:
    break;
  case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
    break;
  case SCSI_CMD_READ_CAPACITY_10:
    break;
  case SCSI_CMD_REQUEST_SENSE:
    break;
  case SCSI_CMD_READ_FORMAT_CAPACITY:
    break;
  case SCSI_CMD_READ_10:
    break;
  case SCSI_CMD_WRITE_10:
    break;
  default:
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return _fuzz_data_provider->ConsumeIntegralInRange<int32_t>(
        std::numeric_limits<int32_t>::min(), -1);
  }

  return 0;
}
}

#endif