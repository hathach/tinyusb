/**************************************************************************/
/*!
    @file     hid_device.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_HID_DEVICE_H_
#define _TUSB_HID_DEVICE_H_

#include "common/common.h"
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

#ifdef DEVICE_ROMDRIVER
/** \brief Initialize HID driver
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
tusb_error_t tusb_hid_init(USBD_HANDLE_T hUsb, USB_INTERFACE_DESCRIPTOR const *const pIntfDesc, uint8_t const * const pHIDReportDesc, uint32_t ReportDescLength, uint32_t* mem_base, uint32_t* mem_size);

/** \brief Notify HID class that usb is configured
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
tusb_error_t tusb_hid_configured(USBD_HANDLE_T hUsb);

/** \brief Used by Application to send Keycode to Host
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
tusb_error_t tusb_hid_keyboard_sendKeys(uint8_t modifier, uint8_t keycodes[], uint8_t numkey);

/** \brief
 *
 * \param[in]  para1
 * \param[out] para2
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note
 */
tusb_error_t tusb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y);

#endif /* ROM DRIVRER */

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_DEVICE_H_ */

/** @} */
