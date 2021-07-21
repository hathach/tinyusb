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
*/
/*
Some smartphones *may* work with this implementation as well, but likely have limited (broken) drivers,
and likely their manufacturer has not tested such functionality.  Some code workarounds could be tried:

The smartphone may only have an ECM driver, but refuse to automatically pick ECM (unlike the OSes above);
try modifying ./examples/devices/net_lwip_webserver/usb_descriptors.c so that CONFIG_ID_ECM is default.

The smartphone may be artificially picky about which Ethernet MAC address to recognize; if this happens, 
try changing the first byte of tud_network_mac_address[] below from 0x02 to 0x00 (clearing bit 1).
*/
#include "components/lwip/port/esp32/include/lwipopts.h"
#include "sdkconfig.h"
#include "lwip/opt.h"

#include "bsp/board.h"
#include "tusb.h"
#include "unistd.h"

#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <sys/param.h> 
#include "esp_usb_netif_glue.h"
#include "components/esp_netif/lwip/esp_netif_lwip_internal.h"

#if (CFG_TUSB_MCU != OPT_MCU_ESP32S2)
  #include "httpd.h"
#else
  #include "esp_http_server.h"
  #include "device/dcd.h"

  #include "lwip/ip4_addr.h"
  #include "esp_netif.h"
#endif

#define WIFI_SOFTAP 1
#define HTTP_SERVER 1

/* lwip context */
static esp_netif_t *usb_netif=NULL;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

#if 0
/* network parameters of this MCU */
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] =
{
    /* mac ip address                          lease time */
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 2), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 3), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t dhcp_config =
{
    .router = IPADDR4_INIT_BYTES(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                                /* listen port */
    .dns = IPADDR4_INIT_BYTES(192, 168, 7, 1), /* dns server (if any) */
    "usb",                                     /* dns suffix */
    TU_ARRAY_SIZE(entries),                    /* num entry */
    entries                                    /* entries */
};
#endif

/**
 * @brief Free resources allocated in L2 layer
 *
 * @param buf memory alloc in L2 layer
 * @note this function is also the callback when invoke pbuf_free
 */
static void ethernet_free_rx_buf_l2(struct netif *netif, void *buf)
{
    free(buf);
}

/**
 * @brief This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif lwip network interface structure for this ethernetif
 * @param buffer ethernet buffer
 * @param len length of buffer
 */
void ethernetif_input(void *h, void *buffer, size_t len, void *eb)
{
    struct netif *netif = h;
    struct pbuf *p;

    if (unlikely(buffer == NULL || !netif_is_up(netif))) {
        if (buffer) {
            ethernet_free_rx_buf_l2(netif, buffer);
        }
        return;
    }

    /* acquire new pbuf, type: PBUF_REF */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_REF);
    if (p == NULL) {
        ethernet_free_rx_buf_l2(netif, buffer);
        return;
    }

    p->payload = buffer;
#if ESP_LWIP
    p->l2_owner = netif;
    p->l2_buf = buffer;
#endif

    /* full packet send to tcpip_thread to process */
    if (unlikely(netif->input(p, netif) != ERR_OK)) {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
    }
    /* the pbuf will be free in upper layer, eg: ethernet_input */
}

/**
 * Set up the network interface. It calls the function low_level_init() to do the
 * actual init work of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the ethernetif is initialized
 */
err_t ethernetif_init(struct netif *netif)
{
  printf("ethernetif_init start\n");
    LWIP_ASSERT("netif != NULL", (netif != NULL));
#if 0

    /* Have to get the esp-netif handle from netif first and then driver==ethernet handle from there */
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    esp_eth_handle_t eth_handle = esp_netif_get_io_driver(esp_netif);
    /* Initialize interface hostname */
#if LWIP_NETIF_HOSTNAME
#if ESP_LWIP
    if (esp_netif_get_hostname(esp_netif, &netif->hostname) != ESP_OK) {
        netif->hostname = CONFIG_LWIP_LOCAL_HOSTNAME;
    }
#else
    netif->hostname = "lwip";
#endif

#endif /* LWIP_NETIF_HOSTNAME */

    /* Initialize the snmp variables and counters inside the struct netif. */
    eth_speed_t speed;

    esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &speed);
    if (speed == ETH_SPEED_100M) {
        NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000);
    } else {
        NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 10000000);
    }

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
    netif->linkoutput = ethernet_low_level_output;
    netif->l2_buffer_free_notify = ethernet_free_rx_buf_l2;

    ethernet_low_level_init(netif);
