/**************************************************************************/
/*!
    @file     rndis_host_app.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "rndis_host_app.h"
#include "app_os_prio.h"

#if TUSB_CFG_HOST_CDC && TUSB_CFG_HOST_CDC_RNDIS

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
void tusbh_cdc_rndis_mounted_cb(uint8_t dev_addr)
{ // application set-up
  uint8_t mac_address[6];

  printf("\nan RNDIS device is mounted\n");
  tusbh_cdc_rndis_get_mac_addr(dev_addr, mac_address);

  printf("MAC Address ");
  for(uint8_t i=0; i<6; i++)  printf("%X ", mac_address[i]);
  printf("\n");
}

void tusbh_cdc_rndis_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\nan RNDIS device is unmounted\n");
}

void rndis_host_app_init(void)
{

}

OSAL_TASK_FUNCTION( rndis_host_app_task, p_task_para)
{
  OSAL_TASK_LOOP_BEGIN
  OSAL_TASK_LOOP_END
}

#endif
