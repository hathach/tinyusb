// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "components/lwip/port/esp32/include/lwipopts.h"
#include "sdkconfig.h"

#include <stdlib.h>
#include "esp_netif.h"
#include "esp_usb.h"
#include "esp_usb_netif_glue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "string.h"
#include "components/esp_netif/lwip/esp_netif_lwip_internal.h"

const static char *TAG = "esp_usb.netif.glue";

typedef struct {
    esp_netif_driver_base_t base;
    esp_usb_handle_t usb_driver;
} esp_usb_netif_glue_t;

static esp_err_t usb_input_to_netif(esp_usb_handle_t usb_handle, uint8_t *buffer, uint32_t length, void *priv)
{
    printf("usb_input_to_netif %d\n", __LINE__);
    return esp_netif_receive((esp_netif_t *)priv, buffer, length, NULL);
}

static esp_err_t esp_usb_post_attach(esp_netif_t *esp_netif, void *args)
{
    printf("esp_usb_post_attach %d\n", __LINE__);
    uint8_t usb_mac[6];
    esp_usb_netif_glue_t *glue = (esp_usb_netif_glue_t *)args;
    glue->base.netif = esp_netif;

    esp_usb_update_input_path(glue->usb_driver, usb_input_to_netif, esp_netif);

    // set driver related config to esp-netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  glue->usb_driver,
        .transmit = esp_usb_transmit,
        .driver_free_rx_buffer = NULL
    };

    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    esp_usb_ioctl(glue->usb_driver, ETH_CMD_G_MAC_ADDR, usb_mac);
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x", usb_mac[0], usb_mac[1],
             usb_mac[2], usb_mac[3], usb_mac[4], usb_mac[5]);

    esp_netif_set_mac(esp_netif, usb_mac);
    ESP_LOGI(TAG, "usbernet attached to netif");

    //printf("**********************************\n");
    //esp_netif_action_start(esp_netif, NULL, 0, NULL);
    return ESP_OK;
}

void *esp_usb_new_netif_glue(esp_usb_handle_t usb_hdl)
{
    printf("esp_usb_new_netif_glue %d\n", __LINE__);
    esp_usb_netif_glue_t *glue = calloc(1, sizeof(esp_usb_netif_glue_t));
    if (!glue) {
        ESP_LOGE(TAG, "create netif glue failed");
        return NULL;
    }
    glue->usb_driver = usb_hdl;
    glue->base.post_attach = esp_usb_post_attach;
    printf("esp_usb_new_netif_glue %d\n", __LINE__);
    esp_usb_increase_reference(usb_hdl);
    printf("esp_usb_new_netif_glue %d\n", __LINE__);
    return &glue->base;
}

esp_err_t esp_usb_del_netif_glue(void *g)
{
    esp_usb_netif_glue_t *glue = (esp_usb_netif_glue_t *)g;
    esp_usb_decrease_reference(glue->usb_driver);
    free(glue);
    return ESP_OK;
}

esp_err_t esp_usb_clear_default_handlers(void *esp_netif)
{
    if (!esp_netif) {
        ESP_LOGE(TAG, "esp-netif handle can't be null");
        return ESP_ERR_INVALID_ARG;
    }
    esp_event_handler_unregister(USB_EVENT, USBNET_EVENT_START, esp_netif_action_start);
    esp_event_handler_unregister(USB_EVENT, USBNET_EVENT_STOP, esp_netif_action_stop);
    esp_event_handler_unregister(USB_EVENT, USBNET_EVENT_CONNECTED, esp_netif_action_connected);
    esp_event_handler_unregister(USB_EVENT, USBNET_EVENT_DISCONNECTED, esp_netif_action_disconnected);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, esp_netif_action_got_ip);

    return ESP_OK;
}

esp_err_t esp_usb_set_default_handlers(void *esp_netif)
{
    esp_err_t ret;

    if (!esp_netif) {
        ESP_LOGE(TAG, "esp-netif handle can't be null");
        return ESP_ERR_INVALID_ARG;
    }

    struct netif *netif = ((esp_netif_t *)esp_netif)->netif_handle;
    netif->flags = ((esp_netif_t *)esp_netif)->flags;

    ret = esp_event_handler_register(USB_EVENT, USBNET_EVENT_START, esp_netif_action_start, esp_netif);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_register(USB_EVENT, USBNET_EVENT_STOP, esp_netif_action_stop, esp_netif);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_register(USB_EVENT, USBNET_EVENT_CONNECTED, esp_netif_action_connected, esp_netif);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_register(USB_EVENT, USBNET_EVENT_DISCONNECTED, esp_netif_action_disconnected, esp_netif);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, esp_netif_action_got_ip, esp_netif);
    if (ret != ESP_OK) {
        goto fail;
    }

    return ESP_OK;

fail:
    esp_usb_clear_default_handlers(esp_netif);
    return ret;
}
