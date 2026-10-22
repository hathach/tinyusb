#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the production implementation so the harness can enter its private
// enumeration state machine without adding test-only public APIs.
#include "host/usbh.c"

// Synthetic device and controller.
#define TEST_RHPORT                       0u
#define TEST_DEVICE_ADDRESS               1u
#define TEST_CONFIG_COUNT                 2u
#define TEST_FIRST_CONFIG_INDEX           0u
#define TEST_SECOND_CONFIG_INDEX          1u
#define TEST_EP0_PACKET_SIZE              64u

// Application decisions. REJECT_ALL intentionally matches neither valid index.
#define TEST_ACCEPT_FIRST                 TEST_FIRST_CONFIG_INDEX
#define TEST_ACCEPT_SECOND                TEST_SECOND_CONFIG_INDEX
#define TEST_REJECT_ALL                   TEST_CONFIG_COUNT
#define TEST_DECISION_COUNT               3u

// Three-byte input format. Keep these ranges synchronized with regression.c.
#define INPUT_INTERFACES_OFFSET           0u
#define INPUT_CONFIG_VALUE_OFFSET         1u
#define INPUT_DECISION_OFFSET             2u

#define TEST_MIN_INTERFACES                1u
#define TEST_INTERFACE_VARIANTS            4u
#define TEST_SECOND_INTERFACE_INCREMENT    1u

#define TEST_FIRST_CONFIG_VALUE            1u
#define TEST_SECOND_CONFIG_VALUE_MIN       3u
#define TEST_CONFIG_VALUE_VARIANTS \
  (UINT8_MAX - TEST_SECOND_CONFIG_VALUE_MIN + 1u)

#define TEST_DEFAULT_SECOND_CONFIG_VALUE   7u

// Descriptor layout and USB request fields.
#define TEST_CONFIG_HEADER_SIZE \
  ((uint16_t) sizeof(tusb_desc_configuration_t))
#define TEST_INTERFACE_DESC_SIZE \
  ((uint16_t) sizeof(tusb_desc_interface_t))
#define TEST_SETUP_PACKET_SIZE \
  ((uint16_t) sizeof(tusb_control_request_t))

#define TEST_MAX_INTERFACES \
  (TEST_MIN_INTERFACES + TEST_INTERFACE_VARIANTS - 1u + \
   TEST_SECOND_INTERFACE_INCREMENT)

#define TEST_DESCRIPTOR_CAPACITY \
  (TEST_CONFIG_HEADER_SIZE + TEST_INTERFACE_DESC_SIZE * TEST_MAX_INTERFACES)

#define TEST_DESCRIPTOR_TYPE_SHIFT         8u
#define TEST_CONFIG_REQUEST_WINDEX         0u

// Standard requests addressed to the device.
#define TEST_REQUEST_DEVICE_IN             0x80u
#define TEST_REQUEST_DEVICE_OUT            0x00u

#define TEST_EP0_IN                        0x80u
#define TEST_EP0_OUT                       0x00u
#define TEST_NO_DATA_LENGTH                0u

#define TEST_CONFIG_ATTRIBUTES_BUS_POWERED 0x80u
#define TEST_MAX_POWER_2MA_UNITS            50u
#define TEST_NO_STRING                     0u
#define TEST_DEFAULT_ALT_SETTING           0u
#define TEST_NO_ENDPOINTS                  0u
#define TEST_INTERFACE_SUBCLASS            0u
#define TEST_INTERFACE_PROTOCOL            0u

// Each configuration requires a header request followed by a full request.
#define TEST_REQUESTS_PER_CONFIG           2u
#define TEST_HEADER_REQUEST_POSITION       0u

// GET_DESCRIPTOR has SETUP, DATA and STATUS completions.
// SET_CONFIGURATION has SETUP and STATUS completions.
#define TEST_DESCRIPTOR_COMPLETIONS        3u
#define TEST_SET_CONFIG_COMPLETIONS        2u
#define TEST_MAX_COMPLETIONS \
  (TEST_CONFIG_COUNT * TEST_REQUESTS_PER_CONFIG * \
   TEST_DESCRIPTOR_COMPLETIONS + TEST_SET_CONFIG_COMPLETIONS)

// Unlike assert(), these checks remain enabled when NDEBUG is defined.
#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "enumeration check failed at line %d: %s\n",          \
              __LINE__, #condition);                                       \
      abort();                                                             \
    }                                                                      \
  } while (0)

