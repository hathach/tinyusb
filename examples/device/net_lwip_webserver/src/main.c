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
this appears as either a RNDIS or CDC-ECM USB virtual network adapter; the OS picks its preference

RNDIS should be valid on Linux and Windows hosts, and CDC-ECM should be valid on Linux and macOS hosts

The MCU appears to the host as IP address 192.168.7.1, and provides a DHCP server, DNS server, and web server.

Link State Control:
- Press the user button to toggle the network link state (UP/DOWN)
- This simulates "ethernet cable unplugged/plugged" events
- The host OS will see the network interface as disconnected/connected accordingly
- Use this to test network error handling and recovery in host applications
*/
/*
Some smartphones *may* work with this implementation as well, but likely have limited (broken) drivers,
and likely their manufacturer has not tested such functionality.  Some code workarounds could be tried:

The smartphone may only have an ECM driver, but refuse to automatically pick ECM (unlike the OSes above);
try modifying ./examples/devices/net_lwip_webserver/usb_descriptors.c so that CONFIG_ID_ECM is default.

The smartphone may be artificially picky about which Ethernet MAC address to recognize; if this happens,
try changing the first byte of tud_network_mac_address[] below from 0x02 to 0x00 (clearing bit 1).
*/

#include "bsp/board_api.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "httpd.h"
#include "lwip/ethip6.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"

#ifdef INCLUDE_IPERF
  #include "lwip/apps/lwiperf.h"
#endif

#define INIT_IP4(a, b, c, d) \
  { PP_HTONL(LWIP_MAKEU32(a, b, c, d)) }

/* lwip context */
static struct netif netif_data;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

/* network parameters of this MCU */
static const ip4_addr_t ipaddr = INIT_IP4(192, 168, 7, 1);
static const ip4_addr_t netmask = INIT_IP4(255, 255, 255, 0);
static const ip4_addr_t gateway = INIT_IP4(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] = {
    /* mac ip address               lease time */
    {{0}, INIT_IP4(192, 168, 7, 2), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 7, 3), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 7, 4), 24 * 60 * 60},
};

static const dhcp_config_t dhcp_config = {
    .router = INIT_IP4(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                      /* listen port */
    .dns = INIT_IP4(192, 168, 7, 1), /* dns server (if any) */
    "usb",                           /* dns suffix */
    TU_ARRAY_SIZE(entries),          /* num entry */
    entries                          /* entries */
};

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
  (void) netif;

  for (;;) {
    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
    if (!tud_ready())
      return ERR_USE;

    /* if the network driver can accept another packet, we make it happen */
    if (tud_network_can_xmit(p->tot_len)) {
      tud_network_xmit(p, 0 /* unused for this example */);
      return ERR_OK;
    }

    /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
    tud_task();
  }
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr) {
  return etharp_output(netif, p, addr);
}

#if LWIP_IPV6
static err_t ip6_output_fn(struct netif *netif, struct pbuf *p, const ip6_addr_t *addr) {
  return ethip6_output(netif, p, addr);
}
#endif

static err_t netif_init_cb(struct netif *netif) {
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn;
  netif->output = ip4_output_fn;
#if LWIP_IPV6
  netif->output_ip6 = ip6_output_fn;
#endif
  return ERR_OK;
}

/* notifies the USB host about the link state change. */
static void usbnet_netif_link_callback(struct netif *netif) {
    bool link_up = netif_is_link_up(netif);
    tud_network_link_state(BOARD_TUD_RHPORT, link_up);
}

static void init_lwip(void) {
  struct netif *netif = &netif_data;

  lwip_init();

  /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;

  netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ethernet_input);
#if LWIP_IPV6
  netif_create_ip6_linklocal_address(netif, 1);
#endif
  netif_set_default(netif);

#if LWIP_NETIF_LINK_CALLBACK
  // Set the link callback to notify USB host about link state changes
  netif_set_link_callback(netif, usbnet_netif_link_callback);
  netif_set_link_up(netif);
#else
  tud_network_link_state(BOARD_TUD_RHPORT, true);
#endif
}

/* handle any DNS requests from dns-server */
bool dns_query_proc(const char *name, ip4_addr_t *addr) {
  if (0 == strcmp(name, "tiny.usb")) {
    *addr = ipaddr;
    return true;
  }
  return false;
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  struct netif *netif = &netif_data;

  if (size) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if (p == NULL) {
      printf("ERROR: Failed to allocate pbuf of size %d\n", size);
      return false;
    }

    /* Copy buf to pbuf */
    pbuf_take(p, src, size);

    // Surrender ownership of our pbuf unless there was an error
    // Only call pbuf_free if not Ok else it will panic with "pbuf_free: p->ref > 0"
    // or steal it from whatever took ownership of it with undefined consequences.
    // See: https://savannah.nongnu.org/patch/index.php?10121
    if (netif->input(p, netif) != ERR_OK) {
      printf("ERROR: netif input failed\n");
      pbuf_free(p);
    }
    // Signal tinyusb that the current frame has been processed.
    tud_network_recv_renew();
  }

  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  struct pbuf *p = (struct pbuf *) ref;

  (void) arg; /* unused for this example */

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

static void handle_link_state_switch(void) {
  /* Check for button press to toggle link state */
  static bool last_link_state = true;
  static bool last_button_state = false;
  bool current_button_state = board_button_read();

  if (current_button_state && !last_button_state) {
    /* Button pressed - toggle link state */
    last_link_state = !last_link_state;
    if (last_link_state) {
      printf("Link state: UP\n");
      netif_set_link_up(&netif_data);
    } else {
      printf("Link state: DOWN\n");
      netif_set_link_down(&netif_data);
    }
    /* LWIP callback will notify USB host about the change */
  }
  last_button_state = current_button_state;

}

int main(void) {
  /* initialize TinyUSB */
  board_init();

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  /* initialize lwip, dhcp-server, dns-server, and http */
  init_lwip();
  while (!netif_is_up(&netif_data));
  while (dhserv_init(&dhcp_config) != ERR_OK);
  while (dnserv_init(IP_ADDR_ANY, 53, dns_query_proc) != ERR_OK);
  httpd_init();

#ifdef INCLUDE_IPERF
  // test with: iperf -c 192.168.7.1 -e -i 1 -M 5000 -l 8192 -r
  lwiperf_start_tcp_server_default(NULL, NULL);
#endif

#if CFG_TUD_NCM
  printf("USB NCM network interface initialized\n");
#elif CFG_TUD_ECM_RNDIS
  printf("USB RNDIS/ECM network interface initialized\n");
#endif

  while (1) {
    tud_task();
    sys_check_timeouts(); // service lwip
    handle_link_state_switch();
  }

  return 0;
}

/* lwip has provision for using a mutex, when applicable */
/* This implementation is for single-threaded use only */
sys_prot_t sys_arch_protect(void) {
  return 0;
}
void sys_arch_unprotect(sys_prot_t pval) {
  (void) pval;
}

/* lwip needs a millisecond time source, and the TinyUSB board support code has one available */
uint32_t sys_now(void) {
  return board_millis();
}
