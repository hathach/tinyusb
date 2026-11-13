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

// a setting 1 bulk IN that is larger than the endpoint buffer once opened
static const uint8_t desc_oversized_bulk[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0),
             ALT1_EP_DESC(0x82, TUSB_XFER_BULK, TUH_EPSIZE_BULK_MAX * 2, 0))
};

// setting 1 with an OUT of its own, then an IN of a type USB-MIDI 2.0 does not allow
static const uint8_t desc_bad_in_after_own_out[] = {
  MIDI2_DESC(ALT1_EP_DESC(0x02, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(0x82, TUSB_XFER_ISOCHRONOUS, EP_SIZE, 1))
};

// setting 1 shares setting 0's OUT and has no IN
static const uint8_t desc_alt1_no_in[] = {
  AC_ITF_DESC,
  MS_ITF_DESC(0, 0x0100), ALT0_EP_DESC(EP_OUT), ALT0_EP_DESC(EP_IN),
  MS_ITF_DESC(1, 0x0200), ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0)
};

// setting 1 announces USB-MIDI 2.0 but has no endpoint
static const uint8_t desc_alt1_no_ep[] = {
  AC_ITF_DESC,
  MS_ITF_DESC(0, 0x0100), ALT0_EP_DESC(EP_OUT), ALT0_EP_DESC(EP_IN),
  MS_ITF_DESC(1, 0x0200)
};

// USB-MIDI 2.0 Appendix B for both directions: OUT 0x01 and IN 0x81 are Interrupt in setting 1
static const uint8_t desc_same_addr_int_both[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_INTERRUPT, EP_INT_SIZE, 1),
             ALT1_EP_DESC(EP_IN, TUSB_XFER_INTERRUPT, EP_INT_SIZE, 1))
};

// setting 1 with an OUT of its own and an Interrupt IN at setting 0's address
static const uint8_t desc_own_out_same_addr_int_in[] = {
  MIDI2_DESC(ALT1_EP_DESC(0x02, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(EP_IN, TUSB_XFER_INTERRUPT, EP_INT_SIZE, 1))
};

// a setting 1 bulk IN at setting 0's address larger than the endpoint buffer
static const uint8_t desc_same_addr_oversized_bulk[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0),
             ALT1_EP_DESC(EP_IN, TUSB_XFER_BULK, TUH_EPSIZE_BULK_MAX * 2, 0))
};

// a setting 1 bulk IN at setting 0's address reporting 512 on a full-speed device
static const uint8_t desc_same_addr_bulk_512[] = {
  MIDI2_DESC(ALT1_EP_DESC(EP_OUT, TUSB_XFER_BULK, EP_SIZE, 0), ALT1_EP_DESC(EP_IN, TUSB_XFER_BULK, 512, 0))
};

// a 3-byte interface descriptor ahead of the MIDI Streaming one: read as a full one, its
// bInterfaceClass and bInterfaceSubClass would be the next descriptor's ITF_MS (TUSB_CLASS_AUDIO)
// and bAlternateSetting 0 (not MIDI Streaming), so a walk reading them would skip it
static const uint8_t desc_short_itf[] = {
  AC_ITF_DESC,
  3, TUSB_DESC_INTERFACE, 0,
  MS_ITF_DESC(0, 0x0100), ALT0_EP_DESC(EP_OUT), ALT0_EP_DESC(EP_IN),
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
static uint8_t open_refused_ep;   // an open of this endpoint with open_refused_type fails (address 0: none)
static uint8_t open_refused_type;
static tusb_speed_t test_speed;

static uint8_t last_xfer_daddr;
static uint8_t last_xfer_ep;
static uint16_t last_xfer_len;
static uint8_t xfer_count;

static bool interface_set_ok;            // SET_INTERFACE(1) submitted
static xfer_result_t interface_set_result;
static uint8_t interface_set_count;
static uint8_t interface_set_alt[4];     // alternate setting of each request, in order
static tuh_xfer_cb_t interface_set_cb;
static uint8_t config_complete_count;
static uint8_t mount_count;
static uint8_t umount_count;
static tuh_midi2_mount_cb_t last_mount;

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  if (open_refused_ep != 0 && desc_ep->bEndpointAddress == open_refused_ep &&
      desc_ep->bmAttributes.xfer == open_refused_type) {
    return false; // e.g. the HCD has no free endpoint of this type
  }
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
  TEST_ASSERT_EQUAL(1, itf_alt); // the driver only ever switches to setting 1
  TEST_ASSERT_TRUE(interface_set_count < TU_ARRAY_SIZE(interface_set_alt));
  interface_set_alt[interface_set_count++] = itf_alt;
  interface_set_cb = interface_set_ok ? complete_cb : NULL;
  return interface_set_ok;
}