static uint8_t descriptors[TEST_CONFIG_COUNT][TEST_DESCRIPTOR_CAPACITY];
static uint16_t lengths[TEST_CONFIG_COUNT];
static uint8_t values[TEST_CONFIG_COUNT];

static unsigned request_count;
static unsigned callback_count;
static uint8_t accept_index;

// The synthetic controller queues one completion at a time. The test loop
// delivers it later, avoiding recursive completion inside an HCD submission.
static bool pending;
static uint8_t pending_ep;
static uint16_t pending_length;
static tusb_control_request_t current_request;

static unsigned expected_configuration_count(void) {
  return accept_index == TEST_ACCEPT_FIRST ? 1u : TEST_CONFIG_COUNT;
}

static void queue_completion(uint8_t ep, uint16_t length) {
  CHECK(!pending);
  pending = true;
  pending_ep = ep;
  pending_length = length;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr,
                    uint8_t const setup_packet[TEST_SETUP_PACKET_SIZE]) {
  CHECK(rhport == TEST_RHPORT);
  CHECK(daddr == TEST_DEVICE_ADDRESS);

  memcpy(&current_request, setup_packet, sizeof(current_request));

  const unsigned descriptor_requests =
      expected_configuration_count() * TEST_REQUESTS_PER_CONFIG;

  // Both configuration GET_DESCRIPTOR and SET_CONFIGURATION use wIndex = 0.
  CHECK(tu_le16toh(current_request.wIndex) == TEST_CONFIG_REQUEST_WINDEX);

  if (request_count < descriptor_requests) {
    // Derive the expected index from the request sequence, independently of
    // the host's requested index. Otherwise the test could mirror its bug.
    const unsigned index = request_count / TEST_REQUESTS_PER_CONFIG;
    const bool is_header =
        request_count % TEST_REQUESTS_PER_CONFIG ==
        TEST_HEADER_REQUEST_POSITION;

    const uint16_t expected_length =
        is_header ? TEST_CONFIG_HEADER_SIZE : lengths[index];

    const uint16_t expected_value =
        (uint16_t) ((TUSB_DESC_CONFIGURATION <<
                     TEST_DESCRIPTOR_TYPE_SHIFT) | index);

    CHECK(current_request.bmRequestType == TEST_REQUEST_DEVICE_IN);
    CHECK(current_request.bRequest == TUSB_REQ_GET_DESCRIPTOR);

    // Catches retrieving configuration 0 again after requesting index 1's
    // header, including the original wIndex-versus-wValue bug.
    CHECK(tu_le16toh(current_request.wValue) == expected_value);

    // The second configuration has a different total length, so reusing
    // configuration 0's wTotalLength also fails this check.
    CHECK(tu_le16toh(current_request.wLength) == expected_length);
  } else {
    // No SET_CONFIGURATION is allowed when every configuration is rejected.
    CHECK(accept_index < TEST_CONFIG_COUNT);

    // Exactly one SET_CONFIGURATION may follow the descriptor requests.
    CHECK(request_count == descriptor_requests);
    CHECK(current_request.bmRequestType == TEST_REQUEST_DEVICE_OUT);
    CHECK(current_request.bRequest == TUSB_REQ_SET_CONFIGURATION);

    // Catches using config_idx + 1 instead of the accepted descriptor's value.
    CHECK(tu_le16toh(current_request.wValue) == values[accept_index]);
    CHECK(tu_le16toh(current_request.wLength) == TEST_NO_DATA_LENGTH);
  }

  request_count++;
  queue_completion(TEST_EP0_OUT, TEST_SETUP_PACKET_SIZE);
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep,
                   uint8_t *buffer, uint16_t length) {
  CHECK(rhport == TEST_RHPORT);
  CHECK(daddr == TEST_DEVICE_ADDRESS);

  if (length != TEST_NO_DATA_LENGTH) {
    // Supply descriptor data selected by the actual request, just as a device
    // would. The independent request checks above verify that selection.
    CHECK(ep == TEST_EP0_IN && buffer != NULL);
    CHECK(current_request.bRequest == TUSB_REQ_GET_DESCRIPTOR);

    const uint8_t index =
        (uint8_t) tu_le16toh(current_request.wValue);

    CHECK(index < TEST_CONFIG_COUNT);
    CHECK(length <= lengths[index]);
    memcpy(buffer, descriptors[index], length);
  } else {
    // Control STATUS travels opposite to the request's data direction.
    const uint8_t expected_ep =
        current_request.bRequest == TUSB_REQ_GET_DESCRIPTOR
            ? TEST_EP0_OUT : TEST_EP0_IN;

    CHECK(ep == expected_ep);
  }

  queue_completion(ep, length);
  return true;
}

