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

#include "esp_err.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum Ethernet payload size
 *
 */
#define USB_MAX_PAYLOAD_LEN (1500)

/**
 * @brief Minimum Ethernet payload size
 *
 */
#define USB_MIN_PAYLOAD_LEN (46)

/**
 * @brief Ethernet frame header size: Dest addr(6 Bytes) + Src addr(6 Bytes) + length/type(2 Bytes)
 *
 */
#define USB_HEADER_LEN (14)

/**
 * @brief Ethernet frame CRC length
 *
 */
#define USB_CRC_LEN (4)

/**
 * @brief Optional 802.1q VLAN Tag length
 *
 */
#define USB_VLAN_TAG_LEN (4)

/**
 * @brief Jumbo frame payload size
 *
 */
#define USB_JUMBO_FRAME_PAYLOAD_LEN (9000)

/**
 * @brief Maximum frame size (1522 Bytes)
 *
 */
#define USB_MAX_PACKET_SIZE (USB_HEADER_LEN + USB_VLAN_TAG_LEN + USB_MAX_PAYLOAD_LEN + USB_CRC_LEN)

/**
 * @brief Minimum frame size (64 Bytes)
 *
 */
#define USB_MIN_PACKET_SIZE (USB_HEADER_LEN + USB_MIN_PAYLOAD_LEN + USB_CRC_LEN)

/**
* @brief Ethernet driver state
*
*/
typedef enum {
    USB_STATE_LLINIT, /*!< Lowlevel init done */
    USB_STATE_DEINIT, /*!< Deinit done */
    USB_STATE_LINK,   /*!< Link status changed */
    USB_STATE_SPEED,  /*!< Speed updated */
    USB_STATE_DUPLEX, /*!< Duplex updated */
    USB_STATE_PAUSE,  /*!< Pause ability updated */
} esp_usb_state_t;

/**
* @brief Command list for ioctl API
*
*/
typedef enum {
    USB_CMD_G_MAC_ADDR,    /*!< Get MAC address */
    USB_CMD_S_MAC_ADDR,    /*!< Set MAC address */
    USB_CMD_G_PHY_ADDR,    /*!< Get PHY address */
    USB_CMD_S_PHY_ADDR,    /*!< Set PHY address */
    USB_CMD_G_SPEED,       /*!< Get Speed */
    USB_CMD_S_PROMISCUOUS, /*!< Set promiscuous mode */
    USB_CMD_S_FLOW_CTRL,   /*!< Set flow control */
    USB_CMD_G_DUPLEX_MODE, /*!< Get Duplex mode */
} esp_usb_io_cmd_t;

/**
* @brief Ethernet link status
*
*/
typedef enum {
    USB_LINK_UP,  /*!< Ethernet link is up */
    USB_LINK_DOWN /*!< Ethernet link is down */
} usb_link_t;

/**
* @brief Ethernet speed
*
*/
typedef enum {
    USB_SPEED_10M, /*!< Ethernet speed is 10Mbps */
    USB_SPEED_100M /*!< Ethernet speed is 100Mbps */
} usb_speed_t;

/**
* @brief Ethernet duplex mode
*
*/
typedef enum {
    USB_DUPLEX_HALF, /*!< Ethernet is in half duplex */
    USB_DUPLEX_FULL  /*!< Ethernet is in full duplex */
} usb_duplex_t;

/**
* @brief Ethernet mediator
*
*/
typedef struct esp_usb_mediator_s esp_usb_mediator_t;

/**
* @brief Ethernet mediator
*
*/
struct esp_usb_mediator_s {
    /**
    * @brief Read PHY register
    *
    * @param[in] usb: mediator of Ethernet driver
    * @param[in] phy_addr: PHY Chip address (0~31)
    * @param[in] phy_reg: PHY register index code
    * @param[out] reg_value: PHY register value
    *
    * @return
    *       - ESP_OK: read PHY register successfully
    *       - ESP_FAIL: read PHY register failed because some error occurred
    *
    */
    esp_err_t (*phy_reg_read)(esp_usb_mediator_t *usb, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value);

    /**
    * @brief Write PHY register
    *
    * @param[in] usb: mediator of Ethernet driver
    * @param[in] phy_addr: PHY Chip address (0~31)
    * @param[in] phy_reg: PHY register index code
    * @param[in] reg_value: PHY register value
    *
    * @return
    *       - ESP_OK: write PHY register successfully
    *       - ESP_FAIL: write PHY register failed because some error occurred
    */
    esp_err_t (*phy_reg_write)(esp_usb_mediator_t *usb, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value);

    /**
    * @brief Deliver packet to upper stack
    *
    * @param[in] usb: mediator of Ethernet driver
    * @param[in] buffer: packet buffer
    * @param[in] length: length of the packet
    *
    * @return
    *       - ESP_OK: deliver packet to upper stack successfully
    *       - ESP_FAIL: deliver packet failed because some error occurred
    *
    */
    esp_err_t (*stack_input)(esp_usb_mediator_t *usb, uint8_t *buffer, uint32_t length);

    /**
    * @brief Callback on Ethernet state changed
    *
    * @param[in] usb: mediator of Ethernet driver
    * @param[in] state: new state
    * @param[in] args: optional argument for the new state
    *
    * @return
    *       - ESP_OK: process the new state successfully
    *       - ESP_FAIL: process the new state failed because some error occurred
    *
    */
    esp_err_t (*on_state_changed)(esp_usb_mediator_t *usb, esp_usb_state_t state, void *args);
};

/**
* @brief Ethernet event declarations
*
*/
typedef enum {
    USBNET_EVENT_START,        /*!< Ethernet driver start */
    USBNET_EVENT_STOP,         /*!< Ethernet driver stop */
    USBNET_EVENT_CONNECTED,    /*!< Ethernet got a valid link */
    USBNET_EVENT_DISCONNECTED, /*!< Ethernet lost a valid link */
} usb_event_t;

/**
* @brief Ethernet event base declaration
*
*/
ESP_EVENT_DECLARE_BASE(USB_EVENT);

/**
* @brief Detect PHY address
*
* @param[in] usb: mediator of Ethernet driver
* @param[out] detected_addr: a valid address after detection
* @return
*       - ESP_OK: detect phy address successfully
*       - ESP_ERR_INVALID_ARG: invalid parameter
*       - ESP_ERR_NOT_FOUND: can't detect any PHY device
*       - ESP_FAIL: detect phy address failed because some error occurred
*/
esp_err_t esp_usb_detect_phy_addr(esp_usb_mediator_t *usb, int *detected_addr);

#ifdef __cplusplus
}
#endif