tusb_speed_t tuh_speed_get(uint8_t daddr) {
  (void) daddr;
  return test_speed;
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
  tu_memclr(interface_set_alt, sizeof(interface_set_alt));
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

  // complete the SET_INTERFACE submitted
  if (interface_set_cb != NULL) {
    const uint8_t alt = interface_set_alt[interface_set_count - 1];
    const tusb_control_request_t request = {
      .bmRequestType = 0x01,
      .bRequest = TUSB_REQ_SET_INTERFACE,
      .wValue = alt,
      .wIndex = ITF_MS,
      .wLength = 0
    };
    tuh_xfer_t xfer = {
      .daddr = daddr,
      .ep_addr = 0,
      .result = interface_set_result,
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

static bool was_closed(uint8_t ep_addr) {
  for (uint8_t i = 0; i < closed_count; i++) {
    if (closed[i] == ep_addr) {
      return true;
    }
  }
  return false;
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
  open_refused_ep = 0;
  open_refused_type = 0;
  test_speed = TUSB_SPEED_FULL;
  interface_set_ok = true;
  interface_set_result = XFER_RESULT_SUCCESS;
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

  // setting 0's endpoints are not part of setting 1: closed after the switch
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_TRUE(was_closed(EP_OUT));
  TEST_ASSERT_TRUE(was_closed(EP_IN));

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

// An HCD that cannot close an endpoint cannot reopen it with setting 1's type and size: the device is
// not switched, and setting 0's endpoint stays in use
void test_alt1_reopen_without_hcd_close_keeps_alt0_endpoint(void) {
  close_supported = false;
  enumerate(1, desc_same_addr_int, sizeof(desc_same_addr_int));
  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, tuh_midi2_get_alt_setting_active(0));

  TEST_ASSERT_NULL(find_opened(EP_IN, 1));
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// A setting 1 endpoint larger than the endpoint buffer is refused: the device stays on setting 0
void test_alt1_oversized_endpoint_stays_on_alt0(void) {
  enumerate(1, desc_oversized, sizeof(desc_oversized));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, last_mount.protocol_version);
  TEST_ASSERT_NULL(find_opened(0x82, 0)); // refused before it is opened
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}

// A setting 1 bulk endpoint found too large once opened is closed again: the device stays on setting 0
void test_alt1_oversized_bulk_endpoint_is_closed(void) {
  enumerate(1, desc_oversized_bulk, sizeof(desc_oversized_bulk));

  TEST_ASSERT_NOT_NULL(find_opened(0x82, 0));
  TEST_ASSERT_TRUE(was_closed(0x82));
  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}

//--------------------------------------------------------------------+
// Falling back to setting 0
//--------------------------------------------------------------------+

// A setting 1 rejected after one of its own endpoints was opened closes that endpoint
void test_alt1_rejected_after_own_endpoint_closes_it(void) {
  enumerate(1, desc_bad_in_after_own_out, sizeof(desc_bad_in_after_own_out));

  TEST_ASSERT_NOT_NULL(find_opened(0x02, 0));
  TEST_ASSERT_EQUAL(1, closed_count);
  TEST_ASSERT_TRUE(was_closed(0x02));
  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  assert_ump_write_goes_to(1, EP_OUT);
}

// SET_INTERFACE failing closes setting 1's own endpoints, setting 0's stay in use
void test_set_interface_failed_closes_alt1_own_endpoints(void) {
  interface_set_result = XFER_RESULT_STALLED;
  enumerate(1, desc_own_ep, sizeof(desc_own_ep));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_TRUE(was_closed(0x02));
  TEST_ASSERT_TRUE(was_closed(0x82));

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// SET_INTERFACE not submitted closes setting 1's own endpoints too
void test_set_interface_not_submitted_closes_alt1_own_endpoints(void) {
  interface_set_ok = false;
  enumerate(1, desc_own_ep, sizeof(desc_own_ep));

  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_TRUE(was_closed(0x02));
  TEST_ASSERT_TRUE(was_closed(0x82));
  assert_ump_write_goes_to(1, EP_OUT);
}

// SET_INTERFACE failing after setting 1's endpoint was swapped in for setting 0's restores setting 0's
void test_set_interface_failed_reopens_alt0_endpoint(void) {
  interface_set_result = XFER_RESULT_STALLED;
  enumerate(1, desc_same_addr_int, sizeof(desc_same_addr_int));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);

  // setting 1's Interrupt IN swapped in before the request, swapped out again for setting 0's Bulk IN
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed[0]);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed[1]);
  const opened_ep_t *ep_in_alt1 = find_opened(EP_IN, 1);
  TEST_ASSERT_NOT_NULL(ep_in_alt1);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, ep_in_alt1->type);
  const opened_ep_t *ep_in = find_opened(EP_IN, 2);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_in->type);
  TEST_ASSERT_EQUAL(EP_SIZE, ep_in->mps);
  TEST_ASSERT_NULL(find_opened(EP_IN, 3));

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}