// callbacks.c forwards the application's configuration-selection callback here.
bool enum_test_accept(uint8_t daddr, uint8_t index,
                      const tusb_desc_configuration_t *desc) {
  CHECK(daddr == TEST_DEVICE_ADDRESS);

  // The callback must see configuration indices in order, not always zero.
  CHECK(index < TEST_CONFIG_COUNT);
  CHECK(index == callback_count);

  // Selection happens only after both header and full-descriptor requests.
  CHECK(request_count == TEST_REQUESTS_PER_CONFIG * (index + 1u));

  // Check that the callback receives the complete descriptor for this index,
  // rather than stale data from the previously inspected configuration.
  CHECK(tu_le16toh(desc->wTotalLength) == lengths[index]);
  CHECK(desc->bConfigurationValue == values[index]);
  CHECK(memcmp(desc, descriptors[index], lengths[index]) == 0);

  callback_count++;
  return index == accept_index;
}

// Minimal platform hooks. All transfers complete successfully and immediately
// when pumped by the test, so this harness does not advance a timeout clock.
uint32_t tusb_time_millis_api(void) {
  return 0;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  (void) rhport;
  (void) daddr;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return TUSB_SPEED_FULL;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr,
                   tusb_desc_endpoint_t const *desc) {
  (void) rhport;
  (void) daddr;
  (void) desc;
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep) {
  (void) rhport;
  (void) daddr;
  (void) ep;
  return true;
}

