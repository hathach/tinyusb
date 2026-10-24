/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Nathan Conrad
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/*
 * This library is not fully reentrant, though it is reentrant from the view
 * of either the application layer or the USB stack. Due to its locking,
 * it is not safe to call its functions from interrupts.
 *
 * The one exception is that its functions may not be called from the application
 * until the USB stack is initialized. This should not be a problem since the
 * device shouldn't be sending messages until it receives a request from the
 * host.
 */


/*
 * In the case of single-CPU "no OS", this task is never preempted other than by
 * interrupts, and the USBTMC code isn't called by interrupts, so all is OK.
 * With an OS, this class driver uses the OSAL to perform locking. The code uses a single lock
 * and does not call outside of this class with a lock held, so deadlocks won't happen.
 */

//Limitations:
// "vendor-specific" commands are handled similar to normal messages, except that the MsgID is changed to "vendor-specific".
// Dealing with "termchar" must be handled by the application layer,
//    though additional error checking is does in this module.
// talkOnly and listenOnly are NOT supported. They're not permitted
// in USB488, anyway.

/* Supported:
 *
 * Notification pulse
 * Trigger
 * Read status byte (both by interrupt endpoint and control message)
 *
 */


// TODO:
// No local lock-out, REN, or GTL.
// Clear message available status byte at the correct time? (488 4.3.1.3)
// Ability to defer status byte transmission
// Transmission of status byte in response to USB488 SRQ condition

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_USBTMC)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "usbtmc_device.h"

// Buffer size must be an exact multiple of the max packet size for both
// bulk  (up to 64 bytes for FS, 512 bytes for HS). In addation, this driver
// imposes a minimum buffer size of 32 bytes.
#define USBTMCD_BUFFER_SIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

// Interrupt endpoint buffer size, default to 2 bytes as USB488 specification.
#ifndef CFG_TUD_USBTMC_INT_EP_SIZE
  #define CFG_TUD_USBTMC_INT_EP_SIZE 2
#endif

/*
 * The state machine does not allow simultaneous reading and writing. This is
 * consistent with USBTMC.
 */

typedef enum {
  STATE_CLOSED,// Endpoints have not yet been opened since USB reset
  STATE_NAK,   // Bulk-out endpoint is in NAK state.
  STATE_IDLE,  // Bulk-out endpoint is waiting for CMD.
  STATE_RCV,   // Bulk-out is receiving DEV_DEP message
  STATE_TX_REQUESTED,
  STATE_TX_INITIATED,
  STATE_TX_SHORTED,
  STATE_CLEARING,
  STATE_ABORTING_BULK_IN,
  STATE_ABORTING_BULK_IN_SHORTED,// aborting, and short packet has been queued for transmission
  STATE_ABORTING_BULK_IN_ABORTED,// aborting, and short packet has been transmitted
  STATE_ABORTING_BULK_OUT,
  STATE_NUM_STATES
} usbtmcd_state_enum;

// The INITIATE request whose CHECK_STATUS is expected (USBTMC 1.0 4.2.1.1)
typedef enum {
  SPLIT_NONE,
  SPLIT_ABORT_BULK_OUT,
  SPLIT_ABORT_BULK_IN,
  SPLIT_CLEAR,
} usbtmcd_split_enum;

#if (CFG_TUD_USBTMC_ENABLE_488)
typedef usbtmc_response_capabilities_488_t usbtmc_capabilities_specific_t;
#else
typedef usbtmc_response_capabilities_t usbtmc_capabilities_specific_t;
#endif


typedef struct
{
  volatile usbtmcd_state_enum state;
  usbtmcd_split_enum split;

  uint8_t itf_id;
  uint8_t rhport;
  uint8_t ep_bulk_in;
  uint8_t ep_bulk_out;
  uint8_t ep_int_in;
  uint32_t ep_bulk_in_mps;
  uint32_t ep_bulk_out_mps;
  uint32_t transfer_size_remaining;// also used for requested length for bulk IN.
  uint32_t transfer_size_sent;     // message data bytes received (Bulk-OUT) or sent to the host (Bulk-IN, not header bytes)
  uint32_t in_flight;              // message data bytes in the Bulk-IN packet queued, counted as sent once it completes

  uint8_t last_bulk_out_tag;// used for aborts (mostly)
  uint8_t last_bulk_in_tag; // used for aborts (mostly)

  uint8_t const *dev_in_buffer;// pointer to application-layer used for transmissions

  usbtmc_capabilities_specific_t const *capabilities;
} usbtmc_interface_state_t;

typedef struct {
  // IN buffer is only used for first packet, not the remainder in order to deal with prepending header
  TUD_EPBUF_DEF(epin, USBTMCD_BUFFER_SIZE);

  // OUT buffer receives one packet at a time
  TUD_EPBUF_DEF(epout, USBTMCD_BUFFER_SIZE);

  // Buffer int msg
  TUD_EPBUF_DEF(epnotif, CFG_TUD_USBTMC_INT_EP_SIZE);
} usbtmc_epbuf_t;

static usbtmc_interface_state_t usbtmc_state = {
    .itf_id = 0xFF,
};

CFG_TUD_MEM_SECTION static usbtmc_epbuf_t usbtmc_epbuf;

// We need all headers to fit in a single packet in this implementation, 32 bytes will fit all standard USBTMC headers
TU_VERIFY_STATIC(USBTMCD_BUFFER_SIZE >= 32u, "USBTMC dev buffer size too small");
TU_VERIFY_STATIC(CFG_TUD_USBTMC_INT_EP_SIZE >= 2u, "USBTMC interrupt buffer size too small");

