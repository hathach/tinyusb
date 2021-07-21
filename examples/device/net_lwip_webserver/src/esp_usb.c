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

#include <sys/cdefs.h>
#include <stdatomic.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_usb.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_heap_caps.h"
#include "string.h"

static const char *TAG = "esp_usb";

ESP_EVENT_DEFINE_BASE(USB_EVENT);

typedef enum {
    ESP_USB_FSM_STOP,
    ESP_USB_FSM_START
} esp_usb_fsm_t;

/**
 * @brief The usbernet driver mainly consists of PHY, MAC and
 * the mediator who will handle the request/response from/to MAC, PHY and Users.
 * usbernet driver adopts an OS timer to check the link status periodically.
 * This structure preserves some important usbernet attributes (e.g. speed, duplex, link).
 * Function stack_input is the channel which set by user, it will deliver all received packets.
 * If stack_input is set to NULL, then all received packets will be passed to tcp/ip stack.
 * on_lowlevel_init_done and on_lowlevel_deinit_done are callbacks set by user.
 * In the callback, user can do any low level operations (e.g. enable/disable crystal clock).
 */
typedef struct {
    esp_usb_mediator_t mediator;
    //esp_usb_phy_t *phy;
    //esp_usb_mac_t *mac;
    uint8_t macaddr[6];
    TimerHandle_t check_link_timer;
    usb_speed_t speed;
    usb_duplex_t duplex;
    usb_link_t link;
    atomic_int ref_count;
    void *priv;
    _Atomic esp_usb_fsm_t fsm;
    esp_err_t (*stack_input)(esp_usb_handle_t usb_handle, uint8_t *buffer, uint32_t length, void *priv);
    esp_err_t (*on_lowlevel_init_done)(esp_usb_handle_t usb_handle);
    esp_err_t (*on_lowlevel_deinit_done)(esp_usb_handle_t usb_handle);
} esp_usb_driver_t;

////////////////////////////////Mediator Functions////////////////////////////////////////////
// Following functions are owned by mediator, which will get invoked by MAC or PHY.
// Mediator functions need to find the right actor (MAC, PHY or user) to perform the operation.
// So in the head of mediator function, we have to get the esp_usb_driver_t pointer.
// With this pointer, we could deliver the task to the real actor (MAC, PHY or user).
// This might sound excessive, but is helpful to separate the PHY with MAC (they can not contact with each other directly).
// For more details, please refer to WiKi. https://en.wikipedia.org/wiki/Mediator_pattern
//////////////////////////////////////////////////////////////////////////////////////////////

static esp_err_t usb_phy_reg_read(esp_usb_mediator_t *usb, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    //esp_usb_driver_t *usb_driver = __containerof(usb, esp_usb_driver_t, mediator);
    //esp_usb_mac_t *mac = usb_driver->mac;
    //return mac->read_phy_reg(mac, phy_addr, phy_reg, reg_value);
    return ESP_OK;
}

static esp_err_t usb_phy_reg_write(esp_usb_mediator_t *usb, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    //esp_usb_driver_t *usb_driver = __containerof(usb, esp_usb_driver_t, mediator);
    //esp_usb_mac_t *mac = usb_driver->mac;
    //return mac->write_phy_reg(mac, phy_addr, phy_reg, reg_value);
    return ESP_OK;
}

static esp_err_t usb_stack_input(esp_usb_mediator_t *usb, uint8_t *buffer, uint32_t length)
{
    //esp_usb_driver_t *usb_driver = __containerof(usb, esp_usb_driver_t, mediator);
    //if (usb_driver->stack_input) {
    //    return usb_driver->stack_input((esp_usb_handle_t)usb_driver, buffer, length, usb_driver->priv);
    //} else {
    //    free(buffer);
    //    return ESP_OK;
    //}
    return ESP_OK;
}

