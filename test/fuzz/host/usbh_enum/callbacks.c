#include "tusb.h"

// Kept in a separate translation unit to override usbh.c's weak callback.
extern bool enum_test_accept(uint8_t daddr, uint8_t index, const tusb_desc_configuration_t *desc);

bool tuh_enum_descriptor_configuration_cb(uint8_t daddr, uint8_t index,
                                          const tusb_desc_configuration_t *desc) {
  return enum_test_accept(daddr, index, desc);
}
