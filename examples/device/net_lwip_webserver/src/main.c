/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Peter Lawrence
 *
 * influenced by lrndis https://github.com/fetisov/lrndis
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
 */

/*
depending on the value of CFG_TUD_NET (tusb_config.h), this can be a RNDIS+CDC-ECM or CDC-EEM USB virtual network adapter

OPT_NET_RNDIS_ECM : RNDIS should be valid on Linux and Windows hosts, and CDC-ECM should be valid on Linux and MacOS hosts
OPT_NET_EEM       : CDC-EEM should be valid on Linux hosts

OPT_NET_RNDIS_ECM should be the best choice, as it makes for a hopefully universal solution.

You *must* customize tusb_config.h to set the CFG_TUD_NET definition to the type of these network option.

The MCU appears to the host as IP address 192.168.7.1, and provides a DHCP server, DNS server, and web server.
*/

#include "bsp/board.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "httpd.h"

/* lwip context */
static struct netif netif_data;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first two bytes are 0x02,0x02 to indicate a link-local address */
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

/* network parameters of this MCU */
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] =
{
  /* mac    ip address        subnet mask        lease time */
  { {0}, {192, 168, 7, 2}, {255, 255, 255, 0}, 24 * 60 * 60 },
  { {0}, {192, 168, 7, 3}, {255, 255, 255, 0}, 24 * 60 * 60 },
  { {0}, {192, 168, 7, 4}, {255, 255, 255, 0}, 24 * 60 * 60 }
};

/* DHCP configuration parameters, leveraging "entries" above */
static const dhcp_config_t dhcp_config =
{
  {192, 168, 7, 1}, 67,    /* server address (self), port */
  {192, 168, 7, 1},        /* dns server (self) */
  "usb",                   /* dns suffix */
  TU_ARRAY_SIZE(entries),  /* number of entries */
  entries                  /* pointer to entries */
};

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
  (void)netif;

  for (;;)
  {
    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
    if (!tud_ready())
      return ERR_USE;

    /* if the network driver can accept another packet, we make it happen */
    if (tud_network_can_xmit())
    {
      tud_network_xmit(p);
      return ERR_OK;
    }

    /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
    tud_task();
  }
}

static err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *addr)
{
  return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn;
  netif->output = output_fn;
  return ERR_OK;
}

static void init_lwip(void)
{
  struct netif *netif = &netif_data;

  lwip_init();

  /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;

  netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ip_input);
  netif_set_default(netif);
}

/* handle any DNS requests from dns-server */
bool dns_query_proc(const char *name, ip_addr_t *addr)
{
  if (0 == strcmp(name, "tiny.usb"))
  {
    *addr = ipaddr;
    return true;
  }
  return false;
}

bool tud_network_recv_cb(struct pbuf *p)
{
  /* this shouldn't happen, but if we get another packet before 
  parsing the previous, we must signal our inability to accept it */
  if (received_frame) return false;

  /* store away the pointer for service_traffic() to later handle */
  received_frame = p;
  return true;
}

static void service_traffic(void)
{
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame)
  {
    ethernet_input(received_frame, &netif_data);
    pbuf_free(received_frame);
    received_frame = NULL;
    tud_network_recv_renew();
  }

  sys_check_timeouts();
}

void tud_network_init_cb(void)
{
  /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
  if (received_frame)
  {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
}

int main(void)
{
  /* initialize TinyUSB */
  board_init();
  tusb_init();

  /* initialize lwip, dhcp-server, dns-server, and http */
  init_lwip();
  while (!netif_is_up(&netif_data));
  while (dhserv_init(&dhcp_config) != ERR_OK);
  while (dnserv_init(&ipaddr, 53, dns_query_proc) != ERR_OK);
  httpd_init();

  while (1)
  {
    tud_task();
    service_traffic();
  }

  return 0;
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void)
{
  return 0;
}
void sys_arch_unprotect(sys_prot_t pval)
{
  (void)pval;
}

/* lwip needs a millisecond time source, and the TinyUSB board support code has one available */
uint32_t sys_now(void)
{
  return board_millis();
}