static bool handle_dev_msg_out_start(uint8_t rhport, void *data, size_t len);
static bool handle_dev_msg_out(uint8_t rhport, void *data, size_t len, size_t packet_len);


// USBTMC Device Callbacks weak implementations
TU_ATTR_WEAK bool tud_usbtmc_notification_complete_cb(void) {
  return true;
}

TU_ATTR_WEAK bool tud_usbtmc_indicator_pulse_cb(tusb_control_request_t const * msg, uint8_t *tmcResult) {
  (void) msg;
  (void) tmcResult;
  return true;
}

#if (CFG_TUD_USBTMC_ENABLE_488)
TU_ATTR_WEAK bool tud_usbtmc_msg_trigger_cb(usbtmc_msg_generic_t* msg) {
  (void) msg;
  return true;
}
#endif

#ifndef NDEBUG
tu_static uint8_t term_char;
#endif

tu_static uint8_t term_char_requested = false;

tu_static bool vendor_specific_requested = false;

#if OSAL_MUTEX_REQUIRED
static OSAL_MUTEX_DEF(usbtmc_mutex_def);
#endif
static osal_mutex_t usbtmc_mutex;

// The lock guards usbtmc_state.state, which the application changes too (tud_usbtmc_transmit_dev_msg_data(),
// tud_usbtmc_start_bus_read()). Every write goes through change_state() or a block between
// usbtmc_lock() and usbtmc_unlock(). A state change and the endpoint transfer or stall it implies are made
// in one locked block, so the other task never sees one without the other. usbd's endpoint functions take
// no lock of ours; application callbacks are never called with the lock held.
TU_ATTR_ALWAYS_INLINE static inline void usbtmc_lock(void) {
  (void) osal_mutex_lock(usbtmc_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
}

TU_ATTR_ALWAYS_INLINE static inline void usbtmc_unlock(void) {
  (void) osal_mutex_unlock(usbtmc_mutex);
}

// Move from expected to new_state, false if the state is no longer expected
static bool change_state(usbtmcd_state_enum expected, usbtmcd_state_enum new_state) {
  usbtmc_lock();
  bool const ret = (usbtmc_state.state == expected);
  if (ret) {
    usbtmc_state.state = new_state;
  }
  usbtmc_unlock();
  return ret;
}

// called from app
// We keep a reference to the buffer, so it MUST not change until the app is
// notified that the transfer is complete.
// length of data is specified in the hdr.

// We can't just send the whole thing at once because we need to concatanate the
// header with the data.
bool tud_usbtmc_transmit_dev_msg_data(
    const void *data, size_t len,
    bool endOfMessage,
    bool usingTermChar) {
  const unsigned int tx_buf_len = USBTMCD_BUFFER_SIZE;

#ifndef NDEBUG
  TU_ASSERT(len > 0u);
  TU_ASSERT(len <= usbtmc_state.transfer_size_remaining);
  TU_ASSERT(usbtmc_state.transfer_size_sent == 0u);
  if (usingTermChar) {
    TU_ASSERT(usbtmc_state.capabilities->bmDevCapabilities.canEndBulkInOnTermChar);
    TU_ASSERT(term_char_requested);
    TU_ASSERT(((uint8_t const *) data)[len - 1u] == term_char);
  }
#endif

  TU_VERIFY(usbtmc_state.state == STATE_TX_REQUESTED);
  usbtmc_msg_dev_dep_msg_in_header_t *hdr = (usbtmc_msg_dev_dep_msg_in_header_t *) usbtmc_epbuf.epin;
  tu_varclr(hdr);
  if (vendor_specific_requested) {
    hdr->header.MsgID = USBTMC_MSGID_VENDOR_SPECIFIC_IN;
  } else {
    hdr->header.MsgID = USBTMC_MSGID_DEV_DEP_MSG_IN;
  }
  hdr->header.bTag = usbtmc_state.last_bulk_in_tag;
  hdr->header.bTagInverse = (uint8_t) ~(usbtmc_state.last_bulk_in_tag);
  hdr->TransferSize = len;
  hdr->bmTransferAttributes.EOM = endOfMessage;
  hdr->bmTransferAttributes.UsingTermChar = usingTermChar;

  // Copy in the header
  const size_t header_len = sizeof(*hdr);
  const size_t data_len = ((header_len + hdr->TransferSize) <= tx_buf_len) ? len : (tx_buf_len - header_len);
  const size_t packet_len = header_len + data_len;

  memcpy((uint8_t *) (usbtmc_epbuf.epin) + header_len, data, data_len);

  // The state change and the transfer are one step against an abort or a clear on the USB task.
  // A full packet doesn't terminate the transfer, a short one must follow (USBTMC 1.0 3.3 rule 10). The buffer
  // can hold several packets, e.g. a high-speed capable device enumerated at full speed.
  usbtmc_lock();
  bool ret = (usbtmc_state.state == STATE_TX_REQUESTED);
  if (ret) {
    usbtmc_state.transfer_size_remaining = len - data_len;
    usbtmc_state.in_flight = data_len;
    usbtmc_state.dev_in_buffer = (uint8_t const *) data + data_len;
    usbtmc_state.state = ((packet_len % usbtmc_state.ep_bulk_in_mps) == 0) ? STATE_TX_INITIATED : STATE_TX_SHORTED;
    ret = usbd_edpt_xfer(usbtmc_state.rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, (uint16_t) packet_len, false);
    if (!ret) {
      usbtmc_state.state = STATE_TX_REQUESTED;
    }
  }
  usbtmc_unlock();
  return ret;
}

bool tud_usbtmc_transmit_notification_data(const void *data, size_t len) {
#ifndef NDEBUG
  TU_ASSERT(len > 0);
#endif
  // No interrupt endpoint, or not configured yet / after a bus reset: never claim endpoint 0x00
  TU_VERIFY(usbtmc_state.ep_int_in != 0);
  TU_VERIFY(usbd_edpt_claim(usbtmc_state.rhport, usbtmc_state.ep_int_in));

  if (tu_memcpy_s(usbtmc_epbuf.epnotif, CFG_TUD_USBTMC_INT_EP_SIZE, data, len) != 0) {
    usbd_edpt_release(usbtmc_state.rhport, usbtmc_state.ep_int_in);
    return false;
  }
  TU_VERIFY(usbd_edpt_xfer(usbtmc_state.rhport, usbtmc_state.ep_int_in, usbtmc_epbuf.epnotif, (uint16_t) len, false));
  return true;
}

void usbtmcd_init_cb(void) {
  usbtmc_state.capabilities = tud_usbtmc_get_capabilities_cb();
#ifndef NDEBUG
  #if CFG_TUD_USBTMC_ENABLE_488
  // Per USB488 spec: table 8
  TU_ASSERT(!usbtmc_state.capabilities->bmIntfcCapabilities.listenOnly, );
  TU_ASSERT(!usbtmc_state.capabilities->bmIntfcCapabilities.talkOnly, );
  #endif
#endif

  usbtmc_mutex = osal_mutex_create(&usbtmc_mutex_def);
}

bool usbtmcd_deinit(void) {
#if OSAL_MUTEX_REQUIRED
  osal_mutex_delete(usbtmc_mutex);
#endif
  return true;
}

uint16_t usbtmcd_open_cb(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) rhport;

  uint16_t drv_len;
  uint8_t const *p_desc;
  uint8_t found_endpoints = 0;

  TU_VERIFY(itf_desc->bInterfaceClass == TUD_USBTMC_APP_CLASS, 0);
  TU_VERIFY(itf_desc->bInterfaceSubClass == TUD_USBTMC_APP_SUBCLASS, 0);

#ifndef NDEBUG
  // Only 2 or 3 endpoints are allowed for USBTMC.
  TU_ASSERT((itf_desc->bNumEndpoints == 2) || (itf_desc->bNumEndpoints == 3), 0);
#endif

  TU_ASSERT(usbtmc_state.state == STATE_CLOSED, 0);

  // Interface
  drv_len = 0u;
  p_desc = (uint8_t const *) itf_desc;

  usbtmc_state.itf_id = itf_desc->bInterfaceNumber;
  usbtmc_state.rhport = rhport;

  while (found_endpoints < itf_desc->bNumEndpoints && drv_len < max_len) {
    if (TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE]) {
      tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *) p_desc;
      switch (ep_desc->bmAttributes.xfer) {
        case TUSB_XFER_BULK:
          // Ensure  buffer is an exact multiple of the maxPacketSize
          TU_ASSERT((USBTMCD_BUFFER_SIZE % tu_edpt_packet_size(ep_desc)) == 0, 0);
          if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
            usbtmc_state.ep_bulk_in = ep_desc->bEndpointAddress;
            usbtmc_state.ep_bulk_in_mps = tu_edpt_packet_size(ep_desc);
          } else {
            usbtmc_state.ep_bulk_out = ep_desc->bEndpointAddress;
            usbtmc_state.ep_bulk_out_mps = tu_edpt_packet_size(ep_desc);
          }

          break;
        case TUSB_XFER_INTERRUPT:
          TU_ASSERT(tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN, 0);
          TU_ASSERT(usbtmc_state.ep_int_in == 0, 0);
          usbtmc_state.ep_int_in = ep_desc->bEndpointAddress;
          break;
        default:
          TU_ASSERT(false, 0);
      }
      TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
      found_endpoints++;
    }

    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  // bulk endpoints are required, but interrupt IN is optional
  TU_ASSERT(usbtmc_state.ep_bulk_in != 0, 0);
  TU_ASSERT(usbtmc_state.ep_bulk_out != 0, 0);
