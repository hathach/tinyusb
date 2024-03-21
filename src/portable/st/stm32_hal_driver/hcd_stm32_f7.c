/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Ha Thach (tinyusb.org)
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

#include "host/hcd.h"

#include <stdint.h>

#include "stm32f7xx_hal.h"
#include "tusb_config.h"

#define USBH_PID_SETUP 0                       /*!< Indicate the URB request is setup type */
#define USBH_PID_DATA 1                        /*!< Indicate the URB request is data type */
#define USBH_SETUP_BUFFER_LENGTH 8             /*!< The length of the setup buffer */
#define USBH_PIPE_MAX CFG_TUH_ENDPOINT_MAX * 2 /*!< Maximum number of pipes */

#define HAL_HOST_CHANNELS 15 /*!< Number of channels initiated by the HAL. */

typedef struct TU_ATTR_PACKED {
	uint8_t dev_addr;
	uint8_t ep_addr;
	uint8_t ep_num;
	tusb_dir_t ep_dir;
	uint8_t ep_type;
	uint8_t num;
	bool is_opened;
	uint8_t *buffer;
	uint16_t buflen;
} pipe_info_t;

static HCD_HandleTypeDef hhcd;                 /*!< HCD handle */
static pipe_info_t pipes[USBH_PIPE_MAX] = {0}; /*!< The pipe info of the endpoints that are used */
static bool is_port_connected = false;         /*!< Indicate if the port is connected */
static bool is_waiting_usbh_restart = false;   /*!< Indicate if we are waiting host driver to be started */

/**
 * @brief Open a control IN pipe at the gien device address and endpoint.
 *
 * @param dev_addr           Device address.
 * @param ep_num             Endpoint unmber.
 * @param max_packet_size    Maximum size of packets.
 * @return Returns a ::bool
 */
static bool open_control_in_pipe(uint8_t dev_addr, uint8_t ep_num, uint16_t max_packet_size);

/**
 * @brief Find the pipe index based on the given (unique) device/endpoint address combination.
 *        Return -1 when no pipe can be find.
 *
 * @param dev_addr           Device address.
 * @param ep_addr            Endpoint address.
 * @return Returns a ::int8_t
 */
static int8_t find_pipe_index_based_on_endpoint_address(uint8_t dev_addr, uint8_t ep_addr);

/**
 * @brief Find the pipe index based on the given device address and endpoint info.
 *        This function is used when the endpoint address is unknown.
 *        Return -1 when no pipe can be find.
 *
 * @param dev_addr           Device address.
 * @param ep_num             Endpoint unmber.
 * @param ep_type            Endpoint type (e.g. Control/Interrupt/Bulk).
 * @param ep_is_in           Indicator of whether the endpoint is in(1) or out(0).
 * @return Returns a ::int8_t
 */
static int8_t
find_pipe_index_based_on_endpoint_attributes(uint8_t dev_addr, uint8_t ep_num, uint8_t ep_type, uint8_t ep_is_in);

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
bool hcd_init(uint8_t rhport)
{
	(void) (rhport);
	hhcd.Instance = USB_OTG_HS;
	hhcd.Init.Host_channels = HAL_HOST_CHANNELS;
	hhcd.Init.speed = HCD_SPEED_HIGH;
	hhcd.Init.dma_enable = DISABLE;
	hhcd.Init.phy_itface = USB_OTG_EMBEDDED_PHY;
	hhcd.Init.Sof_enable = DISABLE;
	hhcd.Init.low_power_enable = DISABLE;
	hhcd.Init.vbus_sensing_enable = DISABLE;
	hhcd.Init.use_external_vbus = DISABLE;

	return true;
}

void hcd_int_handler(uint8_t rhport, bool in_isr)
{
	(void) rhport;
	(void) in_isr;
	HAL_HCD_IRQHandler(&hhcd);
}

void hcd_int_enable(uint8_t rhport)
{
	/* The interrupt is initialzed on the HAL level with HAL_HCD_Init, which calls HAL_HCD_MspInit */
	(void) rhport;
}

void hcd_int_disable(uint8_t rhport)
{
	/* Not supported */
	(void) rhport;
}

