/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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

#include <string.h>

#include "unity.h"
#include "tusb_option.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "class/audio/audio.h"
#include "class/midi/midi.h"
#include "class/midi/midi2_host.h"

TEST_SOURCE_FILE("midi2_host.c")
TEST_SOURCE_FILE("tusb_fifo.c")

//--------------------------------------------------------------------+
// Descriptors: Audio Control (itf 0) + MIDI Streaming (itf 1) with Alt Settings 0 and 1
//--------------------------------------------------------------------+
enum {
  ITF_AC = 0,
  ITF_MS = 1,
  EP_OUT = 0x01,
  EP_IN  = 0x81,
  EP_SIZE = 64,
  EP_INT_SIZE = 32,
};

#define AC_ITF_DESC \
  9, TUSB_DESC_INTERFACE, ITF_AC, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, 0, 0, \
  9, TUSB_DESC_CS_INTERFACE, 0x01, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(9), 1, ITF_MS

#define MS_ITF_DESC(_alt, _bcd) \
  9, TUSB_DESC_INTERFACE, ITF_MS, _alt, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_MIDI_STREAMING, 0, 0, \
  7, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_HEADER, U16_TO_U8S_LE(_bcd), U16_TO_U8S_LE(7)

// Setting 0: bulk endpoint + CS endpoint with one embedded jack
#define ALT0_EP_DESC(_addr) \
  7, TUSB_DESC_ENDPOINT, _addr, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0, \
  5, TUSB_DESC_CS_ENDPOINT, MIDI_CS_ENDPOINT_GENERAL, 1, 1

// Setting 1: endpoint + CS endpoint General 2.0 with one Group Terminal Block
#define ALT1_EP_DESC(_addr, _type, _size, _interval) \
  7, TUSB_DESC_ENDPOINT, _addr, _type, U16_TO_U8S_LE(_size), _interval, \
  5, TUSB_DESC_CS_ENDPOINT, MIDI_CS_ENDPOINT_GENERAL_2_0, 1, 1

#define MIDI2_DESC(_alt1_out, _alt1_in) \
  AC_ITF_DESC, \
  MS_ITF_DESC(0, 0x0100), ALT0_EP_DESC(EP_OUT), ALT0_EP_DESC(EP_IN), \
  MS_ITF_DESC(1, 0x0200), _alt1_out, _alt1_in

// TinyUSB's own midi2_device: setting 1 reuses setting 0's endpoints
static const uint8_t desc_shared[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(EP_IN, TUSB_XFER_BULK, EP_SIZE, 0))
};

// Linux f_midi2: setting 1 has its own endpoints, here with an Interrupt IN
static const uint8_t desc_own_ep[] = {
  MIDI2_DESC(ALT1_EP_DESC(0x02, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(0x82, TUSB_XFER_INTERRUPT, EP_INT_SIZE, 1))
};

// USB-MIDI 2.0 Appendix B: IN 0x81 is Bulk in setting 0 and Interrupt in setting 1
static const uint8_t desc_same_addr_int[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(EP_IN, TUSB_XFER_INTERRUPT, EP_INT_SIZE, 1))
};

// a setting 1 IN larger than the endpoint buffer
static const uint8_t desc_oversized[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0),
             ALT1_EP_DESC(0x82, TUSB_XFER_INTERRUPT, TUH_EPSIZE_BULK_MAX * 2, 1))
};

// an Audio Class function without MIDI Streaming: Audio Control + Audio Streaming
static const uint8_t desc_audio_only[] = {
  AC_ITF_DESC,
  9, TUSB_DESC_INTERFACE, ITF_MS, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, 0, 0,
};

//--------------------------------------------------------------------+
// Host stack stubs
//--------------------------------------------------------------------+
typedef struct {
  uint8_t daddr;
  uint8_t ep_addr;
  uint8_t type;
  uint16_t mps;
} opened_ep_t;

static opened_ep_t opened[16];
static uint8_t opened_count;
static uint8_t closed[8];
static uint8_t closed_count;
static bool close_supported;

static uint8_t last_xfer_daddr;
static uint8_t last_xfer_ep;
static uint16_t last_xfer_len;
static uint8_t xfer_count;

static bool interface_set_ok;
static uint8_t interface_set_count;
static tuh_xfer_cb_t interface_set_cb;
static uint8_t config_complete_count;
static uint8_t mount_count;
static uint8_t umount_count;
static tuh_midi2_mount_cb_t last_mount;

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  TEST_ASSERT_TRUE(opened_count < TU_ARRAY_SIZE(opened));
  opened[opened_count].daddr = daddr;
  opened[opened_count].ep_addr = desc_ep->bEndpointAddress;
  opened[opened_count].type = desc_ep->bmAttributes.xfer;
  opened[opened_count].mps = tu_edpt_packet_size(desc_ep);
  opened_count++;
  return true;
}