#endif
    return ERR_OK;
}


static const struct esp_netif_netstack_config s_usb_netif_config = {
        .lwip = {
            .init_fn = ethernetif_init,
            .input_fn = ethernetif_input
        }
};

#define ESP_NETIF_INHERENT_DEFAULT_USB_CDC \
    {   \
        .flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_GARP | ESP_NETIF_FLAG_EVENT_IP_MODIFIED | NETIF_FLAG_ETHERNET), \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac) \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
        .get_ip_event = IP_EVENT_ETH_GOT_IP, \
        .lost_ip_event = 0, \
        .if_key = "USB_DEF", \
        .if_desc = "usb", \
        .route_prio = 60 \
    };

const esp_netif_inherent_config_t _g_esp_netif_inherent_cdc_config = ESP_NETIF_INHERENT_DEFAULT_USB_CDC;
const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_cdc = &s_usb_netif_config;

#define ESP_NETIF_BASE_DEFAULT_USB_CDC             &_g_esp_netif_inherent_cdc_config
#define ESP_NETIF_NETSTACK_DEFAULT_USB_CDC          _g_esp_netif_netstack_default_cdc

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
  printf("linkoutput_fn start");
  (void)netif;

  for (;;)
  {
    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
    if (!tud_ready())
      return ERR_USE;

    /* if the network driver can accept another packet, we make it happen */
    if (tud_network_can_xmit())
    {
      tud_network_xmit(p, 0 /* unused for this example */);
      return ERR_OK;
    }

    printf("linkoutput_fn running");
    /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
    tud_task();
  }
}

