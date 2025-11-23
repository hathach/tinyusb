/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for xHCI implementation
 */

#ifndef __HCD_SOCFPGA_H__
#define __HCD_SOCFPGA_H__

#include <stdio.h>
#include "xhci.h"
#include "xhci_events.h"
#include <common/tusb_types.h>

#define XHCI_SPEED_SS    (4)
#define XHCI_QUEUE_SZ    (10U)

typedef struct
{
    /* reference to xHCI structure */
    struct xhci_data xhci_priv __attribute__((aligned(64)));
}Usb3_Handle_t;

/*
 * @brief  allocate all the usb3 port memory
 * @return
 *  xHanlde  reference to Usb3_Handle_t structure
 */
Usb3_Handle_t*alloc_usb_port(void);

/*
 * @brief  deallocate all the usb3 port memory
 * @param[in] handle usb3 port reference handle
 */
void dealloc_usb_port(Usb3_Handle_t*handle);

/*
 * @brief api to check whether the reset process has completed or not
 * @param[in] rhport portid of the correspondinf port
 * @return
 *  - true, if operation is successul,
 *  - false, if the operation fails
 */
bool usb_port_reset_end(uint8_t rhport);

/*
 * @brief  common usb api to reset the port
 * @param[in] rhport RH port number
 */
void reset_usb_port(uint8_t rhport);

/*
 * @brief  initialize the HCD queue
 * @return
 *  0, if initalization is successful,
 *  errno, incase of any failure
 */
int init_hcd_params(void);

/*
 * @brief wait for the command completio event
 * @param[in] xhci_ptr reference to xhci context data structure
 * @param[in[ type to specify the type of command completed by xHCI
 * @return
 *  - NO_ERROR, if command is executed succssfully
 *  - -1, if command completion event reports failure
 */
int wait_for_command_completion_event(struct xhci_data *xhci_ptr, int type);

/*
 * @brief  notify about the completion of command ring operation
 * @param[in] command completion event params
 */
void xhci_command_event_complete(xcc_event_t event);

bool hcd_dwc3_init(uint8_t rhport, const tusb_rhport_init_t* rh_init);
void hcd_dwc3_int_enable (uint8_t rhport);
void hcd_dwc3_int_disable(uint8_t rhport);

uint32_t hcd_dwc3_frame_number(uint8_t rhport);
bool hcd_dwc3_port_connect_status(uint8_t rhport);
void hcd_dwc3_port_reset(uint8_t rhport);

void hcd_dwc3_port_reset_end(uint8_t rhport);
tusb_speed_t hcd_dwc3_port_speed_get(uint8_t rhport);
void hcd_dwc3_device_close(uint8_t rhport, uint8_t dev_addr);

bool hcd_dwc3_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t* desc_ep);
bool hcd_dwc3_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen);
bool hcd_dwc3_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr);
bool hcd_dwc3_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]);
bool hcd_dwc3_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr);
bool hcd_dwc3_parse_full_conf_descriptor( tusb_desc_configuration_t *desc_cfg );

void hcd_dwc3_int_handler(uint8_t rhport, bool in_isr);

#endif  /* __HCD_SOCFPGA_H__ */