#ifndef NDEBUG
  if (itf_desc->bNumEndpoints == 2) {
    TU_ASSERT(usbtmc_state.ep_int_in == 0, 0);
  } else if (itf_desc->bNumEndpoints == 3) {
    TU_ASSERT(usbtmc_state.ep_int_in != 0, 0);
  }
  #if (CFG_TUD_USBTMC_ENABLE_488)
  if (usbtmc_state.capabilities->bmIntfcCapabilities488.is488_2 ||
      usbtmc_state.capabilities->bmDevCapabilities488.SR1) {
    TU_ASSERT(usbtmc_state.ep_int_in != 0, 0);
  }
  #endif
#endif
  change_state(STATE_CLOSED, STATE_NAK);
  tud_usbtmc_open_cb(itf_desc->bInterfaceNumber);

  return drv_len;
}
// Tell USBTMC class to set its bulk-in EP to ACK so that it can
// receive USBTMC commands.
// Returns false if it was already in an ACK state or is busy
// processing a command (such as a clear). Returns true if it was
// in the NAK state and successfully transitioned to the ACK wait
// state.
bool tud_usbtmc_start_bus_read(void) {
  bool ret = false;
  usbtmc_lock();
  usbtmcd_state_enum const old_state = usbtmc_state.state;
  switch (old_state) {
    // These may transition to IDLE
    case STATE_NAK:
    case STATE_ABORTING_BULK_IN_ABORTED:
    // When receiving, let it remain receiving
    case STATE_RCV:
      // Already armed (or Halted) is the "already in an ACK state" above, not a reason to assert in usbd
      if (!usbd_edpt_busy(usbtmc_state.rhport, usbtmc_state.ep_bulk_out)) {
        // IDLE before the transfer: its completion may run on the USB task as soon as it is queued
        usbtmc_state.state = (old_state == STATE_RCV) ? STATE_RCV : STATE_IDLE;
        ret = usbd_edpt_xfer(usbtmc_state.rhport, usbtmc_state.ep_bulk_out, usbtmc_epbuf.epout,
                             (uint16_t) usbtmc_state.ep_bulk_out_mps, false);
        if (!ret) {
          usbtmc_state.state = old_state;
        }
      }
      break;
    default:
      break;
  }
  usbtmc_unlock();
  return ret;
}

