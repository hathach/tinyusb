/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Peter Lawrence
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if ( CFG_TUD_ENABLED && CFG_TUD_ECM_RNDIS )

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "net_device.h"
#include "rndis_protocol.h"

extern void rndis_class_set_handler(uint8_t *data, int size); /* found in ./misc/networking/rndis_reports.c */

#define CFG_TUD_NET_PACKET_PREFIX_LEN sizeof(rndis_data_packet_t)
#define CFG_TUD_NET_PACKET_SUFFIX_LEN 0

#define NETD_PACKET_SIZE  (CFG_TUD_NET_PACKET_PREFIX_LEN + CFG_TUD_NET_MTU + CFG_TUD_NET_PACKET_PREFIX_LEN)
#define NETD_CONTROL_SIZE 120

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t itf_num;      // Index number of Management Interface, +1 for Data Interface
  uint8_t itf_data_alt; // Alternate setting of Data Interface. 0 : inactive, 1 : active

  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  bool ecm_mode;

  // Endpoint descriptor use to open/close when receiving SetInterface
  // TODO since configuration descriptor may not be long-lived memory, we should
  // keep a copy of endpoint attribute instead
  uint8_t const * ecm_desc_epdata;
} netd_interface_t;

typedef struct ecm_notify_struct {
  tusb_control_request_t header;
  uint32_t downlink, uplink;
} ecm_notify_t;

typedef struct {
  TUD_EPBUF_DEF(rx, NETD_PACKET_SIZE);
  TUD_EPBUF_DEF(tx, NETD_PACKET_SIZE);

  TUD_EPBUF_DEF(notify, sizeof(ecm_notify_t));
  TUD_EPBUF_DEF(ctrl, NETD_CONTROL_SIZE);
} netd_epbuf_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static netd_interface_t _netd_itf;
CFG_TUD_MEM_SECTION static netd_epbuf_t _netd_epbuf;
static bool can_xmit;
static bool ecm_link_is_up = true;  // Store link state for ECM mode

void tud_network_recv_renew(void) {
  usbd_edpt_xfer(0, _netd_itf.ep_out, _netd_epbuf.rx, NETD_PACKET_SIZE);
}

static void do_in_xfer(uint8_t *buf, uint16_t len) {
  can_xmit = false;
  usbd_edpt_xfer(0, _netd_itf.ep_in, buf, len);
}

void netd_report(uint8_t *buf, uint16_t len) {
  const uint8_t rhport = 0;
  len = tu_min16(len, sizeof(ecm_notify_t));

  if (!usbd_edpt_claim(rhport, _netd_itf.ep_notif)) {
    TU_LOG1("ECM: Failed to claim notification endpoint\n");
    return;
  }

  memcpy(_netd_epbuf.notify, buf, len);
  usbd_edpt_xfer(rhport, _netd_itf.ep_notif, _netd_epbuf.notify, len);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void netd_init(void) {
  tu_memclr(&_netd_itf, sizeof(_netd_itf));
}

bool netd_deinit(void) {
  return true;
}

void netd_reset(uint8_t rhport) {
  (void) rhport;
  netd_init();
}

uint16_t netd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
  bool const is_rndis = (TUD_RNDIS_ITF_CLASS    == itf_desc->bInterfaceClass    &&
                         TUD_RNDIS_ITF_SUBCLASS == itf_desc->bInterfaceSubClass &&
                         TUD_RNDIS_ITF_PROTOCOL == itf_desc->bInterfaceProtocol);

  bool const is_ecm = (TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
                       CDC_COMM_SUBCLASS_ETHERNET_CONTROL_MODEL == itf_desc->bInterfaceSubClass &&
                       0x00                                     == itf_desc->bInterfaceProtocol);

  TU_VERIFY(is_rndis || is_ecm, 0);

  // confirm interface hasn't already been allocated
  TU_ASSERT(0 == _netd_itf.ep_notif, 0);

  // sanity check the descriptor
  _netd_itf.ecm_mode = is_ecm;

  //------------- Management Interface -------------//
  _netd_itf.itf_num = itf_desc->bInterfaceNumber;

  uint16_t drv_len = sizeof(tusb_desc_interface_t);
  uint8_t const * p_desc = tu_desc_next( itf_desc );

  // Communication Functional Descriptors
  while (TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len) {
    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  // notification endpoint (if any)
  if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
    TU_ASSERT(usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), 0);

    _netd_itf.ep_notif = ((tusb_desc_endpoint_t const*)p_desc)->bEndpointAddress;

    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface -------------//
  // - RNDIS Data followed immediately by a pair of endpoints
  // - CDC-ECM data interface has 2 alternate settings
  //   - 0 : zero endpoints for inactive (default)
  //   - 1 : IN & OUT endpoints for active networking
  TU_ASSERT(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);

  do {
    tusb_desc_interface_t const * data_itf_desc = (tusb_desc_interface_t const *) p_desc;
    TU_ASSERT(TUSB_CLASS_CDC_DATA == data_itf_desc->bInterfaceClass, 0);

    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  } while (_netd_itf.ecm_mode && (TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) && (drv_len <= max_len));

  // Pair of endpoints
  TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc), 0);

  if (_netd_itf.ecm_mode) {
    // ECM by default is in-active, save the endpoint attribute
    // to open later when received setInterface
    _netd_itf.ecm_desc_epdata = p_desc;
  } else {
    // Open endpoint pair for RNDIS
    TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &_netd_itf.ep_out, &_netd_itf.ep_in), 0);

    // we are ready to transmit a packet
    can_xmit = true;

    // prepare for incoming packets
    tud_network_recv_renew();
  }

  drv_len += 2*sizeof(tusb_desc_endpoint_t);

  return drv_len;
}