static void make_descriptor(unsigned index, uint8_t interfaces,
                            uint8_t value) {
  uint8_t *desc = descriptors[index];

  lengths[index] = (uint16_t)
      (TEST_CONFIG_HEADER_SIZE + TEST_INTERFACE_DESC_SIZE * interfaces);
  values[index] = value;

  CHECK(lengths[index] <= sizeof(descriptors[index]));
  memset(desc, 0, sizeof(descriptors[index]));

  const tusb_desc_configuration_t header = {
    .bLength = TEST_CONFIG_HEADER_SIZE,
    .bDescriptorType = TUSB_DESC_CONFIGURATION,
    .wTotalLength = tu_htole16(lengths[index]),
    .bNumInterfaces = interfaces,
    .bConfigurationValue = value,
    .iConfiguration = TEST_NO_STRING,
    .bmAttributes = TEST_CONFIG_ATTRIBUTES_BUS_POWERED,
    .bMaxPower = TEST_MAX_POWER_2MA_UNITS
  };

  memcpy(desc, &header, sizeof(header));

  // Vendor-specific interfaces without endpoints keep the fixture focused on
  // configuration selection rather than any particular class driver.
  for (uint8_t i = 0; i < interfaces; i++) {
    const tusb_desc_interface_t interface = {
      .bLength = TEST_INTERFACE_DESC_SIZE,
      .bDescriptorType = TUSB_DESC_INTERFACE,
      .bInterfaceNumber = i,
      .bAlternateSetting = TEST_DEFAULT_ALT_SETTING,
      .bNumEndpoints = TEST_NO_ENDPOINTS,
      .bInterfaceClass = TUSB_CLASS_VENDOR_SPECIFIC,
      .bInterfaceSubClass = TEST_INTERFACE_SUBCLASS,
      .bInterfaceProtocol = TEST_INTERFACE_PROTOCOL,
      .iInterface = TEST_NO_STRING
    };

    memcpy(desc + TEST_CONFIG_HEADER_SIZE + TEST_INTERFACE_DESC_SIZE * i,
           &interface, sizeof(interface));
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Missing bytes use the required regression scenario:
  //   index 0: 18 bytes, rejected
  //   index 1: 27 bytes, accepted, bConfigurationValue = 7
  const uint8_t interfaces =
      size > INPUT_INTERFACES_OFFSET
          ? (uint8_t) (TEST_MIN_INTERFACES +
                       data[INPUT_INTERFACES_OFFSET] %
                           TEST_INTERFACE_VARIANTS)
          : TEST_MIN_INTERFACES;

  const uint8_t value =
      size > INPUT_CONFIG_VALUE_OFFSET
          ? (uint8_t) (TEST_SECOND_CONFIG_VALUE_MIN +
                       data[INPUT_CONFIG_VALUE_OFFSET] %
                           TEST_CONFIG_VALUE_VARIANTS)
          : TEST_DEFAULT_SECOND_CONFIG_VALUE;

  accept_index =
      size > INPUT_DECISION_OFFSET
          ? (uint8_t) (data[INPUT_DECISION_OFFSET] % TEST_DECISION_COUNT)
          : TEST_ACCEPT_SECOND;

  make_descriptor(TEST_FIRST_CONFIG_INDEX, interfaces,
                  TEST_FIRST_CONFIG_VALUE);
  make_descriptor(TEST_SECOND_CONFIG_INDEX,
                  (uint8_t) (interfaces + TEST_SECOND_INTERFACE_INCREMENT),
                  value);

  // Reset state between inputs: libFuzzer invokes this function repeatedly
  // within one process.
  pending = false;
  request_count = 0;
  callback_count = 0;

  memset(&_usbh_data, 0, sizeof(_usbh_data));
  memset(&_usbh_epbuf, 0, sizeof(_usbh_epbuf));

  usbh_device_t *dev = get_device(TEST_DEVICE_ADDRESS);
  clear_device(dev);
  tu_fifo_clear(&_usbh_pending_ctrl_q);

  _usbh_q = osal_queue_create(&_usbh_qdef);
  CHECK(_usbh_q != NULL);

  _usbh_controller_id = TEST_RHPORT;
  _usbh_data.enumerating_daddr = TEST_DEVICE_ADDRESS;
  _usbh_data.root_retry_rhport = TEST_RHPORT;

  // Reject-all should terminate this attempt. Exhaust the retry budget so the
  // harness does not restart unrelated reset/address enumeration stages.
  _usbh_data.root_retry_count = USBH_ENUM_ROOT_RETRY_MAX;

  dev->connected = true;
  dev->addressed = true;
  dev->bus_info.rhport = TEST_RHPORT;
  dev->bus_info.speed = TUSB_SPEED_FULL;
  dev->desc_device.bNumConfigurations = TEST_CONFIG_COUNT;
  dev->desc_device.bMaxPacketSize0 = TEST_EP0_PACKET_SIZE;

  // Start after reset, address assignment and string enumeration.
  tuh_xfer_t start = {
    .daddr = TEST_DEVICE_ADDRESS,
    .result = XFER_RESULT_SUCCESS,
    .user_data = ENUM_GET_9BYTE_CONFIG_DESC
  };
  process_enumeration(&start);

  // Drive real control-transfer completion handling. Bound the number of
  // completions so an enumeration loop becomes a test failure.
  for (unsigned step = 0;
       pending && step < TEST_MAX_COMPLETIONS;
       step++) {
    const uint8_t ep = pending_ep;
    const uint16_t length = pending_length;

    pending = false;
    CHECK(usbh_control_xfer_cb(TEST_DEVICE_ADDRESS, ep,
                              XFER_RESULT_SUCCESS, length));
  }

  const bool accepted = accept_index < TEST_CONFIG_COUNT;
  const unsigned expected_callbacks = expected_configuration_count();
  unsigned expected_requests =
      expected_callbacks * TEST_REQUESTS_PER_CONFIG;

  if (accepted) {
    expected_requests++; // One final SET_CONFIGURATION.
  }

  // Verify both the request sequence and termination. In particular, replacing
  // the reject-all failure path with an early-return assertion leaves the
  // enumeration marker active and fails these checks.
  CHECK(!pending);
  CHECK(_usbh_data.ctrl_xfer_info.stage == CONTROL_STAGE_IDLE);
  CHECK(_usbh_data.enumerating_daddr == TUSB_INDEX_INVALID_8);
  CHECK(_usbh_data.call_after.func == NULL);
  CHECK(callback_count == expected_callbacks);
  CHECK(request_count == expected_requests);
  CHECK((bool) dev->configured == accepted);

  if (!accepted) {
    CHECK(!dev->connected);
  }

  return 0;
}
