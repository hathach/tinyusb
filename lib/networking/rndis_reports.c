/*
  The original version of this code was lrndis/usbd_rndis_core.c from https://github.com/fetisov/lrndis
  It has since been overhauled to suit this application
*/

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 by Sergey Fetisov <fsenok@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include "class/net/net_device.h"
#include "rndis_protocol.h"
#include "netif/ethernet.h"

#define RNDIS_LINK_SPEED 12000000                       /* Link baudrate (12Mbit/s for USB-FS) */
#if !defined(RNDIS_VENDOR)
#define RNDIS_VENDOR     "TinyUSB"                      /* NIC vendor name */
#endif

#define RNDIS_REPORT_UNUSED_VAR(x) ((void)x)

static const uint8_t *const station_hwaddr = tud_network_mac_address;
static const uint8_t *const permanent_hwaddr = tud_network_mac_address;

static usb_eth_stat_t usb_eth_stat = { 0, 0, 0, 0 };
static uint32_t oid_packet_filter = 0x0000000;
static rndis_state_t rndis_state;

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t ndis_report[8] = { 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

static const uint32_t OIDSupportedList[] = 
{
  PP_RNDIS_NTOHL(OID_GEN_SUPPORTED_LIST),
  PP_RNDIS_NTOHL(OID_GEN_HARDWARE_STATUS),
  PP_RNDIS_NTOHL(OID_GEN_MEDIA_SUPPORTED),
  PP_RNDIS_NTOHL(OID_GEN_MEDIA_IN_USE),
  PP_RNDIS_NTOHL(OID_GEN_MAXIMUM_FRAME_SIZE),
  PP_RNDIS_NTOHL(OID_GEN_LINK_SPEED),
  PP_RNDIS_NTOHL(OID_GEN_TRANSMIT_BLOCK_SIZE),
  PP_RNDIS_NTOHL(OID_GEN_RECEIVE_BLOCK_SIZE),
  PP_RNDIS_NTOHL(OID_GEN_VENDOR_ID),
  PP_RNDIS_NTOHL(OID_GEN_VENDOR_DESCRIPTION),
  PP_RNDIS_NTOHL(OID_GEN_VENDOR_DRIVER_VERSION),
  PP_RNDIS_NTOHL(OID_GEN_CURRENT_PACKET_FILTER),
  PP_RNDIS_NTOHL(OID_GEN_MAXIMUM_TOTAL_SIZE),
  PP_RNDIS_NTOHL(OID_GEN_PROTOCOL_OPTIONS),
  PP_RNDIS_NTOHL(OID_GEN_MAC_OPTIONS),
  PP_RNDIS_NTOHL(OID_GEN_MEDIA_CONNECT_STATUS),
//  PP_RNDIS_NTOHL(OID_GEN_MAXIMUM_SEND_PACKETS),
  PP_RNDIS_NTOHL(OID_802_3_PERMANENT_ADDRESS),
  PP_RNDIS_NTOHL(OID_802_3_CURRENT_ADDRESS),
  PP_RNDIS_NTOHL(OID_802_3_MULTICAST_LIST),
  PP_RNDIS_NTOHL(OID_802_3_MAXIMUM_LIST_SIZE),
  PP_RNDIS_NTOHL(OID_802_3_MAC_OPTIONS)
};

#define OID_LIST_LENGTH TU_ARRAY_SIZE(OIDSupportedList)
#define ENC_BUF_SIZE    (OID_LIST_LENGTH * 4 + 32)

static void *encapsulated_buffer;

static void rndis_report(void)
{
  netd_report(ndis_report, sizeof(ndis_report));
}

static void rndis_query_cmplt32(int status, uint32_t data)
{
  rndis_query_cmplt_t *c;

  c = (rndis_query_cmplt_t *)encapsulated_buffer;
  c->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_QUERY_CMPLT);
  c->MessageLength = tu_le32toh(sizeof(rndis_query_cmplt_t) + 4);
  c->InformationBufferLength = PP_RNDIS_NTOHL(4);
  c->InformationBufferOffset = PP_RNDIS_NTOHL(16);
  c->Status = status;
  data = tu_le32toh(data);
  memcpy(c + 1, &data, sizeof(data));
  rndis_report();
}

static void rndis_query_cmplt(int status, const void *data, int size)
{
  rndis_query_cmplt_t *c;

  c = (rndis_query_cmplt_t *)encapsulated_buffer;
  c->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_QUERY_CMPLT);
  c->MessageLength = tu_le32toh(sizeof(rndis_query_cmplt_t) + size);
  c->InformationBufferLength = tu_le32toh(size);
  c->InformationBufferOffset = PP_RNDIS_NTOHL(16);
  c->Status = status;
  memcpy(c + 1, data, size);
  rndis_report();
}