static void ecm_report(bool nc) {
  ecm_notify_t ecm_notify_nc = {
    .header = {
      .bmRequestType = 0xA1,
      .bRequest = 0, /* NETWORK_CONNECTION aka NetworkConnection */
      .wValue = ecm_link_is_up ? 1 : 0,   /* Use current link state */
      .wLength = 0,
    },
  };

  const ecm_notify_t ecm_notify_csc = {
    .header = {
      .bmRequestType = 0xA1,
      .bRequest = 0x2A, /* CONNECTION_SPEED_CHANGE aka ConnectionSpeedChange */
      .wLength = 8,
    },
    .downlink = 9728000,
    .uplink = 9728000,
  };

  ecm_notify_t notify = (nc) ? ecm_notify_nc : ecm_notify_csc;
  notify.header.wIndex = _netd_itf.itf_num;
  netd_report((uint8_t *)&notify, (nc) ? sizeof(notify.header) : sizeof(notify));
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool netd_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
  if (stage == CONTROL_STAGE_SETUP) {
    switch (request->bmRequestType_bit.type) {
      case TUSB_REQ_TYPE_STANDARD:
        switch (request->bRequest) {
          case TUSB_REQ_GET_INTERFACE: {
            uint8_t const req_itfnum = (uint8_t)request->wIndex;
            TU_VERIFY(_netd_itf.itf_num+1 == req_itfnum);

            tud_control_xfer(rhport, request, &_netd_itf.itf_data_alt, 1);
          }
          break;

          case TUSB_REQ_SET_INTERFACE: {
            uint8_t const req_itfnum = (uint8_t)request->wIndex;
            uint8_t const req_alt = (uint8_t)request->wValue;

            // Only valid for Data Interface with Alternate is either 0 or 1
            TU_VERIFY(_netd_itf.itf_num+1 == req_itfnum && req_alt < 2);

            // ACM-ECM only: qequest to enable/disable network activities
            TU_VERIFY(_netd_itf.ecm_mode);

            _netd_itf.itf_data_alt = req_alt;

            if (_netd_itf.itf_data_alt) {
              // TODO since we don't actually close endpoint
              // hack here to not re-open it
              if (_netd_itf.ep_in == 0 && _netd_itf.ep_out == 0) {
                TU_ASSERT(_netd_itf.ecm_desc_epdata);
                TU_ASSERT(
                  usbd_open_edpt_pair(rhport, _netd_itf.ecm_desc_epdata, 2, TUSB_XFER_BULK, &_netd_itf.ep_out, &
                    _netd_itf.ep_in));

                // TODO should be merge with RNDIS's after endpoint opened
                // Also should have opposite callback for application to disable network !!
                can_xmit = true; // we are ready to transmit a packet
                tud_network_recv_renew(); // prepare for incoming packets
              }
            } else {
              // TODO close the endpoint pair
              // For now pretend that we did, this should have no harm since host won't try to
              // communicate with the endpoints again
              // _netd_itf.ep_in = _netd_itf.ep_out = 0
            }

            tud_control_status(rhport, request);
          }
          break;

          // unsupported request
          default: return false;
        }
        break;

      case TUSB_REQ_TYPE_CLASS:
        TU_VERIFY(_netd_itf.itf_num == request->wIndex);

        if (_netd_itf.ecm_mode) {
          /* the only required CDC-ECM Management Element Request is SetEthernetPacketFilter */
          if (0x43 /* SET_ETHERNET_PACKET_FILTER */ == request->bRequest) {
            tud_control_xfer(rhport, request, NULL, 0);
            // Only send connection notification if link is up
            if (ecm_link_is_up) {
              ecm_report(true);
            }
          }
        } else {
          if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
            rndis_generic_msg_t* rndis_msg = (rndis_generic_msg_t*)((void*)_netd_epbuf.ctrl);
            uint32_t msglen = tu_le32toh(rndis_msg->MessageLength);
            TU_ASSERT(msglen <= NETD_CONTROL_SIZE);
            tud_control_xfer(rhport, request, _netd_epbuf.ctrl, (uint16_t)msglen);
          } else {
            tud_control_xfer(rhport, request, _netd_epbuf.ctrl, NETD_CONTROL_SIZE);
          }
        }
        break;

      // unsupported request
      default: return false;
    }
  } else if (stage == CONTROL_STAGE_DATA) {
    // Handle RNDIS class control OUT only
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.direction == TUSB_DIR_OUT &&
        _netd_itf.itf_num == request->wIndex) {
      if (!_netd_itf.ecm_mode) {
        rndis_class_set_handler(_netd_epbuf.ctrl, request->wLength);
      }
    }
  }

  return true;
}