bool tuh_edpt_close(uint8_t daddr, uint8_t ep_addr) {
  (void) daddr;
  TEST_ASSERT_TRUE(closed_count < TU_ARRAY_SIZE(closed));
  if (close_supported) {
    closed[closed_count++] = ep_addr;
  }
  return close_supported;
}

bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt, tuh_xfer_cb_t complete_cb,
                       uintptr_t user_data) {
  (void) daddr; (void) user_data;
  TEST_ASSERT_EQUAL(ITF_MS, itf_num);
  TEST_ASSERT_EQUAL(1, itf_alt);
  interface_set_count++;
  interface_set_cb = interface_set_ok ? complete_cb : NULL;
  return interface_set_ok;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr; (void) itf_num;
  config_complete_count++;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr; (void) ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr; (void) ep_addr;
  return true;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) buffer; (void) complete_cb; (void) user_data;
  last_xfer_daddr = dev_addr;
  last_xfer_ep = ep_addr;
  last_xfer_len = total_bytes;
  xfer_count++;
  return true;
}

// tu_edpt_stream_read_xfer_complete() references it for device streams, a host stream never calls it
void usbd_edpt_rx_consume(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport; (void) ep_addr;
}

// The stream helpers of tusb.c, which this test does not link
bool tu_edpt_stream_init(tu_edpt_stream_t *s, bool is_host, bool is_tx, bool overwritable, void *ff_buf,
                         uint16_t ff_bufsize, uint8_t *ep_buf) {
  (void) is_tx;
  s->is_host = is_host;
  s->ep_buf = ep_buf;
  return tu_fifo_config(&s->ff, ff_buf, ff_bufsize, overwritable);
}

uint32_t tu_edpt_stream_read_xfer(tu_edpt_stream_t *s) {
  TU_VERIFY(s->ep_addr != 0 && s->is_host && s->ep_buf != NULL, 0);
  const uint16_t available = tu_fifo_remaining(&s->ff);
  TU_VERIFY(available >= s->mps, 0);
  const uint16_t count = tu_min16((uint16_t) (available - available % s->mps), s->xfer_len);
  TU_VERIFY(usbh_edpt_xfer(s->hwid, s->ep_addr, s->ep_buf, count), 0);
  return count;
}

uint32_t tu_edpt_stream_read(tu_edpt_stream_t *s, void *buffer, uint32_t bufsize) {
  const uint32_t n = tu_fifo_read_n(&s->ff, buffer, (uint16_t) bufsize);
  tu_edpt_stream_read_xfer(s);
  return n;
}

void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t *mount_cb_data) {
  (void) idx;
  last_mount = *mount_cb_data;
  mount_count++;
}

void tuh_midi2_umount_cb(uint8_t idx) {
  (void) idx;
  umount_count++;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void reset_records(void) {
  opened_count = 0;
  closed_count = 0;
  xfer_count = 0;
  last_xfer_daddr = 0;
  last_xfer_ep = 0;
  last_xfer_len = 0;
  interface_set_count = 0;
  interface_set_cb = NULL;
  config_complete_count = 0;
  mount_count = 0;
  umount_count = 0;
  tu_memclr(&last_mount, sizeof(last_mount));
}

// What usbh does: offer each unclaimed interface, then configure each bound one
static void enumerate(uint8_t daddr, const uint8_t *desc, uint16_t len) {
  const uint16_t alt0_len = midih2_open(0, daddr, (const tusb_desc_interface_t *) desc, len);
  TEST_ASSERT_TRUE(alt0_len > 0 && alt0_len < len);

  const uint8_t *alt1 = desc + alt0_len;
  const uint16_t alt1_len = (uint16_t) (len - alt0_len);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, alt1[1]);
  const uint16_t claimed = midih2_open(0, daddr, (const tusb_desc_interface_t *) alt1, alt1_len);
  TEST_ASSERT_TRUE(claimed == 0 || claimed == alt1_len);

  TEST_ASSERT_TRUE(midih2_set_config(daddr, ITF_AC));
  TEST_ASSERT_TRUE(midih2_set_config(daddr, ITF_MS));

  if (interface_set_cb != NULL) {
    const tusb_control_request_t request = {
      .bmRequestType = 0x01,
      .bRequest = TUSB_REQ_SET_INTERFACE,
      .wValue = 1,
      .wIndex = ITF_MS,
      .wLength = 0
    };
    tuh_xfer_t xfer = {
      .daddr = daddr,
      .ep_addr = 0,
      .result = XFER_RESULT_SUCCESS,
      .setup = &request,
    };
    tuh_xfer_cb_t cb = interface_set_cb;
    interface_set_cb = NULL;
    cb(&xfer);
  }
}

