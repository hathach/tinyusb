/*
 * cdc.h
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tinyUSB stack.
 */

/** \file
 *  \brief CDC Class Driver
 *
 *  \note TBD
 */

/** \ingroup Group_TinyUSB
 *  \addtogroup Group_ClassDriver Class Driver
 *  @{
 *  \defgroup Group_CDC Communication Device Class
 *  @{
 */

#ifndef _TUSB_CDC_H__
#define _TUSB_CDC_H__

// todo refractor later
#include "common/common.h"
#include "device/dcd.h"

#define CDC_BUFFER_SIZE (2*CDC_DATA_EP_MAXPACKET_SIZE)

/** \brief send a character to host
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
bool tusb_cdc_putc(uint8_t c);

/** \brief get a character from host
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
bool tusb_cdc_getc(uint8_t *c);

/** \brief send a number of characters to host
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
uint16_t tusb_cdc_send(uint8_t* buffer, uint16_t count);

/** \brief get a number of characters from host
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
uint16_t tusb_cdc_recv(uint8_t* buffer, uint16_t max);

/** \brief initialize cdc driver
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
TUSB_Error_t tusb_cdc_init(USBD_HANDLE_T hUsb, USB_INTERFACE_DESCRIPTOR const *const pControlIntfDesc, USB_INTERFACE_DESCRIPTOR const *const pDataIntfDesc, uint32_t* mem_base, uint32_t* mem_size);

/** \brief notify cdc driver that usb is configured
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
TUSB_Error_t tusb_cdc_configured(USBD_HANDLE_T hUsb);
#endif
