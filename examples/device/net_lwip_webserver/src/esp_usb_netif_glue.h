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
#pragma once

#include "esp_usb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a netif glue for usb cdc-ecm driver
 * @note netif glue is used to attach io driver to TCP/IP netif
 *
 * @param usb_hdl usb cdc-ecm driver handle
 * @return glue object, which inherits esp_netif_driver_base_t
 */
void *esp_usb_new_netif_glue(esp_usb_handle_t usb_hdl);

/**
 * @brief Delete netif glue of usb cdc-ecm driver
 *
 * @param glue netif glue
 * @return -ESP_OK: delete netif glue successfully
 */
esp_err_t esp_usb_del_netif_glue(void *glue);

/**
 * @brief Register default IP layer handlers for usb cdc-ecm
 *
 * @note: usb cdc-ecm handle might not yet properly initialized when setting up these default handlers
 *
 * @param[in] esp_netif esp network interface handle created for usb cdc-ecm driver
 * @return
 *      - ESP_ERR_INVALID_ARG: invalid parameter (esp_netif is NULL)
 *      - ESP_OK: set default IP layer handlers successfully
 *      - others: other failure occurred during register esp_event handler
 */

esp_err_t esp_usb_set_default_handlers(void *esp_netif);

/**
 * @brief Unregister default IP layer handlers for usb cdc-ecm
 *
 * @param[in] esp_netif esp network interface handle created for usb cdc-ecm driver
 * @return
 *      - ESP_ERR_INVALID_ARG: invalid parameter (esp_netif is NULL)
 *      - ESP_OK: clear default IP layer handlers successfully
 *      - others: other failure occurred during unregister esp_event handler
 */
esp_err_t esp_usb_clear_default_handlers(void *esp_netif);

#ifdef __cplusplus
}
#endif
