/**************************************************************************/
/*!
    @file     cdc_device_app.c
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

#include "cdc_device_app.h"

#if CFG_TUSB_DEVICE_CDC

#include "common/tusb_fifo.h" // TODO refractor
#include "app_os_prio.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
enum { SERIAL_BUFFER_SIZE = 64 };

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void cdc_serial_app_mount(uint8_t rhport)
{
}

void cdc_serial_app_umount(uint8_t rhport)
{

}

void tud_cdc_rx_cb(uint8_t rhport)
{

}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void cdc_serial_app_init(void)
{
}

tusb_error_t cdc_serial_subtask(void);

void cdc_serial_app_task(void* param)
{
  (void) param;

  OSAL_TASK_BEGIN
  cdc_serial_subtask();
  OSAL_TASK_END
}

tusb_error_t cdc_serial_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  if ( tud_mounted() && tud_cdc_available() )
  {
    uint8_t buf[64];

    // read and echo back
    uint32_t count = tud_cdc_read(buf, sizeof(buf));

    tud_cdc_write(buf, count);
  }

  OSAL_SUBTASK_END
}

#endif
