/**************************************************************************/
/*!
    @file     custom_device.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_CUSTOM_CLASS)

#define _TINY_USB_SOURCE_FILE_

#include "common/tusb_common.h"
#include "custom_device.h"
#include "device/usbd_pvt.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
/* VARIABLE DECLARATION
 *------------------------------------------------------------------*/
typedef struct {
  uint8_t itf_num;

  uint8_t ep_in;
  uint8_t ep_out;

} cusd_interface_t;

static cusd_interface_t _cusd_itf;

/*------------------------------------------------------------------*/
/* FUNCTION DECLARATION
 *------------------------------------------------------------------*/
void cusd_init(void)
{
  tu_varclr(&_cusd_itf);
}

tusb_error_t cusd_open(uint8_t rhport, tusb_desc_interface_t const * p_desc_itf, uint16_t *p_len)
{
  cusd_interface_t* p_itf = &_cusd_itf;

  // Open endpoint pair with usbd helper
  tusb_desc_endpoint_t const *p_desc_ep = (tusb_desc_endpoint_t const *) descriptor_next( (uint8_t const*) p_desc_itf );
  TU_ASSERT_ERR( usbd_open_edpt_pair(rhport, p_desc_ep, TUSB_XFER_BULK, &p_itf->ep_out, &p_itf->ep_in) );

  p_itf->itf_num = p_desc_itf->bInterfaceNumber;

  (*p_len) = sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  // TODO Prepare for incoming data
//  TU_ASSERT( dcd_edpt_xfer(rhport, p_itf->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)), TUSB_ERROR_DCD_EDPT_XFER );

  return TUSB_ERROR_NONE;
}

tusb_error_t cusd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
}

tusb_error_t cusd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  return TUSB_ERROR_NONE;
}

void cusd_reset(uint8_t rhport)
{

}

#endif