// A direction setting 1 does not have is not used on setting 1: no RX is armed on setting 0's IN,
// which is closed
void test_alt1_without_in_closes_alt0_in(void) {
  enumerate(1, desc_alt1_no_in, sizeof(desc_alt1_no_in));

  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(1, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(1, closed_count);
  TEST_ASSERT_TRUE(was_closed(EP_IN));
  TEST_ASSERT_EQUAL(0, xfer_count); // nothing armed on mount

  assert_ump_write_goes_to(1, EP_OUT); // shared OUT kept
}

//--------------------------------------------------------------------+
// Setting 1 endpoint at setting 0's address
//--------------------------------------------------------------------+

// One the HCD refuses to open: as Linux's usb_set_interface(), which takes the new setting's resources
// before its request, SET_INTERFACE(1) is never sent and setting 0's endpoint is reopened
void test_alt1_swap_refused_stays_on_alt0(void) {
  open_refused_ep = EP_IN;
  open_refused_type = TUSB_XFER_INTERRUPT;
  enumerate(1, desc_same_addr_int, sizeof(desc_same_addr_int));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(2, config_complete_count); // Audio Control passed through, then MIDI Streaming
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, tuh_midi2_get_alt_setting_active(0));

  // closed for the swap, then reopened as setting 0's Bulk IN
  TEST_ASSERT_EQUAL(1, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed[0]);
  const opened_ep_t *ep_in = find_opened(EP_IN, 1);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_in->type);
  TEST_ASSERT_EQUAL(EP_SIZE, ep_in->mps);
  TEST_ASSERT_NULL(find_opened(EP_IN, 2));

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// The other direction, already swapped in, is swapped back to setting 0's
void test_alt1_swap_refused_undoes_other_direction(void) {
  open_refused_ep = EP_IN;
  open_refused_type = TUSB_XFER_INTERRUPT;
  enumerate(1, desc_same_addr_int_both, sizeof(desc_same_addr_int_both));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);

  // OUT swapped in, IN refused, then setting 1's OUT closed again
  TEST_ASSERT_EQUAL(3, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, closed[0]);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed[1]);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, closed[2]);

  const opened_ep_t *ep_out_alt1 = find_opened(EP_OUT, 1);
  TEST_ASSERT_NOT_NULL(ep_out_alt1);
  TEST_ASSERT_EQUAL(TUSB_XFER_INTERRUPT, ep_out_alt1->type);
  const opened_ep_t *ep_out_alt0 = find_opened(EP_OUT, 2);
  TEST_ASSERT_NOT_NULL(ep_out_alt0);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_out_alt0->type);
  TEST_ASSERT_EQUAL(EP_SIZE, ep_out_alt0->mps);
  const opened_ep_t *ep_in_alt0 = find_opened(EP_IN, 1);
  TEST_ASSERT_NOT_NULL(ep_in_alt0);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_in_alt0->type);

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// Setting 1's own endpoint is closed too, setting 0's of that direction stays in use
void test_alt1_swap_refused_closes_alt1_own_endpoint(void) {
  open_refused_ep = EP_IN;
  open_refused_type = TUSB_XFER_INTERRUPT;
  enumerate(1, desc_own_out_same_addr_int_in, sizeof(desc_own_out_same_addr_int_in));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_NOT_NULL(find_opened(0x02, 0));
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_TRUE(was_closed(EP_IN));
  TEST_ASSERT_TRUE(was_closed(0x02));
  TEST_ASSERT_FALSE(was_closed(EP_OUT));

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// SET_INTERFACE failing swaps both directions back to setting 0's endpoints
void test_set_interface_failed_swaps_both_directions_back(void) {
  interface_set_result = XFER_RESULT_STALLED;
  enumerate(1, desc_same_addr_int_both, sizeof(desc_same_addr_int_both));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);

  // setting 0's closed for the swap, setting 1's closed after the failure
  TEST_ASSERT_EQUAL(4, closed_count);
  const opened_ep_t *ep_out = find_opened(EP_OUT, 2);
  const opened_ep_t *ep_in = find_opened(EP_IN, 2);
  TEST_ASSERT_NOT_NULL(ep_out);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_out->type);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_in->type);

  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
  assert_ump_write_goes_to(1, EP_OUT);
}