static esp_err_t usb_on_state_changed(esp_usb_mediator_t *usb, esp_usb_state_t state, void *args)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = __containerof(usb, esp_usb_driver_t, mediator);
    // esp_usb_mac_t *mac = usb_driver->mac;
    switch (state) {
    case USB_STATE_LLINIT: {
        if (usb_driver->on_lowlevel_init_done) {
            ESP_GOTO_ON_ERROR(usb_driver->on_lowlevel_init_done(usb_driver), err, TAG, "extra lowlevel init failed");
        }
        break;
    }
    case USB_STATE_DEINIT: {
        if (usb_driver->on_lowlevel_deinit_done) {
            ESP_GOTO_ON_ERROR(usb_driver->on_lowlevel_deinit_done(usb_driver), err, TAG, "extra lowlevel deinit failed");
        }
        break;
    }
    case USB_STATE_LINK: {
        usb_link_t link = (usb_link_t)args;
        // ESP_GOTO_ON_ERROR(mac->set_link(mac, link), err, TAG, "usbernet mac set link failed");
        usb_driver->link = link;
        if (link == USB_LINK_UP) {
            ESP_GOTO_ON_ERROR(esp_event_post(USB_EVENT, USBNET_EVENT_CONNECTED, &usb_driver, sizeof(esp_usb_driver_t *), 0), err,
                              TAG, "send usbERNET_EVENT_CONNECTED event failed");
        } else if (link == USB_LINK_DOWN) {
            ESP_GOTO_ON_ERROR(esp_event_post(USB_EVENT, USBNET_EVENT_DISCONNECTED, &usb_driver, sizeof(esp_usb_driver_t *), 0), err,
                              TAG, "send usbERNET_EVENT_DISCONNECTED event failed");
        }
        break;
    }
    case USB_STATE_SPEED: {
        // usb_speed_t speed = (usb_speed_t)args;
        // ESP_GOTO_ON_ERROR(mac->set_speed(mac, speed), err, TAG, "usbernet mac set speed failed");
        // usb_driver->speed = speed;
        break;
    }
    case USB_STATE_DUPLEX: {
        // usb_duplex_t duplex = (usb_duplex_t)args;
        // ESP_GOTO_ON_ERROR(mac->set_duplex(mac, duplex), err, TAG, "usbernet mac set duplex failed");
        // usb_driver->duplex = duplex;
        break;
    }
    case USB_STATE_PAUSE: {
        // uint32_t peer_pause_ability = (uint32_t)args;
        // ESP_GOTO_ON_ERROR(mac->set_peer_pause_ability(mac, peer_pause_ability), err, TAG, "usbernet mac set peer pause ability failed");
        break;
    }
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "unknown usbernet state: %d", state);
        break;
    }
    return ESP_OK;
err:
    return ret;
}

static void usb_check_link_timer_cb(TimerHandle_t xTimer)
{
    // esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)pvTimerGetTimerID(xTimer);
    // esp_usb_phy_t *phy = usb_driver->phy;
    // esp_usb_increase_reference(usb_driver);
    // phy->get_link(phy);
    // esp_usb_decrease_reference(usb_driver);
}

////////////////////////////////User face APIs////////////////////////////////////////////////
// User has to pass the handle of usbernet driver to each API.
// Different usbernet driver instance is identified with a unique handle.
// It's helpful for us to support multiple usbernet port on ESP32.
//////////////////////////////////////////////////////////////////////////////////////////////

