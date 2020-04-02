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

#if ( TUSB_OPT_DEVICE_ENABLED && (CFG_TUD_NET != OPT_NET_NONE) )

#include "net_device.h"
#include "device/usbd_pvt.h"
#include "rndis_protocol.h"

void rndis_class_set_handler(uint8_t *data, int size); /* found in ./misc/networking/rndis_reports.c */

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;
  uint_fast8_t active_alt_setting;
} netd_interface_t;

#if CFG_TUD_NET == OPT_NET_ECM
  #define CFG_TUD_NET_PACKET_PREFIX_LEN 0
  #define CFG_TUD_NET_PACKET_SUFFIX_LEN 0
  #define CFG_TUD_NET_INTERFACESUBCLASS CDC_COMM_SUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL
#elif CFG_TUD_NET == OPT_NET_RNDIS
  #define CFG_TUD_NET_PACKET_PREFIX_LEN sizeof(rndis_data_packet_t)
  #define CFG_TUD_NET_PACKET_SUFFIX_LEN 0
  #define CFG_TUD_NET_INTERFACESUBCLASS TUD_RNDIS_ITF_SUBCLASS
#elif CFG_TUD_NET == OPT_NET_EEM
  #define CFG_TUD_NET_PACKET_PREFIX_LEN 2
  #define CFG_TUD_NET_PACKET_SUFFIX_LEN 4
  #define CFG_TUD_NET_INTERFACESUBCLASS CDC_COMM_SUBCLASS_ETHERNET_EMULATION_MODEL
#endif

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t received[CFG_TUD_NET_PACKET_PREFIX_LEN + CFG_TUD_NET_MTU + CFG_TUD_NET_PACKET_PREFIX_LEN];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t transmitted[CFG_TUD_NET_PACKET_PREFIX_LEN + CFG_TUD_NET_MTU + CFG_TUD_NET_PACKET_PREFIX_LEN];

#if CFG_TUD_NET == OPT_NET_ECM
  CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static tusb_control_request_t notify =
  {
    .bmRequestType = 0x21,
    .bRequest = 0 /* NETWORK_CONNECTION */,
    .wValue = 1 /* Connected */,
    .wLength = 0,
  };
#elif CFG_TUD_NET == OPT_NET_RNDIS
  CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t rndis_buf[120];
#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static netd_interface_t _netd_itf;

static bool can_xmit;

void tud_network_recv_renew(void)
{
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_out, received, sizeof(received));
}

static void do_in_xfer(uint8_t *buf, uint16_t len)
{
  can_xmit = false;
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_in, buf, len);
}

void netd_report(uint8_t *buf, uint16_t len)
{
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_notif, buf, len);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void netd_init(void)
{
  tu_memclr(&_netd_itf, sizeof(_netd_itf));
}

void netd_reset(uint8_t rhport)
{
  (void) rhport;

  netd_init();
}

bool netd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length)
{
  // sanity check the descriptor
  TU_ASSERT (CFG_TUD_NET_INTERFACESUBCLASS == itf_desc->bInterfaceSubClass);

  // confirm interface hasn't already been allocated
  TU_ASSERT(0 == _netd_itf.ep_in);

  //------------- first Interface -------------//
  _netd_itf.itf_num = itf_desc->bInterfaceNumber;
  _netd_itf.active_alt_setting = 0u;

  uint8_t const * p_desc = tu_desc_next( itf_desc );
  (*p_length) = sizeof(tusb_desc_interface_t);

#if CFG_TUD_NET != OPT_NET_EEM
  // Communication Functional Descriptors
  while ( TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) )
  {
    (*p_length) += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  // notification endpoint (if any)
  if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
  {
    TU_ASSERT( dcd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc) );

    _netd_itf.ep_notif = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = tu_desc_next(p_desc);
  }

  //------------- second Interface -------------//
  if ( (TUSB_DESC_INTERFACE == p_desc[DESC_OFFSET_TYPE]) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    // next to endpoint descriptor
    p_desc = tu_desc_next(p_desc);
    (*p_length) += sizeof(tusb_desc_interface_t);
  }
#endif

  if (TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
  {
    // Open endpoint pair
    TU_ASSERT( usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &_netd_itf.ep_out, &_netd_itf.ep_in) );

    (*p_length) += 2*sizeof(tusb_desc_endpoint_t);
  }

  tud_network_init_cb();

  // we are ready to transmit a packet
  can_xmit = true;

  // prepare for incoming packets
  tud_network_recv_renew();

  return true;
}

// Invoked when class request DATA stage is finished.
// return false to stall control endpoint (e.g Host send nonsense DATA)
bool netd_control_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  // Handle class request only
  TU_VERIFY (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  TU_VERIFY (_netd_itf.itf_num == request->wIndex);

#if CFG_TUD_NET == OPT_NET_RNDIS
  if (request->bmRequestType_bit.direction == TUSB_DIR_OUT)
  {
    rndis_class_set_handler(rndis_buf, request->wLength);
  }
#endif

  return true;
}

uint8_t netd_get_alt_setting(uint8_t rhport, tusb_control_request_t const * request)
{
  (void)rhport;
  (void)request;
  /* TODO: Return actual value */
  return _netd_itf.active_alt_setting;
}