static const opened_ep_t *find_opened(uint8_t ep_addr, uint8_t nth) {
  for (uint8_t i = 0; i < opened_count; i++) {
    if (opened[i].ep_addr == ep_addr && nth-- == 0) {
      return &opened[i];
    }
  }
  return NULL;
}

static void assert_ump_write_goes_to(uint8_t daddr, uint8_t ep_addr) {
  const uint32_t ump = 0x20903C7Fu; // MIDI 1.0 channel voice message, one word
  xfer_count = 0;
  TEST_ASSERT_EQUAL(1, tuh_midi2_ump_write(0, &ump, 1));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL(daddr, last_xfer_daddr);
  TEST_ASSERT_EQUAL_HEX8(ep_addr, last_xfer_ep);
  TEST_ASSERT_EQUAL(4, last_xfer_len);
}

void setUp(void) {
  reset_records();
  close_supported = true;
  interface_set_ok = true;
  TEST_ASSERT_TRUE(midih2_init());
}

void tearDown(void) {
  for (uint8_t daddr = 1; daddr < 4; daddr++) {
    midih2_close(daddr);
  }
}

//--------------------------------------------------------------------+
// UMP Word Count (shared helper, defined in midi.h)
//--------------------------------------------------------------------+

void test_midi2_host_ump_word_count_1word(void) {
  uint8_t types[] = {0x0, 0x1, 0x2, 0x6, 0x7};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(1, midi2_ump_word_count(types[i]));
  }
}

void test_midi2_host_ump_word_count_2word(void) {
  uint8_t types[] = {0x3, 0x4, 0x8, 0x9, 0xA};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(2, midi2_ump_word_count(types[i]));
  }
}

void test_midi2_host_ump_word_count_4word(void) {
  uint8_t types[] = {0x5, 0xD, 0xE, 0xF};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(4, midi2_ump_word_count(types[i]));
  }
}

//--------------------------------------------------------------------+
// Callback struct field validation
//--------------------------------------------------------------------+

void test_midi2_descriptor_cb_struct_fields(void) {
  tuh_midi2_descriptor_cb_t desc = {
    .protocol_version = 1,
    .bcdMSC_hi = 0x02,
    .bcdMSC_lo = 0x00,
    .rx_cable_count = 1,
    .tx_cable_count = 1
  };
  TEST_ASSERT_EQUAL(1, desc.protocol_version);
  TEST_ASSERT_EQUAL(0x02, desc.bcdMSC_hi);
  TEST_ASSERT_EQUAL(0x00, desc.bcdMSC_lo);
  TEST_ASSERT_EQUAL(1, desc.rx_cable_count);
  TEST_ASSERT_EQUAL(1, desc.tx_cable_count);
}

void test_midi2_mount_cb_struct_fields(void) {
  tuh_midi2_mount_cb_t mount = {
    .daddr = 1,
    .bInterfaceNumber = 0,
    .protocol_version = 1,
    .alt_setting_active = 1,
    .rx_cable_count = 2,
    .tx_cable_count = 2
  };
  TEST_ASSERT_EQUAL(1, mount.daddr);
  TEST_ASSERT_EQUAL(0, mount.bInterfaceNumber);
  TEST_ASSERT_EQUAL(1, mount.protocol_version);
  TEST_ASSERT_EQUAL(1, mount.alt_setting_active);
  TEST_ASSERT_EQUAL(2, mount.rx_cable_count);
  TEST_ASSERT_EQUAL(2, mount.tx_cable_count);
}

//--------------------------------------------------------------------+
// CS Endpoint subtypes
//--------------------------------------------------------------------+

void test_midi2_host_cs_endpoint_subtypes(void) {
  TEST_ASSERT_EQUAL(0x01, MIDI_CS_ENDPOINT_GENERAL);
  TEST_ASSERT_EQUAL(0x02, MIDI_CS_ENDPOINT_GENERAL_2_0);
}

//--------------------------------------------------------------------+
// Slot lifecycle
//--------------------------------------------------------------------+

// Setting 1 reusing setting 0's endpoints: nothing more is opened or closed
void test_shared_endpoints_mount_on_alt1(void) {
  enumerate(1, desc_shared, sizeof(desc_shared));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(2, config_complete_count); // Audio Control passed through, then MIDI Streaming
  TEST_ASSERT_EQUAL(1, last_mount.alt_setting_active);
  TEST_ASSERT_TRUE(tuh_midi2_mounted(0));
  TEST_ASSERT_EQUAL(2, opened_count);
  TEST_ASSERT_EQUAL(0, closed_count);

  // RX armed on mount
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// Close keeps the FIFOs and endpoint buffers set up by init: a replug into the same slot works
void test_replug_into_same_slot_keeps_rx_and_tx(void) {
  enumerate(1, desc_shared, sizeof(desc_shared));
  TEST_ASSERT_TRUE(tuh_midi2_mounted(0));

  midih2_close(1);
  TEST_ASSERT_EQUAL(1, umount_count);
  TEST_ASSERT_FALSE(tuh_midi2_mounted(0));

  reset_records();
  enumerate(2, desc_shared, sizeof(desc_shared));
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(2, last_mount.daddr);

  // RX armed on mount
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL(2, last_xfer_daddr);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);

  assert_ump_write_goes_to(2, EP_OUT);
}