static void handle_incoming_packet(uint32_t len) {
  uint8_t* pnt = _netd_epbuf.rx;
  uint32_t size = 0;

  if (_netd_itf.ecm_mode) {
    size = len;
  } else {
    rndis_data_packet_t* r = (rndis_data_packet_t*)((void*)pnt);
    if (len >= sizeof(rndis_data_packet_t)) {
      if ((r->MessageType == REMOTE_NDIS_PACKET_MSG) && (r->MessageLength <= len)) {
        if ((r->DataOffset + offsetof(rndis_data_packet_t, DataOffset) + r->DataLength) <= len) {
          pnt = &_netd_epbuf.rx[r->DataOffset + offsetof(rndis_data_packet_t, DataOffset)];
          size = r->DataLength;
        }
      }
    }
  }

  if (!tud_network_recv_cb(pnt, (uint16_t)size)) {
    /* if a buffer was never handled by user code, we must renew on the user's behalf */
    tud_network_recv_renew();
  }
}

bool netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)rhport;
  (void)result;

  /* new packet received */
  if (ep_addr == _netd_itf.ep_out) {
    handle_incoming_packet(xferred_bytes);
  }

  /* data transmission finished */
  if (ep_addr == _netd_itf.ep_in) {
    /* TinyUSB requires the class driver to implement ZLP (since ZLP usage is class-specific) */

    if (xferred_bytes && (0 == (xferred_bytes % CFG_TUD_NET_ENDPOINT_SIZE))) {
      do_in_xfer(NULL, 0); /* a ZLP is needed */
    } else {
      /* we're finally finished */
      can_xmit = true;
    }
  }

  if (_netd_itf.ecm_mode && (ep_addr == _netd_itf.ep_notif)) {
    // Notification transfer complete - endpoint is now free
    // Don't automatically send speed change notification after link state changes
  }

  return true;
}

bool tud_network_can_xmit(uint16_t size) {
  (void)size;
  return can_xmit;
}

void tud_network_xmit(void *ref, uint16_t arg) {
  if (!can_xmit) {
    return;
  }

  uint16_t len = (_netd_itf.ecm_mode) ? 0 : CFG_TUD_NET_PACKET_PREFIX_LEN;
  uint8_t* data = _netd_epbuf.tx + len;

  len += tud_network_xmit_cb(data, ref, arg);

  if (!_netd_itf.ecm_mode) {
    rndis_data_packet_t *hdr = (rndis_data_packet_t *) ((void*) _netd_epbuf.tx);
    memset(hdr, 0, sizeof(rndis_data_packet_t));
    hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = len;
    hdr->DataOffset = sizeof(rndis_data_packet_t) - offsetof(rndis_data_packet_t, DataOffset);
    hdr->DataLength = len - sizeof(rndis_data_packet_t);
  }

  do_in_xfer(_netd_epbuf.tx, len);
}

// Set the network link state (up/down) and notify the host
void tud_network_link_state(uint8_t rhport, bool is_up) {
  (void)rhport;

  if (_netd_itf.ecm_mode) {
    ecm_link_is_up = is_up;

    // For ECM mode, send network connection notification only
    // Don't trigger speed change notification for link state changes
    ecm_notify_t notify = {
      .header = {
        .bmRequestType = 0xA1,
        .bRequest = 0,        /* NETWORK_CONNECTION */
        .wValue = is_up ? 1 : 0,  /* 0 = disconnected, 1 = connected */
        .wLength = 0,
      },
    };
    notify.header.wIndex = _netd_itf.itf_num;
    netd_report((uint8_t *)&notify, sizeof(notify.header));
  } else {
    // For RNDIS mode, we would need to implement RNDIS status indication
    // This is more complex and requires RNDIS_INDICATE_STATUS_MSG
    // For now, RNDIS doesn't support dynamic link state changes
    (void)is_up;
  }
}

#endif