// As Linux's USB-MIDI 2.0 driver: a USB-MIDI 2.0 setting 1 without endpoints is not used
void test_alt1_without_endpoints_stays_on_midi1(void) {
  enumerate(1, desc_alt1_no_ep, sizeof(desc_alt1_no_ep));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, last_mount.protocol_version);
  TEST_ASSERT_EQUAL(0, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  assert_ump_write_goes_to(1, EP_OUT);
}

// One larger than the endpoint buffer is refused while parsing: the device is never switched
void test_alt1_reopen_oversized_bulk_stays_on_alt0(void) {
  test_speed = TUSB_SPEED_HIGH;
  enumerate(1, desc_same_addr_oversized_bulk, sizeof(desc_same_addr_oversized_bulk));

  TEST_ASSERT_EQUAL(0, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(0, last_mount.alt_setting_active);
  TEST_ASSERT_EQUAL(0, closed_count);
  TEST_ASSERT_NULL(find_opened(EP_IN, 1));
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(EP_SIZE, last_xfer_len);
}

// A full-speed bulk size of 512 gets tuh_edpt_open()'s rewrite to 64 while parsing, and is used
void test_alt1_reopen_full_speed_bulk_512_is_used_as_64(void) {
  enumerate(1, desc_same_addr_bulk_512, sizeof(desc_same_addr_bulk_512));

  TEST_ASSERT_EQUAL(1, interface_set_count);
  TEST_ASSERT_EQUAL(1, mount_count);
  TEST_ASSERT_EQUAL(1, last_mount.alt_setting_active);
  const opened_ep_t *ep_in = find_opened(EP_IN, 1);
  TEST_ASSERT_NOT_NULL(ep_in);
  TEST_ASSERT_EQUAL(TUSB_XFER_BULK, ep_in->type);
  TEST_ASSERT_EQUAL(64, ep_in->mps);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, last_xfer_ep);
  TEST_ASSERT_EQUAL(64, last_xfer_len);
}

//--------------------------------------------------------------------+
// Malformed descriptors
//--------------------------------------------------------------------+

// A short interface descriptor ends the walk past Audio Control and is rejected, not read past its bLength
void test_short_interface_after_audio_control_is_rejected(void) {
  TEST_ASSERT_EQUAL(TUSB_CLASS_AUDIO, ITF_MS);
  TEST_ASSERT_EQUAL(0, midih2_open(0, 1, (const tusb_desc_interface_t *) desc_short_itf, sizeof(desc_short_itf)));
  TEST_ASSERT_EQUAL(0, opened_count);
}