void usbtmcd_reset_cb(uint8_t rhport) {
  (void) rhport;
  usbtmc_capabilities_specific_t const *capabilities = tud_usbtmc_get_capabilities_cb();

  usbtmc_lock();
  tu_varclr(&usbtmc_state);
  usbtmc_state.capabilities = capabilities;
  usbtmc_state.itf_id = 0xFFu;
  usbtmc_unlock();
}

static bool handle_dev_msg_out_start(uint8_t rhport, void *data, size_t len) {
  // must be a header, should have been confirmed before calling here.
  usbtmc_msg_request_dev_dep_out *msg = (usbtmc_msg_request_dev_dep_out *) data;
  TU_VERIFY(msg->TransferSize > 0u); // USBTMC 1.0 Tables 3 and 5

  // return true upon failure, as we can assume error is being handled elsewhere.
  TU_VERIFY(change_state(STATE_IDLE, STATE_RCV), true);
  usbtmc_state.transfer_size_sent = 0u;
  usbtmc_state.transfer_size_remaining = msg->TransferSize;
  TU_VERIFY(tud_usbtmc_msgBulkOut_start_cb(msg));

  TU_VERIFY(handle_dev_msg_out(rhport, (uint8_t *) data + sizeof(*msg), len - sizeof(*msg), len));
  return true;
}

static bool handle_dev_msg_out(uint8_t rhport, void *data, size_t len, size_t packet_len) {
  // return true upon failure, as we can assume error is being handled elsewhere.
  TU_VERIFY(usbtmc_state.state == STATE_RCV, true);

  bool short_packet = (packet_len < usbtmc_state.ep_bulk_out_mps);

  // Packet is to be considered complete when we get enough data or at a short packet.
  bool at_end = false;
  if (len >= usbtmc_state.transfer_size_remaining || short_packet) {
    at_end = true;
    // Message data starts 4-byte aligned in every packet, so up to 3 alignment bytes may follow the last data byte.
    // A transfer ending short of TransferSize, or carrying more than the data and alignment bytes, must Halt
    // bulk-OUT, the received bytes are still forwarded (USBTMC 1.0 Table 7 indexes 4 and 6).
    uint32_t const alignment = (4u - (usbtmc_state.transfer_size_remaining & 3u)) & 3u;
    if ((len < usbtmc_state.transfer_size_remaining) ||
        (len - usbtmc_state.transfer_size_remaining > alignment)) {
      // Halted until CLEAR_FEATURE as for an abort, tud_usbtmc_start_bus_read() refuses to arm until then
      usbtmc_lock();
      bool const in_rcv = (usbtmc_state.state == STATE_RCV);
      if (in_rcv) {
        usbtmc_state.state = STATE_ABORTING_BULK_OUT;
        usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
      }
      usbtmc_unlock();
      TU_VERIFY(in_rcv);
    } else {
      TU_VERIFY(change_state(STATE_RCV, STATE_NAK));
    }
  }

  len = tu_min32(len, usbtmc_state.transfer_size_remaining);

  usbtmc_state.transfer_size_remaining -= len;
  usbtmc_state.transfer_size_sent += len;

  // App may (should?) call the wait_for_bus() command at this point
  if (!tud_usbtmc_msg_data_cb(data, len, at_end)) {
    // TODO: Go to an error state upon failure other than just stalling the EP?
    return false;
  }


  return true;
}

static bool handle_dev_msg_in(void *data, size_t len) {
  TU_VERIFY(len == sizeof(usbtmc_msg_request_dev_dep_in));
  usbtmc_msg_request_dev_dep_in *msg = (usbtmc_msg_request_dev_dep_in *) data;
  TU_VERIFY(msg->TransferSize > 0u); // USBTMC 1.0 Tables 4 and 6
  TU_VERIFY(!msg->bmTransferAttributes.TermCharEnabled ||
            usbtmc_state.capabilities->bmDevCapabilities.canEndBulkInOnTermChar);

  bool state_changed = change_state(STATE_IDLE, STATE_TX_REQUESTED);
  TU_VERIFY(state_changed);
  usbtmc_state.last_bulk_in_tag = msg->header.bTag;
  usbtmc_state.transfer_size_remaining = msg->TransferSize;
  usbtmc_state.transfer_size_sent = 0u;

  term_char_requested = msg->bmTransferAttributes.TermCharEnabled;

#ifndef NDEBUG
  term_char = msg->TermChar;
#endif

  TU_VERIFY(tud_usbtmc_msgBulkIn_request_cb(msg));
  return true;
}