// Handle class control request
// return false to stall control endpoint (e.g unsupported request)
bool netd_control_request(uint8_t rhport, tusb_control_request_t const * request)
{

  if((request->bmRequestType_bit.direction == TUSB_DIR_OUT)
    && (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
    && (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE))
  {
    /* TODO: Do alternate interface setting work here. Only allow alternate setting of zero, for now. */
    if (request->wIndex > 0)
    {
      return false;
    }
    return true;
  }

  // Handle class request only
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  TU_VERIFY (_netd_itf.itf_num == request->wIndex);

#if CFG_TUD_NET == OPT_NET_ECM
  /* the only required CDC-ECM Management Element Request is SetEthernetPacketFilter */
  if (0x43 /* SET_ETHERNET_PACKET_FILTER */ == request->bRequest)
  {
    tud_control_xfer(rhport, request, NULL, 0);
    notify.wIndex = request->wIndex;
    usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_notif, (uint8_t *)&notify, sizeof(notify));
  }
#elif CFG_TUD_NET == OPT_NET_RNDIS
  if (request->bmRequestType_bit.direction == TUSB_DIR_IN)
  {
    rndis_generic_msg_t *rndis_msg = (rndis_generic_msg_t *)rndis_buf;
    uint32_t msglen = tu_le32toh(rndis_msg->MessageLength);
    TU_ASSERT(msglen <= sizeof(rndis_buf));
    tud_control_xfer(rhport, request, rndis_buf, msglen);
  }
  else
  {
    tud_control_xfer(rhport, request, rndis_buf, sizeof(rndis_buf));
  }
#else
  (void)rhport;
#endif

  return true;
}

struct cdc_eem_packet_header
{
  uint16_t length:14;
  uint16_t bmCRC:1;
  uint16_t bmType:1;
};

static void handle_incoming_packet(uint32_t len)
{
  uint8_t *pnt = received;
  uint32_t size = 0;

#if CFG_TUD_NET == OPT_NET_ECM
  size = len;
#elif CFG_TUD_NET == OPT_NET_RNDIS
  rndis_data_packet_t *r = (rndis_data_packet_t *)pnt;
  if (len >= sizeof(rndis_data_packet_t))
    if ( (r->MessageType == REMOTE_NDIS_PACKET_MSG) && (r->MessageLength <= len))
      if ( (r->DataOffset + offsetof(rndis_data_packet_t, DataOffset) + r->DataLength) <= len)
      {
        pnt = &received[r->DataOffset + offsetof(rndis_data_packet_t, DataOffset)];
        size = r->DataLength;
      }
#elif CFG_TUD_NET == OPT_NET_EEM
  struct cdc_eem_packet_header *hdr = (struct cdc_eem_packet_header *)pnt;

  (void)len;

  if (hdr->bmType)
  {
    /* EEM Control Packet: discard it */
    tud_network_recv_renew();
  }
  else
  {
    /* EEM Data Packet */
    pnt += CFG_TUD_NET_PACKET_PREFIX_LEN;
    size = hdr->length - 4; /* discard the unused CRC-32 */
  }
#endif

  if (size)
  {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    bool accepted = true;

    if (p)
    {
      memcpy(p->payload, pnt, size);
      p->len = size;
      accepted = tud_network_recv_cb(p);
    }

    if (!p || !accepted)
    {
      /* if a buffer couldn't be allocated or accepted by the callback, we must discard this packet */
      tud_network_recv_renew();
    }
  }
}

bool netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  /* new packet received */
  if ( ep_addr == _netd_itf.ep_out )
  {
    handle_incoming_packet(xferred_bytes);
  }

  /* data transmission finished */
  if ( ep_addr == _netd_itf.ep_in )
  {
    /* TinyUSB requires the class driver to implement ZLP (since ZLP usage is class-specific) */

    if ( xferred_bytes && (0 == (xferred_bytes % CFG_TUD_NET_ENDPOINT_SIZE)) )
    {
      do_in_xfer(NULL, 0); /* a ZLP is needed */
    }
    else
    {
      /* we're finally finished */
      can_xmit = true;
    }
  }

  return true;
}

bool tud_network_can_xmit(void)
{
  return can_xmit;
}

void tud_network_xmit(struct pbuf *p)
{
  struct pbuf *q;
  uint8_t *data;
  uint16_t len;

  if (!can_xmit)
    return;

  len = CFG_TUD_NET_PACKET_PREFIX_LEN;
  data = transmitted + len;

  for(q = p; q != NULL; q = q->next)
  {
    memcpy(data, (char *)q->payload, q->len);
    data += q->len;
    len += q->len;
  }

#if CFG_TUD_NET == OPT_NET_RNDIS
  rndis_data_packet_t *hdr = (rndis_data_packet_t *)transmitted;
  memset(hdr, 0, sizeof(rndis_data_packet_t));
  hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
  hdr->MessageLength = len;
  hdr->DataOffset = sizeof(rndis_data_packet_t) - offsetof(rndis_data_packet_t, DataOffset);
  hdr->DataLength = len - sizeof(rndis_data_packet_t);
#elif CFG_TUD_NET == OPT_NET_EEM
  struct cdc_eem_packet_header *hdr = (struct cdc_eem_packet_header *)transmitted;
  /* append a fake CRC-32; the standard allows 0xDEADBEEF, which takes less CPU time */
  data[0] = 0xDE; data[1] = 0xAD; data[2] = 0xBE; data[3] = 0xEF;
  /* adjust length to reflect added fake CRC-32 */
  len += 4;
  hdr->bmType = 0; /* EEM Data Packet */
  hdr->length = len - sizeof(struct cdc_eem_packet_header);
  hdr->bmCRC = 0; /* Ethernet Frame CRC-32 set to 0xDEADBEEF */
#endif

  do_in_xfer(transmitted, len);
}

#endif
