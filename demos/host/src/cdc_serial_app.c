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

static uint8_t serial_in_buffer[32] TUSB_CFG_ATTR_USBRAM;
static uint8_t serial_out_buffer[32] TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// tinyusb Callbacks
//--------------------------------------------------------------------+
void tusbh_cdc_mounted_cb(uint8_t dev_addr)
{ // application set-up
  printf("\na CDC device is mounted\n");

  osal_queue_flush(queue_hdl);
}

void tusbh_cdc_unmounted_cb(uint8_t dev_addr)
{ // application tear-down
  printf("\na CDC device is unmounted\n");
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
          osal_queue_send(queue_hdl, serial_in_buffer+i);
        }
      break;

      case TUSB_EVENT_XFER_ERROR: break; // ignore

      case TUSB_EVENT_XFER_STALLED:
      default :
        ASSERT(false, VOID_RETURN);
      break;
    }
  }
  else if (pipe_id == CDC_PIPE_DATA_OUT) { }
  else if (pipe_id == CDC_PIPE_NOTIFICATION) { }
}

//--------------------------------------------------------------------+
// APPLICATION
//--------------------------------------------------------------------+
void cdc_serial_app_init(void)
{
  memclr_(serial_in_buffer, sizeof(serial_in_buffer));

  queue_hdl = osal_queue_create( OSAL_QUEUE_REF(queue_def) );
  ASSERT_PTR( queue_hdl, VOID_RETURN);

  ASSERT( TUSB_ERROR_NONE == osal_task_create(OSAL_TASK_REF(cdc_serial_app_task)), VOID_RETURN);
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( cdc_serial_app_task ) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  for(uint8_t dev_addr=0; dev_addr< TUSB_CFG_HOST_DEVICE_MAX; dev_addr++)
  {
    if ( tusbh_cdc_serial_is_mounted(dev_addr) )
    {
      //------------- send characters got from uart terminal -------------//
      int ch_tx = getchar();
      if ( ch_tx > 0 )
      { // USB is much faster than serial, here we assume usb is always complete. There could be some characters missing
        serial_out_buffer[0] = (uint8_t) ch_tx;

        if ( !tusbh_cdc_is_busy(dev_addr, CDC_PIPE_DATA_OUT) )
        {
          tusbh_cdc_send(dev_addr, serial_out_buffer, 1, false); // no need for interrupt on serial out pipe
        }
      }

      //------------- print out received characters -------------//
      tusb_error_t error;
      do{
        uint8_t ch_rx = 0;
        osal_queue_receive(queue_hdl, &ch_rx, OSAL_TIMEOUT_NOTIMEOUT, &error); // instant return
        if (error == TUSB_ERROR_NONE && ch_rx) printf("%c", ch_rx);
      }while (error == TUSB_ERROR_NONE);

      if ( !tusbh_cdc_is_busy(dev_addr, CDC_PIPE_DATA_IN) )
      {
        tusbh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true);
      }

      break; // demo app only communicate with the first CDC-capable device
    }
  }

  OSAL_TASK_LOOP_END
}

#endif