// First packet of a Bulk-OUT transfer. Returning false Halts bulk-OUT, as a Table 7 error requires (USBTMC 1.0 3.2.2.4)
static bool handle_msg_out_header(uint8_t rhport, usbtmc_msg_generic_t *msg, uint32_t len) {
  TU_VERIFY(len >= sizeof(usbtmc_msg_generic_t));
  TU_VERIFY(msg->header.bTag == (uint8_t) ~(msg->header.bTagInverse));
  TU_VERIFY(msg->header.bTag != 0x00);

  // Reported by INITIATE_ABORT_BULK_OUT as the current or most recent Bulk-OUT transfer (USBTMC 1.0 Table 19)
  usbtmc_state.last_bulk_out_tag = msg->header.bTag;

  switch (msg->header.MsgID) {
    case USBTMC_MSGID_DEV_DEP_MSG_OUT:
      vendor_specific_requested = false;
      return handle_dev_msg_out_start(rhport, msg, len);

    case USBTMC_MSGID_DEV_DEP_MSG_IN:
      vendor_specific_requested = false;
      return handle_dev_msg_in(msg, len);

#if (CFG_TUD_USBTMC_ENABLE_488)
    case USBTMC_MSGID_USB488_TRIGGER:
      // Unlike the messages above, TRIGGER is complete on arrival and has no response, so nothing else
      // will move us out of STATE_IDLE. Do it here, otherwise the tud_usbtmc_start_bus_read() below (and
      // any call the application makes from its callback) is a no-op and the bulk-OUT endpoint is left
      // un-armed, silently timing out every subsequent host transfer.
      TU_VERIFY(change_state(STATE_IDLE, STATE_NAK));

      // Spec says we halt the EP if we didn't declare we support it; do the same when the application
      // rejects the trigger.
      TU_VERIFY(usbtmc_state.capabilities->bmIntfcCapabilities488.supportsTrigger);
      TU_VERIFY(tud_usbtmc_msg_trigger_cb(msg));
      // Result deliberately ignored: false here means the endpoint is already armed - either the
      // application re-armed it from its callback, or a transfer is still queued - not that arming
      // failed. Stalling on it would halt a healthy endpoint.
      tud_usbtmc_start_bus_read();
      return true;
#endif

    case USBTMC_MSGID_VENDOR_SPECIFIC_MSG_OUT:
      vendor_specific_requested = true;
      return handle_dev_msg_out_start(rhport, msg, len);

    case USBTMC_MSGID_VENDOR_SPECIFIC_IN:
      vendor_specific_requested = true;
      return handle_dev_msg_in(msg, len);

    default:
      return false;
  }
}

