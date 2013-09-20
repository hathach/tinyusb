/**************************************************************************/
/*!
    @file     cdc_serial_app.c
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

#include "cdc_serial_app.h"

#if TUSB_CFG_OS != TUSB_OS_NONE
#include "app_os_prio.h"
#endif

#if TUSB_CFG_HOST_CDC

#define QUEUE_SERIAL_DEPTH   100

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
OSAL_TASK_DEF(cdc_serial_app_task, 128, CDC_SERIAL_APP_TASK_PRIO);
OSAL_QUEUE_DEF(queue_def, QUEUE_SERIAL_DEPTH, uint8_t);

static osal_queue_handle_t queue_hdl;
static uint8_t buffer_in[64] TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// tinyusb Callbacks
//--------------------------------------------------------------------+
void tusbh_cdc_mounted_cb(uint8_t dev_addr)
{
  // application set-up

  printf("a CDC device is mounted\n");

  osal_queue_flush(queue_hdl);
  tusbh_cdc_receive(dev_addr, buffer_in, sizeof(buffer_in), true); // first report
}

void tusbh_cdc_unmounted_isr(uint8_t dev_addr)
{
  // application tear-down
}

void tusbh_cdc_xfer_isr(uint8_t dev_addr, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{
  if ( pipe_id == CDC_PIPE_DATA_IN )
  {
    switch(event)
    {
      case TUSB_EVENT_XFER_COMPLETE:
        for(uint32_t i=0; i<xferred_bytes; i++)
        {
          osal_queue_send(queue_hdl, buffer_in+i);
        }
        tusbh_cdc_receive(dev_addr, buffer_in, sizeof(buffer_in), true);
        break;

      case TUSB_EVENT_XFER_ERROR:
        tusbh_cdc_receive(dev_addr, buffer_in, sizeof(buffer_in), true); // ignore & continue
        break;

      case TUSB_EVENT_XFER_STALLED:
      default :
        ASSERT(false, VOID_RETURN); // error
        break;
    }
  }else if (pipe_id == CDC_PIPE_DATA_OUT)
  {

  }else if (pipe_id == CDC_PIPE_NOTIFICATION)
  {

  }
}

//--------------------------------------------------------------------+
// APPLICATION
//--------------------------------------------------------------------+
void cdc_serial_app_init(void)
{
  memclr_(buffer_in, sizeof(buffer_in));

  queue_hdl = osal_queue_create( OSAL_QUEUE_REF(queue_def) );
  ASSERT_PTR( queue_hdl, VOID_RETURN);

  ASSERT( TUSB_ERROR_NONE == osal_task_create(OSAL_TASK_REF(cdc_serial_app_task)), VOID_RETURN);
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( cdc_serial_app_task ) (void* p_task_para)
{
  tusb_error_t error;
  uint8_t c = 0;

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(queue_hdl, &c, OSAL_TIMEOUT_WAIT_FOREVER, &error);

  if (c)
  {
    printf("%c", c);
  }

  OSAL_TASK_LOOP_END
}
#else

// dummy implementation to remove #ifdef in main.c
void cdc_serial_app_init(void) { }
OSAL_TASK_FUNCTION( cdc_serial_app_task ) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN
  OSAL_TASK_LOOP_END
}

#endif