esp_err_t esp_usb_driver_install(const esp_usb_config_t *config, esp_usb_handle_t *out_hdl)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(config, ESP_ERR_INVALID_ARG, err, TAG, "usb config can't be null");
    ESP_GOTO_ON_FALSE(out_hdl, ESP_ERR_INVALID_ARG, err, TAG, "usb handle can't be null");
    // esp_usb_mac_t *mac = config->mac;
    // esp_usb_phy_t *phy = config->phy;
    // ESP_GOTO_ON_FALSE(mac && phy, ESP_ERR_INVALID_ARG, err, TAG, "can't set usb->mac or usb->phy to null");
    // usb_driver contains an atomic variable, which should not be put in PSRAM
    esp_usb_driver_t *usb_driver = heap_caps_calloc(1, sizeof(esp_usb_driver_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_NO_MEM, err, TAG, "request memory for usb_driver failed");
    atomic_init(&usb_driver->ref_count, 1);
    atomic_init(&usb_driver->fsm, ESP_USB_FSM_STOP);
    // usb_driver->mac = mac;
    // usb_driver->phy = phy;
    memcpy(usb_driver->macaddr, config->macaddr, sizeof(usb_driver->macaddr)/sizeof(usb_driver->macaddr[0]));
    usb_driver->link = USB_LINK_DOWN;
    usb_driver->duplex = USB_DUPLEX_HALF;
    usb_driver->speed = USB_SPEED_10M;
    usb_driver->stack_input = config->stack_input;
    usb_driver->on_lowlevel_init_done = config->on_lowlevel_init_done;
    usb_driver->on_lowlevel_deinit_done = config->on_lowlevel_deinit_done;
    usb_driver->mediator.phy_reg_read = usb_phy_reg_read;
    usb_driver->mediator.phy_reg_write = usb_phy_reg_write;
    usb_driver->mediator.stack_input = usb_stack_input;
    usb_driver->mediator.on_state_changed = usb_on_state_changed;
    /* some PHY can't output RMII clock if in reset state, so hardware reset PHY chip firstly */
    // phy->reset_hw(phy);
    // ESP_GOTO_ON_ERROR(mac->set_mediator(mac, &usb_driver->mediator), err_mediator, TAG, "set mediator for mac failed");
    // ESP_GOTO_ON_ERROR(phy->set_mediator(phy, &usb_driver->mediator), err_mediator, TAG, "set mediator for phy failed");
    // ESP_GOTO_ON_ERROR(mac->init(mac), err_init_mac, TAG, "init mac failed");
    // ESP_GOTO_ON_ERROR(phy->init(phy), err_init_phy, TAG, "init phy failed");
    usb_driver->check_link_timer = xTimerCreate("usb_link_timer", pdMS_TO_TICKS(config->check_link_period_ms), pdTRUE,
                                   usb_driver, usb_check_link_timer_cb);
    ESP_GOTO_ON_FALSE(usb_driver->check_link_timer, ESP_FAIL, err_create_timer, TAG, "create usb_link_timer failed");
    *out_hdl = (esp_usb_handle_t)usb_driver;

    // for backward compatible to 4.0, and will get removed in 5.0
#if 0//CONFIG_ESP_NETIF_TCPIP_ADAPTER_COMPATIBLE_LAYER
    extern esp_err_t tcpip_adapter_compat_start_usb(void *usb_driver);
    tcpip_adapter_compat_start_usb(usb_driver);
#endif

    return ESP_OK;
 err_create_timer:
//     phy->deinit(phy);
// err_init_phy:
//     mac->deinit(mac);
//err_init_mac:
//err_mediator:
//     free(usb_driver);
err:
    return ret;
}