#define MAC_OPT NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | \
                NDIS_MAC_OPTION_RECEIVE_SERIALIZED  | \
                NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  | \
                NDIS_MAC_OPTION_NO_LOOPBACK

static const char *rndis_vendor = RNDIS_VENDOR;

static void rndis_query(void)
{
  switch (((rndis_query_msg_t *)encapsulated_buffer)->Oid) {
    case PP_RNDIS_NTOHL(OID_GEN_SUPPORTED_LIST):
      rndis_query_cmplt(RNDIS_STATUS_SUCCESS, OIDSupportedList, 4 * OID_LIST_LENGTH);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_VENDOR_DRIVER_VERSION):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0x00001000);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_CURRENT_ADDRESS):
      rndis_query_cmplt(RNDIS_STATUS_SUCCESS, station_hwaddr, 6);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_PERMANENT_ADDRESS):
      rndis_query_cmplt(RNDIS_STATUS_SUCCESS, permanent_hwaddr, 6);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MEDIA_SUPPORTED):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MEDIA_IN_USE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_PHYSICAL_MEDIUM):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_HARDWARE_STATUS):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_LINK_SPEED):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, RNDIS_LINK_SPEED / 100);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_VENDOR_ID):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0x00FFFFFF);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_VENDOR_DESCRIPTION):
      rndis_query_cmplt(RNDIS_STATUS_SUCCESS, rndis_vendor, strlen(rndis_vendor) + 1);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_CURRENT_PACKET_FILTER):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, oid_packet_filter);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MAXIMUM_FRAME_SIZE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, CFG_TUD_NET_MTU - SIZEOF_ETH_HDR);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MAXIMUM_TOTAL_SIZE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, CFG_TUD_NET_MTU);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_TRANSMIT_BLOCK_SIZE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, CFG_TUD_NET_MTU);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_RECEIVE_BLOCK_SIZE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, CFG_TUD_NET_MTU);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MEDIA_CONNECT_STATUS):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIA_STATE_CONNECTED);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_RNDIS_CONFIG_PARAMETER):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_MAXIMUM_LIST_SIZE):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 1);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_MULTICAST_LIST):
      rndis_query_cmplt32(PP_RNDIS_NTOHL(RNDIS_STATUS_NOT_SUPPORTED), 0);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_MAC_OPTIONS):
      rndis_query_cmplt32(PP_RNDIS_NTOHL(RNDIS_STATUS_NOT_SUPPORTED), 0);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_MAC_OPTIONS):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, /*MAC_OPT*/ 0);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_RCV_ERROR_ALIGNMENT):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_XMIT_ONE_COLLISION):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    case PP_RNDIS_NTOHL(OID_802_3_XMIT_MORE_COLLISIONS):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_XMIT_OK):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.txok);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_RCV_OK):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.rxok);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_RCV_ERROR):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.rxbad);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_XMIT_ERROR):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.txbad);
      break;
    case PP_RNDIS_NTOHL(OID_GEN_RCV_NO_BUFFER):
      rndis_query_cmplt32(RNDIS_STATUS_SUCCESS, 0);
      break;
    default:
      rndis_query_cmplt(PP_RNDIS_NTOHL(RNDIS_STATUS_FAILURE), NULL, 0);
      break;
  }
}

#define INFBUF  ((uint8_t *)&(m->RequestId) + tu_le32toh(m->InformationBufferOffset))

static void rndis_handle_config_parm(const char *data, int keyoffset, int valoffset, int keylen, int vallen)
{
    (void)data;
    (void)keyoffset;
    (void)valoffset;
    (void)keylen;
    (void)vallen;
}

static void rndis_packetFilter(uint32_t newfilter)
{
    (void)newfilter;
}