bool usbtmcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  TU_VERIFY(result == XFER_RESULT_SUCCESS);
  //uart_tx_str_sync("TMC XFER CB\r\n");
  if (usbtmc_state.state == STATE_CLEARING) {
    // A queued Bulk-IN packet can't be flushed: a full one is followed by a short packet to
    // terminate its transfer (USBTMC 1.0 Table 32 step 4b). Ignore everything else.
    if ((ep_addr == usbtmc_state.ep_bulk_in) && (xferred_bytes != 0u) &&
        ((xferred_bytes % usbtmc_state.ep_bulk_in_mps) == 0u)) {
      TU_VERIFY(usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, 0u, false));
    }
    return true;
  }

  if (ep_addr == usbtmc_state.ep_bulk_out) {
    switch (usbtmc_state.state) {
      case STATE_IDLE:
        if (!handle_msg_out_header(rhport, (usbtmc_msg_generic_t *) usbtmc_epbuf.epout, xferred_bytes)) {
          usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
          return false;
        }
        return true;

      case STATE_RCV:
        if (!handle_dev_msg_out(rhport, usbtmc_epbuf.epout, xferred_bytes, xferred_bytes)) {
          usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
          return false;
        }
        return true;

      case STATE_ABORTING_BULK_OUT:
        // Should be stalled by now, shouldn't have received a packet.
        return false;

      case STATE_TX_REQUESTED:
      case STATE_TX_INITIATED:
      case STATE_ABORTING_BULK_IN:
      case STATE_ABORTING_BULK_IN_SHORTED:
      case STATE_ABORTING_BULK_IN_ABORTED:
      default:
        return false;
    }
  } else if (ep_addr == usbtmc_state.ep_bulk_in) {
    // The queued packet reached the host
    usbtmc_state.transfer_size_sent += usbtmc_state.in_flight;
    usbtmc_state.in_flight = 0;

    switch (usbtmc_state.state) {
      case STATE_TX_SHORTED:
        TU_VERIFY(change_state(STATE_TX_SHORTED, STATE_NAK));
        TU_VERIFY(tud_usbtmc_msgBulkIn_complete_cb());
        break;

      case STATE_TX_INITIATED:
        if (usbtmc_state.transfer_size_remaining >= USBTMCD_BUFFER_SIZE) {
          // Copy buffer to ensure alignment correctness
          memcpy(usbtmc_epbuf.epin, usbtmc_state.dev_in_buffer, USBTMCD_BUFFER_SIZE);
          usbtmc_state.dev_in_buffer += USBTMCD_BUFFER_SIZE;
          usbtmc_state.transfer_size_remaining -= USBTMCD_BUFFER_SIZE;
          usbtmc_state.in_flight = USBTMCD_BUFFER_SIZE;
          TU_VERIFY(usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, USBTMCD_BUFFER_SIZE, false));
        } else// last packet
        {
          size_t packet_len = usbtmc_state.transfer_size_remaining;
          memcpy(usbtmc_epbuf.epin, usbtmc_state.dev_in_buffer, usbtmc_state.transfer_size_remaining);
          usbtmc_state.in_flight = packet_len;
          usbtmc_state.transfer_size_remaining = 0;
          usbtmc_state.dev_in_buffer = NULL;
          if (((packet_len % usbtmc_state.ep_bulk_in_mps) != 0) || (packet_len == 0)) {
            (void) change_state(STATE_TX_INITIATED, STATE_TX_SHORTED);
          }
          TU_VERIFY(usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, (uint16_t) packet_len, false));
        }
        return true;

      case STATE_ABORTING_BULK_IN:
        // The full packet in flight has gone, terminate the transfer with a short packet
        (void) change_state(STATE_ABORTING_BULK_IN, STATE_ABORTING_BULK_IN_SHORTED);
        TU_VERIFY(usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, (uint16_t) 0u, false));
        return true;

      case STATE_ABORTING_BULK_IN_SHORTED:
        // The short packet has been read, the abort is complete
        (void) change_state(STATE_ABORTING_BULK_IN_SHORTED, STATE_ABORTING_BULK_IN_ABORTED);
        return true;

      default:
        // Last packet of a transfer the host has since dropped, e.g. CLEAR_FEATURE while clearing
        return true;
    }
  } else if (ep_addr == usbtmc_state.ep_int_in) {
    TU_VERIFY(tud_usbtmc_notification_complete_cb());
    return true;
  }
  return false;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool usbtmcd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  // nothing to do with DATA and ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  uint8_t tmc_status = USBTMC_STATUS_FAILED;

  if ((request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) &&
      (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) &&
      (request->bRequest == TUSB_REQ_CLEAR_FEATURE) &&
      (request->wValue == TUSB_REQ_FEATURE_EDPT_HALT)) {
    uint32_t ep_addr = (request->wIndex);

    // At this point, a transfer MAY be in progress. Based on USB spec, when clearing bulk EP HALT,
    // the EP transfer buffer needs to be cleared and DTOG needs to be reset, even if
    // the EP is not halted. The only USBD API interface to do this is to stall and then un-stall the EP.
    if (ep_addr == usbtmc_state.ep_bulk_out) {
      // USBD core has placed the EP in NAK state. A Bulk-IN reply in progress keeps its state, bulk-OUT is
      // armed again once it completes, and so does a clear: its actions complete across a standard request
      // (USBTMC 1.0 Table 17).
      usbtmc_lock();
      usbd_edpt_stall(rhport, (uint8_t) ep_addr);
      usbd_edpt_clear_stall(rhport, (uint8_t) ep_addr);
      usbtmcd_state_enum const state = usbtmc_state.state;
      if ((state != STATE_TX_REQUESTED) && (state != STATE_TX_INITIATED) && (state != STATE_TX_SHORTED) &&
          (state != STATE_ABORTING_BULK_IN) && (state != STATE_ABORTING_BULK_IN_SHORTED) &&
          (state != STATE_CLEARING)) {
        usbtmc_state.state = STATE_NAK;
      }
      usbtmc_unlock();
      tud_usbtmc_bulkOut_clearFeature_cb();
    } else if (ep_addr == usbtmc_state.ep_bulk_in) {
      bool rearm = false;
      usbtmc_lock();
      usbd_edpt_stall(rhport, (uint8_t) ep_addr);
      usbd_edpt_clear_stall(rhport, (uint8_t) ep_addr);
      // The packet in flight is dropped without a completion: it was never sent
      usbtmc_state.in_flight = 0;
      switch (usbtmc_state.state) {
        case STATE_TX_REQUESTED:
        case STATE_TX_INITIATED:
        case STATE_TX_SHORTED:
          // The reply is gone, take commands again
          usbtmc_state.state = STATE_NAK;
          rearm = true;
          break;
        case STATE_ABORTING_BULK_IN:
        case STATE_ABORTING_BULK_IN_SHORTED:
          // The abort still ends with a short packet (USBTMC 1.0 Tables 17, 26)
          usbtmc_state.state = STATE_ABORTING_BULK_IN_SHORTED;
          (void) usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, 0u, false);
          break;
        default:
          break;
      }
      usbtmc_unlock();
      tud_usbtmc_bulkIn_clearFeature_cb();
      if (rearm) {
        // bulk-OUT was left unarmed for the reply. Result ignored: false means the callback armed it already.
        (void) tud_usbtmc_start_bus_read();
      }
    } else if ((usbtmc_state.ep_int_in != 0) && (ep_addr == usbtmc_state.ep_int_in)) {
      // Clearing interrupt in EP
      usbd_edpt_stall(rhport, (uint8_t) ep_addr);
      usbd_edpt_clear_stall(rhport, (uint8_t) ep_addr);
    } else {
      return false;
    }
    return true;
  }

  // Otherwise, we only handle class requests.
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) {
    return false;
  }

  // Verification that we own the interface is unneeded since it's been routed to us specifically.

  switch (request->bRequest) {
    // USBTMC required requests
    case USBTMC_bREQUEST_INITIATE_ABORT_BULK_OUT: {
      usbtmc_initiate_abort_rsp_t rsp = {
          .bTag = usbtmc_state.last_bulk_out_tag,
      };
      TU_VERIFY(request->bmRequestType == 0xA2);// in,class,interface
      TU_VERIFY(request->wLength == sizeof(rsp));
      TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_out);

      // wValue is the requested bTag to abort (USBTMC 1.0 Tables 18 and 20)
      if (usbtmc_state.state != STATE_RCV) {
        rsp.USBTMC_status = USBTMC_STATUS_FAILED;
      } else if (usbtmc_state.last_bulk_out_tag != tu_u16_low(request->wValue)) {
        rsp.USBTMC_status = USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS;
      } else {
        rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
        usbtmc_lock();
        usbtmc_state.state = STATE_ABORTING_BULK_OUT;
        usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
        usbtmc_unlock();
        TU_VERIFY(tud_usbtmc_initiate_abort_bulk_out_cb(&(rsp.USBTMC_status)));
        if (rsp.USBTMC_status == USBTMC_STATUS_SUCCESS) {
          usbtmc_state.split = SPLIT_ABORT_BULK_OUT;
        }
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &rsp, sizeof(rsp)));
      return true;
    }

    case USBTMC_bREQUEST_CHECK_ABORT_BULK_OUT_STATUS: {
      usbtmc_check_abort_bulk_rsp_t rsp = {
          .USBTMC_status = USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS};
      TU_VERIFY(request->bmRequestType == 0xA2);// in,class,EP
      TU_VERIFY(request->wLength == sizeof(rsp));
      TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_out);
      // Bulk-OUT is Halted the moment the abort is accepted, so it is complete already (USBTMC 1.0 Table 23).
      // Without that INITIATE the CHECK is unexpected (4.2.1.1 rule 6).
      if (usbtmc_state.split == SPLIT_ABORT_BULK_OUT) {
        usbtmc_state.split = SPLIT_NONE;
        rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
        rsp.NBYTES_RXD_TXD = usbtmc_state.transfer_size_sent;
        TU_VERIFY(tud_usbtmc_check_abort_bulk_out_cb(&rsp));
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &rsp, sizeof(rsp)));
      return true;
    }

    case USBTMC_bREQUEST_INITIATE_ABORT_BULK_IN: {
      usbtmc_initiate_abort_rsp_t rsp = {
          .bTag = usbtmc_state.last_bulk_in_tag,
      };
      TU_VERIFY(request->bmRequestType == 0xA2);// in,class,interface
      TU_VERIFY(request->wLength == sizeof(rsp));
      TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_in);
      // wValue is the requested bTag to abort (USBTMC 1.0 Tables 24 and 26). The transfer is in progress
      // until its short packet has been sent. Decided under the lock, the application may be starting the reply.
      usbtmc_lock();
      usbtmcd_state_enum const old_state = usbtmc_state.state;
      bool const in_progress = (old_state == STATE_TX_REQUESTED) || (old_state == STATE_TX_INITIATED) ||
                              (old_state == STATE_TX_SHORTED);
      bool const abort = in_progress && (usbtmc_state.last_bulk_in_tag == tu_u16_low(request->wValue));
      if (abort) {
        usbtmc_state.transfer_size_remaining = 0u;
        // TX_INITIATED always has a full packet in flight, the short packet follows it in usbtmcd_xfer_cb()
        usbtmc_state.state = (old_state == STATE_TX_INITIATED) ? STATE_ABORTING_BULK_IN : STATE_ABORTING_BULK_IN_SHORTED;
        if (old_state == STATE_TX_REQUESTED) {
          // Send short packet, nothing is in the buffer yet
          (void) usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_epbuf.epin, (uint16_t) 0u, false);
        }
      }
      usbtmc_unlock();
      if (abort) {
        rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
        TU_VERIFY(tud_usbtmc_initiate_abort_bulk_in_cb(&(rsp.USBTMC_status)));
        if (rsp.USBTMC_status == USBTMC_STATUS_SUCCESS) {
          usbtmc_state.split = SPLIT_ABORT_BULK_IN;
        }
      } else if (in_progress) {// FIXME: Unsure how to check  if the OUT endpoint fifo is non-empty....
        rsp.USBTMC_status = USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS;
      } else {
        rsp.USBTMC_status = USBTMC_STATUS_FAILED;
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &rsp, sizeof(rsp)));
      return true;
    }

    case USBTMC_bREQUEST_CHECK_ABORT_BULK_IN_STATUS: {
      TU_VERIFY(request->bmRequestType == 0xA2);// in,class,EP
      TU_VERIFY(request->wLength == 8u);
      TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_in);

      // Status is decided before the application callback, which may re-arm bulk-OUT (USBTMC 1.0 Tables 28 and 29).
      // Without that INITIATE the CHECK is unexpected (4.2.1.1 rule 6).
      usbtmc_check_abort_bulk_rsp_t rsp = {
          .USBTMC_status = USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS,
      };
      if (usbtmc_state.split == SPLIT_ABORT_BULK_IN) {
        rsp.USBTMC_status = USBTMC_STATUS_FAILED;
        usbtmc_lock();
        switch (usbtmc_state.state) {
          case STATE_ABORTING_BULK_IN_ABORTED:
            rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
            rsp.NBYTES_RXD_TXD = usbtmc_state.transfer_size_sent;
            // bulk-OUT is not armed, tud_usbtmc_start_bus_read() arms it from NAK
            usbtmc_state.state = STATE_NAK;
            break;
          case STATE_ABORTING_BULK_IN:
          case STATE_ABORTING_BULK_IN_SHORTED:
            // The full or the short packet is still queued
            rsp.USBTMC_status = USBTMC_STATUS_PENDING;
            rsp.bmAbortBulkIn.BulkInFifoBytes = 1u;
            break;
          default:
            break;
        }
        usbtmc_unlock();
        if (rsp.USBTMC_status != USBTMC_STATUS_PENDING) {
          usbtmc_state.split = SPLIT_NONE;
        }
        TU_VERIFY(tud_usbtmc_check_abort_bulk_in_cb(&rsp));
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &rsp, sizeof(rsp)));

      return true;
    }

    case USBTMC_bREQUEST_INITIATE_CLEAR: {
      TU_VERIFY(request->bmRequestType == 0xA1);// in,class,interface
      TU_VERIFY(request->wLength == sizeof(tmc_status));
      // After receiving an INITIATE_CLEAR request, the device must Halt the Bulk-OUT endpoint, queue the
      // control endpoint response shown in Table 31, and clear all input buffers and output buffers.
      // Any other status than SUCCESS must not Halt Bulk-OUT (USBTMC 1.0 Table 32).
      TU_VERIFY(tud_usbtmc_initiate_clear_cb(&tmc_status));
      if (tmc_status == USBTMC_STATUS_SUCCESS) {
        usbtmc_lock();
        usbtmc_state.state = STATE_CLEARING;
        usbtmc_state.transfer_size_remaining = 0;
        usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
        usbtmc_unlock();
        usbtmc_state.split = SPLIT_CLEAR;
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &tmc_status, sizeof(tmc_status)));
      return true;
    }

    case USBTMC_bREQUEST_CHECK_CLEAR_STATUS: {
      TU_VERIFY(request->bmRequestType == 0xA1);// in,class,interface
      usbtmc_get_clear_status_rsp_t clear_rsp = {0};
      TU_VERIFY(request->wLength == sizeof(clear_rsp));

      if (usbtmc_state.split != SPLIT_CLEAR) {
        // No INITIATE_CLEAR to report on (USBTMC 1.0 4.2.1.1 rule 6)
        clear_rsp.USBTMC_status = USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS;
      } else if (usbd_edpt_busy(rhport, usbtmc_state.ep_bulk_in)) {
        // Stuff stuck in TX buffer?
        clear_rsp.bmClear.BulkInFifoBytes = 1;
        clear_rsp.USBTMC_status = USBTMC_STATUS_PENDING;
      } else {
        // Let app check if it's clear
        TU_VERIFY(tud_usbtmc_check_clear_cb(&clear_rsp));
        if (clear_rsp.USBTMC_status != USBTMC_STATUS_PENDING) {
          usbtmc_state.split = SPLIT_NONE;
        }
        if (clear_rsp.USBTMC_status == USBTMC_STATUS_SUCCESS) {
          // bulk-OUT stays Halted until the host's CLEAR_FEATURE, whose callback arms it again
          (void) change_state(STATE_CLEARING, STATE_NAK);
        }
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &clear_rsp, sizeof(clear_rsp)));
      return true;
    }

    case USBTMC_bREQUEST_GET_CAPABILITIES: {
      TU_VERIFY(request->bmRequestType == 0xA1);// in,class,interface
      TU_VERIFY(request->wLength == sizeof(*(usbtmc_state.capabilities)));
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) (uintptr_t) usbtmc_state.capabilities, sizeof(*usbtmc_state.capabilities)));
      return true;
    }
      // USBTMC Optional Requests

    case USBTMC_bREQUEST_INDICATOR_PULSE:// Optional
    {
      TU_VERIFY(request->bmRequestType == 0xA1);// in,class,interface
      TU_VERIFY(request->wLength == sizeof(tmc_status));
      TU_VERIFY(usbtmc_state.capabilities->bmIntfcCapabilities.supportsIndicatorPulse);
      TU_VERIFY(tud_usbtmc_indicator_pulse_cb(request, &tmc_status));
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &tmc_status, sizeof(tmc_status)));
      return true;
    }