static err_t output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr)
{
  printf("output_fn start");
  return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'u';
  netif->name[1] = 'b';
  netif->linkoutput = linkoutput_fn;
  netif->output = output_fn;
  netif->hwaddr_len = ETH_HWADDR_LEN;
  return ERR_OK;
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    printf( "Ethernet Got IP Address");
    printf( "~~~~~~~~~~~");
    printf( "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    printf( "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    printf( "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    printf( "~~~~~~~~~~~");
}

static void init_lwip(void)
{
  printf("init_lwip start\n"); 
  printf("init_lwip %d\n", __LINE__);

  esp_netif_config_t cfg;                               
  cfg.base = ESP_NETIF_BASE_DEFAULT_USB_CDC;      
  cfg.driver = NULL;  
  cfg.stack = ESP_NETIF_NETSTACK_DEFAULT_USB_CDC; 
  usb_netif = esp_netif_new(&cfg);

  struct netif *netif = usb_netif->lwip_netif;

  uint8_t mac[6];
  memcpy(mac, tud_network_mac_address, sizeof(tud_network_mac_address));
  mac[5] ^= 0x01;
  esp_netif_set_mac(usb_netif, mac);

  {    
    const ip_addr_t test_local_ip = IPADDR4_INIT_BYTES(192, 168, 3, 1);
    netif->ip_addr = test_local_ip;
  }
  // Set default handlers to process TCP/IP stuffs
  ESP_ERROR_CHECK(esp_usb_set_default_handlers(usb_netif));
  ////ESP_ERROR_CHECK(esp_event_handler_register(USB_EVENT, ESP_EVENT_ANY_ID, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
  
  esp_usb_config_t config = USB_DEFAULT_CONFIG(mac);
  esp_usb_handle_t usb_handle = NULL;
  ESP_ERROR_CHECK(esp_usb_driver_install(&config, &usb_handle));

  /* attach usb cdc-ecn driver to TCP/IP stack */
  ESP_ERROR_CHECK(esp_netif_attach(usb_netif, esp_usb_new_netif_glue(usb_handle)));

  /* start USB driver state machine */
  ESP_ERROR_CHECK(esp_usb_start(usb_handle));

  esp_netif_dhcps_stop(usb_netif);
  esp_netif_ip_info_t ip_info;
  IP4_ADDR(&ip_info.ip, 192, 168, 3, 1);
  IP4_ADDR(&ip_info.gw, 192, 168, 3, 1);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
  esp_netif_set_ip_info(usb_netif, &ip_info);
  esp_netif_dhcps_start(usb_netif);

  netif_init_cb(usb_netif->lwip_netif);
  netif_init_cb(usb_netif->netif_handle);
  printf("init_lwip end\n");
}

void send_bus_signal (void)
{
  dcd_event_t event = { .rhport = 0, .event_id = USBD_EVENT_DATA_AVAIL };
  dcd_event_handler(&event, false);
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  /* this shouldn't happen, but if we get another packet before 
  parsing the previous, we must signal our inability to accept it */
  if (received_frame) return false;

  if (size)
  {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if (p)
    {
      /* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
      memcpy(p->payload, src, size);

      /* store away the pointer for service_traffic() to later handle */
      received_frame = p;

      send_bus_signal();
    }
  }

  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  struct pbuf *p = (struct pbuf *)ref;
  struct pbuf *q;
  uint16_t len = 0;

  (void)arg; /* unused for this example */

  /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
  for(q = p; q != NULL; q = q->next)
  {
    memcpy(dst, (char *)q->payload, q->len);
    dst += q->len;
    len += q->len;
    if (q->len == q->tot_len) break;
  }

  return len;
}

static void service_traffic(void)
{
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame)
  {
    esp_netif_receive(usb_netif, received_frame->payload, received_frame->len, NULL);
    if (received_frame->ref > 0) 
    { 
      pbuf_free(received_frame); 
    }
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

#if WIFI_SOFTAP

/* The examples use WiFi configuration that you can set via project configuration menu.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    printf("wifi_event_handler start\n");
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
printf("######3\n");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192, 168, 2, 100);
	  IP4_ADDR(&ipInfo.gw, 192, 168, 2, 100);
	  IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	  esp_netif_dhcps_stop(wifiAP);
	  esp_netif_set_ip_info(wifiAP, &ipInfo);
    esp_netif_dhcps_start(wifiAP);

    ESP_ERROR_CHECK(esp_wifi_start());
printf("######10\n");
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}
#endif

#if HTTP_SERVER

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    const char resp[] = "URI GET Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    /* Send a simple response */
    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/uri",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri      = "/uri",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}
#endif //HTTP_SERVER

#if (CFG_TUSB_MCU == OPT_MCU_ESP32S2)
int app_main(void)
#else
int main(void)
#endif
{
  /* initialize TinyUSB */
  board_init();
  tusb_init();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

printf("######1\n");
  #if WIFI_SOFTAP
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
  #endif //WIFI

printf("######2\n");
  /* initialize lwip, dhcp-server, dns-server, and http */
  init_lwip();

  struct netif *netif = usb_netif->netif_handle;
printf("###### usb_netif: %p %p\n", usb_netif, netif);
printf("###### usb_netif: %d %d\n", netif_is_up(usb_netif), netif_is_up(netif));
  while (!netif_is_up(netif)) { usleep(2000*1000); printf("###### while\n");};
printf("######3\n");

  #if (0)
    httpd_init();
  #else
    start_webserver();
  #endif

  while (1)
  {
    printf("main loop start\n");
    tud_task();
    service_traffic();
  }

  return 0;
}


#if (CFG_TUSB_MCU != OPT_MCU_ESP32S2)
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
#endif