uint32_t hcd_frame_number(uint8_t rhport)
{
	(void) rhport;
	return HAL_HCD_GetCurrentFrame(&hhcd);
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+ */
bool hcd_port_connect_status(uint8_t rhport)
{
	(void) rhport;
	/* The port may be briefly disconnected -> connected. In such cases, the USB interrupt
	   handler is likely to malfunction already. Hence, we wait for the user to restart the host
	   driver before concluding the port is successfully connected */
	return (is_port_connected && !is_waiting_usbh_restart);
}

void hcd_port_reset(uint8_t rhport)
{
	(void) rhport;
	HAL_HCD_ResetPort(&hhcd);
}

void hcd_port_reset_end(uint8_t rhport)
{
	(void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
	(void) (rhport);
	switch (HAL_HCD_GetCurrentSpeed(&hhcd)) {
	case HCD_SPEED_HIGH:
		return TUSB_SPEED_HIGH;
	case HCD_SPEED_FULL:
		return TUSB_SPEED_FULL;
	default:
		return TUSB_SPEED_INVALID;
	}
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
	(void) rhport;
	(void) dev_addr;
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc)
{
	(void) rhport;
	uint8_t i = 0;

	/* Make sure the pipes are cleared when starting a new enumeration */
	if (dev_addr == 0 && ep_desc->bmAttributes.xfer == EP_TYPE_CTRL) {
		memset(pipes, 0, sizeof(pipe_info_t) * USBH_PIPE_MAX);
	}

	/* Find a free pipe */
	for (; i < USBH_PIPE_MAX; i++) {
		if (pipes[i].is_opened == false) {
			break;
		}
	}

	/* Return false when no pipe is available (should not happen) */
	if (i >= USBH_PIPE_MAX) {
		return false;
	}

	pipes[i].dev_addr = dev_addr;
	pipes[i].ep_addr = ep_desc->bEndpointAddress;
	pipes[i].ep_num = tu_edpt_number(pipes[i].ep_addr);
	pipes[i].ep_dir = tu_edpt_dir(pipes[i].ep_addr);
	pipes[i].ep_type = ep_desc->bmAttributes.xfer;

	if (pipes[i].ep_type == EP_TYPE_CTRL && pipes[i].ep_dir == TUSB_DIR_OUT) {
		/* At a device address, the control endpoint is always number 0, meaning:
		   control OUT is always pipe number 0, and control IN is always pipe number 1*/
		pipes[i].num = 0;
	} else {
		/* Calculate the pipe number at one device:
		   taking control OUT pipe numbder as the base */
		pipes[i].num = i - find_pipe_index_based_on_endpoint_address(pipes[i].dev_addr, 0);
	}

	if (HAL_HCD_HC_Init(&hhcd, pipes[i].num, pipes[i].ep_addr, pipes[i].dev_addr, USB_OTG_SPEED_HIGH, pipes[i].ep_type,
	                    ep_desc->wMaxPacketSize) != HAL_OK) {
		return false;
	}
	pipes[i].is_opened = true;

	/* Open a IN pipe for the control endpoint manually since tinyUSB does not do so */
	if (pipes[i].ep_type == EP_TYPE_CTRL && pipes[i].ep_dir == TUSB_DIR_OUT) {
		return open_control_in_pipe(pipes[i].dev_addr, pipes[i].ep_num, ep_desc->wMaxPacketSize);
	}

	return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
	(void) rhport;

	int8_t i = find_pipe_index_based_on_endpoint_address(dev_addr, 0);
	if (i < 0) {
		/* Should never happen, if happens it means the pipes are not opened correctly */
		return false;
	}

	/* Control OUT pipe is always number 0 on each device */
	if (HAL_HCD_HC_SubmitRequest(&hhcd, pipes[i].num, TUSB_DIR_OUT, TUSB_XFER_CONTROL, USBH_PID_SETUP,
	                             (uint8_t *) setup_packet, USBH_SETUP_BUFFER_LENGTH, 0) != HAL_OK) {
		return false;
	}

	return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen)
{
	(void) (rhport);

	int8_t i = find_pipe_index_based_on_endpoint_address(dev_addr, ep_addr);
	if (i < 0) {
		/* Should never happen, if happens it means we opened interfaces that are not selected */
		return false;
	}

	if (pipes[i].ep_dir == TUSB_DIR_IN) {
		/* Make sure the receiving buffer is cleared before sending a receiving request */
		memset(buffer, 0, buflen);
	} else {
		/* Store the OUT buffer and length in case we need to re-send it in HAL_HCD_HC_NotifyURBChange_Callback */
		pipes[i].buffer = buffer;
		pipes[i].buflen = buflen;
	}

	if (HAL_HCD_HC_SubmitRequest(&hhcd, pipes[i].num, pipes[i].ep_dir, pipes[i].ep_type, USBH_PID_DATA, buffer, buflen,
	                             0) != HAL_OK) {
		return false;
	}

	return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
	(void) rhport;
	(void) dev_addr;
	(void) ep_addr;
	return true;
}

/*--------------------------------------------------------------------+
 * Weak HAL functions
 *--------------------------------------------------------------------+*/
void HAL_HCD_MspInit(HCD_HandleTypeDef *handle)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	if (handle->Instance == USB_OTG_HS) {
		/**USB_OTG_HS GPIO Configuration
         * !! USE YOUR OWN PINS HERE !!
		PB14     ------> USB_OTG_HS_DM
		PB15     ------> USB_OTG_HS_DP
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_USB_OTG_HS_CLK_ENABLE();

		/* Peripheral interrupt init */
		HAL_NVIC_SetPriority(OTG_HS_IRQn, 4, 0);
		HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
	}
}

void HAL_HCD_MspDeInit(HCD_HandleTypeDef *handle)
{
	if (handle->Instance == USB_OTG_HS) {
		/* Peripheral interrupt Deinit*/
		HAL_NVIC_DisableIRQ(OTG_HS_IRQn);

		/* Peripheral clock disable */
		__HAL_RCC_USB_OTG_HS_CLK_DISABLE();

		/**USB_OTG_HS GPIO Configuration
		PB14     ------> USB_OTG_HS_DM
		PB15     ------> USB_OTG_HS_DP
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_14 | GPIO_PIN_15);
	}
}

void HAL_HCD_Connect_Callback(HCD_HandleTypeDef *handle)
{
	(void) handle;
	hcd_event_device_attach(BOARD_TUH_RHPORT, true);
}

void HAL_HCD_Disconnect_Callback(HCD_HandleTypeDef *handle)
{
	(void) handle;
	hcd_event_device_remove(BOARD_TUH_RHPORT, true);
}

void HAL_HCD_HC_NotifyURBChange_Callback(HCD_HandleTypeDef *handle, uint8_t chnum, HCD_URBStateTypeDef urb_state)
{
	(void) handle;

	/* chnum == pipe number */
	int8_t i = find_pipe_index_based_on_endpoint_attributes(hhcd.hc[chnum].dev_addr, hhcd.hc[chnum].ep_num,
	                                                        hhcd.hc[chnum].ep_type, hhcd.hc[chnum].ep_is_in);
	if (i < 0) {
		return;
	}

	switch (urb_state) {
	case URB_IDLE:
		return;
	case URB_DONE:
		/* Only send transfer complete event when the URB state is done */
		break;
	case URB_NOTREADY:
		/* Re-send the request when we have received a NAK via the out pipe,
		   otherwise we might get stuck at waiting for nothing. */
		if (pipes[i].ep_dir == TUSB_DIR_OUT) {
			HAL_HCD_HC_SubmitRequest(&hhcd, pipes[i].num, pipes[i].ep_dir, pipes[i].ep_type, USBH_PID_DATA,
			                         pipes[i].buffer, pipes[i].buflen, 0);
		}
		return;
	default:
		/* Deinit the USB interrupt to avoid USB interrupt blocking
		   the system. It is then the user's responsibility to reinit the USB */
		HAL_HCD_MspDeInit(&hhcd);
		return;
	}

	/* Invoke transfer complete event */
	if (pipes[i].ep_dir == TUSB_DIR_IN) {
		hcd_event_xfer_complete(pipes[i].dev_addr, pipes[i].ep_addr, hhcd.hc[chnum].xfer_count, XFER_RESULT_SUCCESS,
		                        true);
	} else {
		hcd_event_xfer_complete(pipes[i].dev_addr, pipes[i].ep_addr, hhcd.hc[chnum].xfer_len, XFER_RESULT_SUCCESS,
		                        true);
	}
}

void HAL_HCD_PortEnabled_Callback(HCD_HandleTypeDef *handle)
{
	(void) handle;
	is_port_connected = true;
}

void HAL_HCD_PortDisabled_Callback(HCD_HandleTypeDef *handle)
{
	(void) handle;
	is_port_connected = false;
	/* We do not depend on the USB device re-attaching itself. Hence, whenever a deattchment happens,
	   we mark the flag to wait for the host driver restarting. */
	is_waiting_usbh_restart = true;
}

void HAL_HCD_SOF_Callback(HCD_HandleTypeDef *handle)
{
	(void) handle;
}

/*--------------------------------------------------------------------+
 * Extra specific functions
 *--------------------------------------------------------------------+*/
bool hcd_start(uint8_t rhport)
{
	(void) rhport;

	if (HAL_HCD_Init(&hhcd) != HAL_OK) {
		return false;
	}

	if (HAL_HCD_Start(&hhcd) != HAL_OK) {
		return false;
	}

	/* Clear the flag when the host driver is started */
	is_waiting_usbh_restart = false;

	return true;
}

bool hcd_stop(uint8_t rhport)
{
	(void) rhport;

	/* Halt all the opened pipe */
	for (uint8_t i = 0; i < USBH_PIPE_MAX; i++) {
		if (pipes[i].is_opened == true) {
			HAL_HCD_HC_Halt(&hhcd, pipes[i].num);
			pipes[i].is_opened = false;
		}
	}

	if (HAL_HCD_Stop(&hhcd) != HAL_OK) {
		return false;
	}

	if (HAL_HCD_DeInit(&hhcd) != HAL_OK) {
		return false;
	}

	return true;
}


/*--------------------------------------------------------------------+
 * Static functions
 *--------------------------------------------------------------------+*/
static bool open_control_in_pipe(uint8_t dev_addr, uint8_t ep_num, uint16_t max_packet_size)
{
	uint8_t i = 0;

	/* Find a free pipe */
	for (; i < USBH_PIPE_MAX; i++) {
		if (pipes[i].is_opened == false) {
			break;
		}
	}

	/* Return false when no pipe is available (should not happen) */
	if (i >= USBH_PIPE_MAX) {
		return false;
	}

	pipes[i].dev_addr = dev_addr;
	pipes[i].ep_addr = tu_edpt_addr(ep_num, TUSB_DIR_IN);
	pipes[i].ep_num = ep_num;
	pipes[i].ep_dir = TUSB_DIR_IN;
	pipes[i].ep_type = EP_TYPE_CTRL;
	/* Control OUT is always pipe number 0, and control IN is always pipe number 1*/
	pipes[i].num = 1;

	if (HAL_HCD_HC_Init(&hhcd, pipes[i].num, pipes[i].ep_addr, pipes[i].dev_addr, USB_OTG_SPEED_HIGH, pipes[i].ep_type,
	                    max_packet_size) != HAL_OK) {
		return false;
	}

	pipes[i].is_opened = true;
	return true;
}

static int8_t find_pipe_index_based_on_endpoint_address(uint8_t dev_addr, uint8_t ep_addr)
{
	for (int8_t i = 0; i < USBH_PIPE_MAX; i++) {
		/* Find the pipe based on dev_addr/ep_addr combination */
		if (pipes[i].dev_addr == dev_addr && pipes[i].ep_addr == ep_addr) {
			return i;
		}
	}
	return -1;
}

static int8_t
find_pipe_index_based_on_endpoint_attributes(uint8_t dev_addr, uint8_t ep_num, uint8_t ep_type, uint8_t ep_is_in)
{
	for (int8_t i = 0; i < USBH_PIPE_MAX; i++) {
		/* Find the pipe based on dev_addr/ep_num/ep_type/ep_dir combination */
		if (pipes[i].dev_addr == dev_addr && pipes[i].ep_num == ep_num && pipes[i].ep_type == ep_type &&
		    pipes[i].ep_dir == ep_is_in) {
			return i;
		}
	}
	return -1;
}