#if (CFG_TUD_USBTMC_ENABLE_488)

      // USB488 required requests
    case USB488_bREQUEST_READ_STATUS_BYTE: {
      usbtmc_read_stb_rsp_488_t rsp;
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(request->wLength == sizeof(rsp));// in,class,interface

      uint8_t const btag = (uint8_t) (request->wValue & 0x7Fu);
      TU_VERIFY((request->wValue & (~0x7F)) == 0u);// Other bits are required to be zero (USB488v1.0 Table 11)
      TU_VERIFY(btag >= 0x02);
      TU_VERIFY(request->wIndex == usbtmc_state.itf_id);
      rsp.bTag = btag;
      if (usbtmc_state.ep_int_in != 0) {
        rsp.statusByte = 0x00;// Use interrupt endpoint, instead. Must be 0x00 (USB488v1.0 4.3.1.2)
        // Claim before asking for the status byte, so the byte returned is the byte queued (USB488v1.0 3.4.1)
        if (!usbd_edpt_claim(rhport, usbtmc_state.ep_int_in)) {
          rsp.USBTMC_status = USB488_STATUS_INTERRUPT_IN_BUSY;
        } else {
          rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
          usbtmc_read_stb_interrupt_488_t *int_msg = (usbtmc_read_stb_interrupt_488_t *) usbtmc_epbuf.epnotif;
          int_msg->bNotify1.one = 1;
          int_msg->bNotify1.bTag = btag;
          int_msg->StatusByte = tud_usbtmc_get_stb_cb(&(rsp.USBTMC_status));
          // Must be queued before control request response sent (USB488v1.0 4.3.1.2)
          if (rsp.USBTMC_status != USBTMC_STATUS_SUCCESS) {
            // Nothing to send when the application fails the request
            usbd_edpt_release(rhport, usbtmc_state.ep_int_in);
          } else if (!usbd_edpt_xfer(rhport, usbtmc_state.ep_int_in, usbtmc_epbuf.epnotif, (uint16_t) sizeof(*int_msg), false)) {
            rsp.USBTMC_status = USB488_STATUS_INTERRUPT_IN_BUSY;
          }
        }
      } else {
        rsp.statusByte = tud_usbtmc_get_stb_cb(&(rsp.USBTMC_status));
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void *) &rsp, sizeof(rsp)));
      return true;
    }
      // USB488 optional requests
    case USB488_bREQUEST_REN_CONTROL:
    case USB488_bREQUEST_GO_TO_LOCAL:
    case USB488_bREQUEST_LOCAL_LOCKOUT: {
      TU_VERIFY(request->bmRequestType == 0xA1);// in,class,interface
      return false;
    }
#endif

    default:
      return false;
  }
}

#endif /* CFG_TUD_USBTMC */
