#ifndef USBH_ENUM_INPUT_FORMAT_H_
#define USBH_ENUM_INPUT_FORMAT_H_

#include <stdint.h>

// Three-byte input format: fuzz.c decodes it, regression.c enumerates every encoded value.
#define INPUT_INTERFACES_OFFSET           0u
#define INPUT_CONFIG_VALUE_OFFSET         1u
#define INPUT_DECISION_OFFSET             2u
#define TEST_INPUT_SIZE                   3u

#define TEST_INTERFACE_VARIANTS           4u
#define TEST_SECOND_CONFIG_VALUE_MIN      3u
#define TEST_CONFIG_VALUE_VARIANTS \
  (UINT8_MAX - TEST_SECOND_CONFIG_VALUE_MIN + 1u)
#define TEST_DECISION_COUNT               3u

#endif
