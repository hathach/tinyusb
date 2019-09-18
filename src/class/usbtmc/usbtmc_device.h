/*
 * usbtmc_device.h
 *
 *  Created on: Sep 10, 2019
 *      Author: nconrad
 */
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 N Conrad
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
 * This file is part of the TinyUSB stack.
 */


#ifndef CLASS_USBTMC_USBTMC_DEVICE_H_
#define CLASS_USBTMC_USBTMC_DEVICE_H_

#include "usbtmc.h"

// Enable 488 mode by default
#if !defined(USBTMC_CFG_ENABLE_488)
#define USBTMC_CFG_ENABLE_488 (1)
#endif

// USB spec says that full-speed must be 8,16,32, or 64.
// However, this driver implementation requires it to be >=32
#define USBTMCD_MAX_PACKET_SIZE (64u)

/***********************************************
 *  Functions to be implemeted by the class implementation
 */

#if (USBTMC_CFG_ENABLE_488)
extern usbtmc_response_capabilities_488_t const usbtmcd_app_capabilities;
#else
extern usbtmc_response_capabilities_t const usbtmcd_app_capabilities;
#endif

bool usbtmcd_app_msgBulkOut_start(uint8_t rhport, usbtmc_msg_request_dev_dep_out const * msgHeader);
// transfer_complete does not imply that a message is complete.
bool usbtmcd_app_msg_data(uint8_t rhport, void *data, size_t len, bool transfer_complete);
void usmtmcd_app_bulkOut_clearFeature(uint8_t rhport); // Notice to clear and abort the pending BULK out transfer

bool usbtmcd_app_msgBulkIn_request(uint8_t rhport, usbtmc_msg_request_dev_dep_in const * request);
bool usbtmcd_app_msgBulkIn_complete(uint8_t rhport);
void usmtmcd_app_bulkIn_clearFeature(uint8_t rhport); // Notice to clear and abort the pending BULK out transfer

bool usbtmcd_app_initiate_abort_bulk_in(uint8_t rhport, uint8_t *tmcResult);
bool usbtmcd_app_initiate_abort_bulk_out(uint8_t rhport, uint8_t *tmcResult);
bool usbtmcd_app_initiate_clear(uint8_t rhport, uint8_t *tmcResult);

bool usbtmcd_app_check_abort_bulk_in(uint8_t rhport, usbtmc_check_abort_bulk_rsp_t *rsp);
bool usbtmcd_app_check_abort_bulk_out(uint8_t rhport, usbtmc_check_abort_bulk_rsp_t *rsp);
bool usbtmcd_app_check_clear(uint8_t rhport, usbtmc_get_clear_status_rsp_t *rsp);

// Indicator pulse should be 0.5 to 1.0 seconds long
TU_ATTR_WEAK bool usbtmcd_app_indicator_pluse(uint8_t rhport, tusb_control_request_t const * msg, uint8_t *tmcResult);

#if (USBTMC_CFG_ENABLE_488)
uint8_t usbtmcd_app_get_stb(uint8_t rhport, uint8_t *tmcResult);
TU_ATTR_WEAK bool usbtmcd_app_msg_trigger(uint8_t rhport, usbtmc_msg_generic_t* msg);
//TU_ATTR_WEAK bool usbtmcd_app_go_to_local(uint8_t rhport);
#endif

/*******************************************
 * Called from app
 *
 * We keep a reference to the buffer, so it MUST not change until the app is
 * notified that the transfer is complete.
 ******************************************/

bool usbtmcd_transmit_dev_msg_data(
    uint8_t rhport,
    const void * data, size_t len,
    bool usingTermChar);


/* "callbacks" from USB device core */

bool usbtmcd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length);
void usbtmcd_reset(uint8_t rhport);
bool usbtmcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
bool usbtmcd_control_request(uint8_t rhport, tusb_control_request_t const * request);
bool usbtmcd_control_complete(uint8_t rhport, tusb_control_request_t const * request);
void usbtmcd_init(void);

/************************************************************
 * USBTMC Descriptor Templates
 *************************************************************/

#define USBTMC_APP_CLASS    TUSB_CLASS_APPLICATION_SPECIFIC
#define USBTMC_APP_SUBCLASS 0x03u

#define USBTMC_PROTOCOL_STD    0x00u
#define USBTMC_PROTOCOL_USB488 0x01u

//   Interface number, number of endpoints, EP string index, USB_TMC_PROTOCOL*, bulk-out endpoint ID,
//   bulk-in endpoint ID
#define USBTMC_IF_DESCRIPTOR(_itfnum, _bNumEndpoints, _stridx, _itfProtocol) \
/* Interface */ \
  0x09, TUSB_DESC_INTERFACE, _itfnum, 0x00, _bNumEndpoints, USBTMC_APP_CLASS, USBTMC_APP_SUBCLASS, _itfProtocol, _stridx

#define USBTMC_IF_DESCRIPTOR_LEN 9u

// bulk-out Size must be a multiple of 4 bytes
#define USBTMC_BULK_DESCRIPTORS(_epout, _epin) \
/* Endpoint Out */ \
7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(USBTMCD_MAX_PACKET_SIZE), 0u, \
/* Endpoint In */ \
7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(USBTMCD_MAX_PACKET_SIZE), 0u

#define USBTMC_BULK_DESCRIPTORS_LEN (7u+7u)

/* optional interrupt endpoint */ \
// _int_pollingInterval : for LS/FS, expressed in frames (1ms each). 16 may be a good number?
#define USBTMC_INT_DESCRIPTOR(_ep_interrupt, _ep_interrupt_size, _int_pollingInterval ) \
7, TUSB_DESC_ENDPOINT, _ep_interrupt, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_interrupt_size), 0x16

#define USBTMC_INT_DESCRIPTOR_LEN (7u)


#endif /* CLASS_USBTMC_USBTMC_DEVICE_H_ */