static void rndis_handle_set_msg(void)
{
  rndis_set_cmplt_t *c;
  rndis_set_msg_t *m;
  rndis_Oid_t oid;

  c = (rndis_set_cmplt_t *)encapsulated_buffer;
  m = (rndis_set_msg_t *)encapsulated_buffer;

  oid = m->Oid;
  c->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_SET_CMPLT);
  c->MessageLength = tu_le32toh(sizeof(rndis_set_cmplt_t));
  c->Status = PP_RNDIS_NTOHL(RNDIS_STATUS_SUCCESS);

  switch (oid)
  {
    /* Parameters set up in 'Advanced' tab */
    case PP_RNDIS_NTOHL(OID_GEN_RNDIS_CONFIG_PARAMETER):
      {
        rndis_config_parameter_t *p;
        char *ptr = (char *)m;
        ptr += sizeof(rndis_generic_msg_t);
        ptr += m->InformationBufferOffset;
        p = (rndis_config_parameter_t *) ((void*) ptr);
        rndis_handle_config_parm(ptr, p->ParameterNameOffset, p->ParameterValueOffset, p->ParameterNameLength, p->ParameterValueLength);
      }
      break;

    /* Mandatory general OIDs */
    case PP_RNDIS_NTOHL(OID_GEN_CURRENT_PACKET_FILTER):
      memcpy(&oid_packet_filter, INFBUF, 4);
      if (oid_packet_filter) {
        rndis_packetFilter(tu_le32toh(oid_packet_filter));
        rndis_state = rndis_data_initialized;

      } else {
        rndis_state = rndis_initialized;
      }
      RNDIS_REPORT_UNUSED_VAR(rndis_state);
      break;

    case PP_RNDIS_NTOHL(OID_GEN_CURRENT_LOOKAHEAD):
      break;

    case PP_RNDIS_NTOHL(OID_GEN_PROTOCOL_OPTIONS):
      break;

    /* Mandatory 802_3 OIDs */
    case PP_RNDIS_NTOHL(OID_802_3_MULTICAST_LIST):
      break;

    /* Power Managment: fails for now */
    case PP_RNDIS_NTOHL(OID_PNP_ADD_WAKE_UP_PATTERN):
    case PP_RNDIS_NTOHL(OID_PNP_REMOVE_WAKE_UP_PATTERN):
    case PP_RNDIS_NTOHL(OID_PNP_ENABLE_WAKE_UP):
    default:
      c->Status = PP_RNDIS_NTOHL(RNDIS_STATUS_FAILURE);
      break;
  }

  /* c->MessageID is same as before */
  rndis_report();
  return;
}

void rndis_class_set_handler(uint8_t *data, int size)
{
  encapsulated_buffer = data;
  (void)size;

  switch (((rndis_generic_msg_t *)encapsulated_buffer)->MessageType) {
    case PP_RNDIS_NTOHL(REMOTE_NDIS_INITIALIZE_MSG):
      {
        rndis_initialize_cmplt_t *m;
        m = ((rndis_initialize_cmplt_t *)encapsulated_buffer);
        /* m->MessageID is same as before */
        m->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_INITIALIZE_CMPLT);
        m->MessageLength = tu_le32toh(sizeof(rndis_initialize_cmplt_t));
        m->MajorVersion = PP_RNDIS_NTOHL(RNDIS_MAJOR_VERSION);
        m->MinorVersion = PP_RNDIS_NTOHL(RNDIS_MINOR_VERSION);
        m->Status = PP_RNDIS_NTOHL(RNDIS_STATUS_SUCCESS);
        m->DeviceFlags = PP_RNDIS_NTOHL(RNDIS_DF_CONNECTIONLESS);
        m->Medium = PP_RNDIS_NTOHL(RNDIS_MEDIUM_802_3);
        m->MaxPacketsPerTransfer = PP_RNDIS_NTOHL(1);
        m->MaxTransferSize = tu_le32toh(CFG_TUD_NET_MTU + sizeof(rndis_data_packet_t));
        m->PacketAlignmentFactor = PP_RNDIS_NTOHL(0);
        m->AfListOffset = PP_RNDIS_NTOHL(0);
        m->AfListSize = PP_RNDIS_NTOHL(0);
        rndis_state = rndis_initialized;
        rndis_report();
      }
      break;

    case PP_RNDIS_NTOHL(REMOTE_NDIS_QUERY_MSG):
      rndis_query();
      break;
      
    case PP_RNDIS_NTOHL(REMOTE_NDIS_SET_MSG):
      rndis_handle_set_msg();
      break;

    case PP_RNDIS_NTOHL(REMOTE_NDIS_RESET_MSG):
      {
        rndis_reset_cmplt_t * m;
        m = ((rndis_reset_cmplt_t *)encapsulated_buffer);
        rndis_state = rndis_uninitialized;
        m->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_RESET_CMPLT);
        m->MessageLength = tu_le32toh(sizeof(rndis_reset_cmplt_t));
        m->Status = PP_RNDIS_NTOHL(RNDIS_STATUS_SUCCESS);
        m->AddressingReset = PP_RNDIS_NTOHL(1); /* Make it look like we did something */

        /* m->AddressingReset = 0; - Windows halts if set to 1 for some reason */
        rndis_report();
      }
      break;

    case PP_RNDIS_NTOHL(REMOTE_NDIS_KEEPALIVE_MSG):
      {
        rndis_keepalive_cmplt_t * m;
        m = (rndis_keepalive_cmplt_t *)encapsulated_buffer;
        m->MessageType = PP_RNDIS_NTOHL(REMOTE_NDIS_KEEPALIVE_CMPLT);
        m->MessageLength = tu_le32toh(sizeof(rndis_keepalive_cmplt_t));
        m->Status = PP_RNDIS_NTOHL(RNDIS_STATUS_SUCCESS);

        /* We have data to send back */
        rndis_report();
      }
      break;

    default:
      break;
  }
}