// An Audio Class function without MIDI Streaming takes no slot, and is never unmounted
void test_non_midi_audio_interface_does_not_keep_a_slot(void) {
  TEST_ASSERT_EQUAL(0, midih2_open(0, 1, (const tusb_desc_interface_t *) desc_audio_only, sizeof(desc_audio_only)));
  const uint8_t *as_itf = desc_audio_only + 9 + 9;
  TEST_ASSERT_EQUAL(0, midih2_open(0, 1, (const tusb_desc_interface_t *) as_itf,
                                   (uint16_t) (sizeof(desc_audio_only) - 18)));

  // the only slot is still free for a MIDI device on another port
  enumerate(2, desc_shared, sizeof(desc_shared));
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(2, last_mount.daddr);

  midih2_close(1);
  TEST_ASSERT_EQUAL(0, umount_count);
  TEST_ASSERT_TRUE(tuh_midi2_mounted(0));
}

// Without a slot of its own Alt Setting 0, an Alt Setting 1 is not claimed
void test_alt1_without_alt0_slot_is_not_claimed(void) {
  const uint8_t *alt1 = desc_shared + 18 + 9 + 7 + 12 + 12;
  TEST_ASSERT_EQUAL(1, alt1[3]);
  TEST_ASSERT_EQUAL(0, midih2_open(0, 1, (const tusb_desc_interface_t *) alt1,
                                   (uint16_t) (sizeof(desc_shared) - (alt1 - desc_shared))));
  enumerate(2, desc_shared, sizeof(desc_shared));
  TEST_ASSERT_EQUAL(1, mount_count);
}

// SET_INTERFACE not submitted: no callback would continue, so the configuration completes on alt 0
void test_set_interface_not_submitted_completes_on_alt0(void) {
  interface_set_ok = false;
  enumerate(1, desc_shared, sizeof(desc_shared));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(2, config_complete_count); // Audio Control passed through, then MIDI Streaming
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
}

//--------------------------------------------------------------------+
// Setting 1 endpoints
//--------------------------------------------------------------------+

// Setting 1 with endpoints of its own: they are opened and carry the UMP traffic
void test_alt1_own_endpoints_are_used(void) {
  enumerate(1, desc_own_ep, sizeof(desc_own_ep));
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(1, last_mount.alt_setting_active);

  const opened_ep_t *ep_out = find_opened(0x02, 0);
  const opened_ep_t *ep_in = find_opened(0x82, 0);
  TEST_ASSERT_NOT_NULL(ep_out);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_out->type);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, ep_in->type);
  TEST_ASSERT_EQUAL(EP_INT_SIZE, ep_in->mps);
  TEST_ASSERT_EQUAL(0, closed_count);

  // RX armed on setting 1's Interrupt IN, one packet at a time
  TEST_ASSERT_EQUAL_HEX8(0x82, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_INT_SIZE, last_xfer_len);

  assert_ump_write_goes_to(1, 0x02);
}

// Setting 1 IN at setting 0's address but Interrupt: reopened with setting 1's descriptor
void test_alt1_same_address_other_type_is_reopened(void) {
  enumerate(1, desc_same_addr_int, sizeof(desc_same_addr_int));
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(1, last_mount.alt_setting_active);

  TEST_ASSERT_EQUAL(1, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed[0]);
  const opened_ep_t *ep_in = find_opened(EP_IN, 1);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, ep_in->type);
  TEST_ASSERT_EQUAL(EP_INT_SIZE, ep_in->mps);
  TEST_ASSERT_NULL(find_opened(EP_OUT, 1)); // OUT is shared

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_INT_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// An HCD that cannot close an endpoint keeps setting 0's
void test_alt1_reopen_without_hcd_close_keeps_alt0_endpoint(void) {
  close_supported = false;
  enumerate(1, desc_same_addr_int, sizeof(desc_same_addr_int));
  TEST_ASSERT_EQUAL(1, mount_count);

  TEST_ASSERT_NULL(find_opened(EP_IN, 1));
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}

// A setting 1 endpoint larger than the endpoint buffer is refused: the device stays on setting 0
void test_alt1_oversized_endpoint_stays_on_alt0(void) {
  enumerate(1, desc_oversized, sizeof(desc_oversized));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, last_mount.protocol_version);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}
