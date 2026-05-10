/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Andrew Leech
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

/*
 * USB/IP-over-Ethernet host bridge example.
 *
 * TinyUSB host stack + lwIP raw API + DHCP + a USB/IP server on
 * port 3240. A USB device on the OTG_FS host port can be attached
 * to a Linux box over the LAN with `usbip attach`; the kernel sees
 * it as a local device (cdc-acm becomes /dev/ttyACM<N>, etc).
 *
 *   board UART log (representative):
 *     usbipd example: host + lwip + dhcp + usbip
 *       rhport=0  speed=FS
 *     tusb_init ok
 *     dhcp: got 192.168.1.42
 *     usbip: listening on tcp/3240
 *     MOUNT: daddr=1 rhport=0 speed=FS vid=0x2e8a pid=0x000c
 *
 * From the host:
 *     usbip list -r 192.168.1.42
 *     sudo usbip attach -r 192.168.1.42 -b 1-1
 *
 * Requires a TinyUSB host port plus an lwIP-capable Ethernet
 * interface and a network with a DHCP server. See README for the
 * supported boards.
 */

#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"

#include "ethernetif.h"
#include "usbip_server.h"

static struct netif gnetif;
static bool dhcp_started = false;
static ip_addr_t last_logged_ip;
static bool server_started = false;

static void mount_log(uint8_t daddr);

int main(void) {
  board_init();
  printf("\nusbipd example: host + lwip + dhcp + usbip\n");
  printf("  rhport=%u  speed=%s\n",
         BOARD_TUH_RHPORT,
         BOARD_TUH_MAX_SPEED == OPT_MODE_HIGH_SPEED ? "HS" :
         BOARD_TUH_MAX_SPEED == OPT_MODE_FULL_SPEED ? "FS" : "default");

  tusb_rhport_init_t host_init = {
      .role  = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO,
  };
  if (!tusb_init(BOARD_TUH_RHPORT, &host_init)) {
    printf("tusb_init failed\n");
    return 1;
  }
  board_init_after_tusb();
  printf("tusb_init ok\n");

  // lwIP, no DHCP yet (kicked off when the link comes up).
  lwip_init();
  ip_addr_t ip = {0}, nm = {0}, gw = {0};
  netif_add(&gnetif, ip_2_ip4(&ip), ip_2_ip4(&nm), ip_2_ip4(&gw),
            NULL, ethernetif_init, ethernet_input);
  netif_set_default(&gnetif);
  netif_set_up(&gnetif);

  ip_addr_set_zero(&last_logged_ip);

  while (1) {
    tuh_task();
    ethernetif_input(&gnetif);
    sys_check_timeouts();
    ethernet_link_check_state(&gnetif);

    if (netif_is_link_up(&gnetif) && !dhcp_started) {
      printf("eth: up, starting dhcp\n");
      dhcp_start(&gnetif);
      dhcp_started = true;
    }
    if (!netif_is_link_up(&gnetif) && dhcp_started) {
      printf("eth: down\n");
      dhcp_stop(&gnetif);
      dhcp_started = false;
      server_started = false;
    }

    if (dhcp_started && dhcp_supplied_address(&gnetif)) {
      if (!ip_addr_cmp(&last_logged_ip, &gnetif.ip_addr)) {
        ip_addr_copy(last_logged_ip, gnetif.ip_addr);
        printf("dhcp: got %s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
      }
      if (!server_started) {
        usbip_server_init();
        printf("usbip: listening on tcp/3240\n");
        server_started = true;
      }
    }
  }

  return 0;
}

//--------------------------------------------------------------------+
// TinyUSB host callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t daddr) {
  mount_log(daddr);
  usbip_server_on_mount(daddr);
}

void tuh_umount_cb(uint8_t daddr) {
  printf("UMOUNT: daddr=%u\n", daddr);
  usbip_server_on_umount(daddr);
}

static void mount_log(uint8_t daddr) {
  tuh_bus_info_t bus_info;
  if (!tuh_bus_info_get(daddr, &bus_info)) {
    printf("MOUNT: daddr=%u (bus_info unavailable)\n", daddr);
    return;
  }

  uint16_t vid = 0, pid = 0;
  tuh_vid_pid_get(daddr, &vid, &pid);

  printf("MOUNT: daddr=%u rhport=%u speed=%s vid=0x%04x pid=0x%04x\n",
         daddr,
         bus_info.rhport,
         bus_info.speed == TUSB_SPEED_HIGH ? "HS" :
         bus_info.speed == TUSB_SPEED_FULL ? "FS" : "LS",
         vid, pid);
}