esp_err_t esp_usb_driver_uninstall(esp_usb_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // check if driver has started
    esp_usb_fsm_t expected_fsm = ESP_USB_FSM_STOP;
    if (!atomic_compare_exchange_strong(&usb_driver->fsm, &expected_fsm, ESP_USB_FSM_STOP)) {
        ESP_LOGW(TAG, "driver not stopped yet");
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
    // don't uninstall driver unless there's only one reference
    int expected_ref_count = 1;
    if (!atomic_compare_exchange_strong(&usb_driver->ref_count, &expected_ref_count, 0)) {
        ESP_LOGE(TAG, "%d usbernet reference in use", expected_ref_count);
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
    // esp_usb_mac_t *mac = usb_driver->mac;
    // esp_usb_phy_t *phy = usb_driver->phy;
    ESP_GOTO_ON_FALSE(xTimerDelete(usb_driver->check_link_timer, 0) == pdPASS, ESP_FAIL, err, TAG, "delete usb_link_timer failed");
    // ESP_GOTO_ON_ERROR(phy->deinit(phy), err, TAG, "deinit phy failed");
    // ESP_GOTO_ON_ERROR(mac->deinit(mac), err, TAG, "deinit mac failed");
    // free(usb_driver);
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_usb_start(esp_usb_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // check if driver has started
printf("%s %d\n", __FILE__, __LINE__);    
    esp_usb_fsm_t expected_fsm = ESP_USB_FSM_STOP;
    if (!atomic_compare_exchange_strong(&usb_driver->fsm, &expected_fsm, ESP_USB_FSM_START)) {
        ESP_LOGW(TAG, "driver started already");
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
printf("%s %d\n", __FILE__, __LINE__);    
    // ESP_GOTO_ON_ERROR(usb_driver->phy->reset(usb_driver->phy), err, TAG, "reset phy failed");
    ESP_GOTO_ON_FALSE(xTimerStart(usb_driver->check_link_timer, 0), ESP_FAIL, err, TAG, "start usb_link_timer failed");
printf("%s %d\n", __FILE__, __LINE__);    
    ESP_GOTO_ON_ERROR(esp_event_post(USB_EVENT, USBNET_EVENT_START, &usb_driver, sizeof(esp_usb_driver_t *), 0), err_event, TAG, "send USBNET_EVENT_START event failed");
    /**tm*/ESP_GOTO_ON_ERROR(esp_event_post(USB_EVENT, USBNET_EVENT_CONNECTED, &usb_driver, sizeof(esp_usb_driver_t *), 0), err_event, TAG, "send USBNET_EVENT_CONNECTED event failed");
printf("%s %d\n", __FILE__, __LINE__);    
    return ESP_OK;
err_event:
    xTimerStop(usb_driver->check_link_timer, 0);
err:
    return ret;
}

esp_err_t esp_usb_stop(esp_usb_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // check if driver has started
    esp_usb_fsm_t expected_fsm = ESP_USB_FSM_START;
    if (!atomic_compare_exchange_strong(&usb_driver->fsm, &expected_fsm, ESP_USB_FSM_STOP)) {
        ESP_LOGW(TAG, "driver not started yet");
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
    // esp_usb_mac_t *mac = usb_driver->mac;
    // ESP_GOTO_ON_ERROR(mac->stop(mac), err, TAG, "stop mac failed");
    ESP_GOTO_ON_FALSE(xTimerStop(usb_driver->check_link_timer, 0), ESP_FAIL, err, TAG, "stop usb_link_timer failed");
    ESP_GOTO_ON_ERROR(esp_event_post(USB_EVENT, USBNET_EVENT_STOP, &usb_driver, sizeof(esp_usb_driver_t *), 0), err, TAG, "send USBNET_EVENT_STOP event failed");
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_usb_update_input_path(
    esp_usb_handle_t hdl,
    esp_err_t (*stack_input)(esp_usb_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv),
    void *priv)
{
    printf("####%s %d\n", __FILE__, __LINE__);  
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    usb_driver->priv = priv;
    usb_driver->stack_input = stack_input;
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_usb_transmit(esp_usb_handle_t hdl, void *buf, size_t length)
{
    printf("####%s %d\n", __FILE__, __LINE__);  
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(buf, ESP_ERR_INVALID_ARG, err, TAG, "can't set buf to null");
    ESP_GOTO_ON_FALSE(length, ESP_ERR_INVALID_ARG, err, TAG, "buf length can't be zero");
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // esp_usb_mac_t *mac = usb_driver->mac;
    // return mac->transmit(mac, buf, length);
err:
    return ret;
}

esp_err_t esp_usb_receive(esp_usb_handle_t hdl, uint8_t *buf, uint32_t *length)
{
    printf("####%s %d\n", __FILE__, __LINE__);  
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(buf && length, ESP_ERR_INVALID_ARG, err, TAG, "can't set buf and length to null");
    ESP_GOTO_ON_FALSE(*length > 60, ESP_ERR_INVALID_ARG, err, TAG, "length can't be less than 60");
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // esp_usb_mac_t *mac = usb_driver->mac;
    // return mac->receive(mac, buf, length);
err:
    return ret;
}

esp_err_t esp_usb_ioctl(esp_usb_handle_t hdl, int cmd, void *data)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    // esp_usb_mac_t *mac = usb_driver->mac;
    // esp_usb_phy_t *phy = usb_driver->phy;
    switch (cmd) {

    case ETH_CMD_G_MAC_ADDR:
    {
        uint8_t *d = data;
        memcpy(d, usb_driver->macaddr, sizeof(usb_driver->macaddr)/sizeof(usb_driver->macaddr[0]));
        break;
    }
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "unknown io command: %d", cmd);
        break;
    }
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_usb_increase_reference(esp_usb_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    atomic_fetch_add(&usb_driver->ref_count, 1);
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_usb_decrease_reference(esp_usb_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_usb_driver_t *usb_driver = (esp_usb_driver_t *)hdl;
    ESP_GOTO_ON_FALSE(usb_driver, ESP_ERR_INVALID_ARG, err, TAG, "usbernet driver handle can't be null");
    atomic_fetch_sub(&usb_driver->ref_count, 1);
    return ESP_OK;
err:
    return ret;
}
