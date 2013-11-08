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
OSAL_SEM_DEF(serial_semaphore);

static osal_semaphore_handle_t sem_hdl;

static uint8_t serial_in_buffer[32] TUSB_CFG_ATTR_USBRAM;
static uint8_t serial_out_buffer[32] TUSB_CFG_ATTR_USBRAM;

static uint8_t received_bytes; // set by transfer complete callback

//--------------------------------------------------------------------+
// tinyusb Callbacks
//--------------------------------------------------------------------+
void tusbh_cdc_mounted_cb(uint8_t dev_addr)
{ // application set-up
  printf("\na CDC device is mounted\n");

  memclr_(serial_in_buffer, sizeof(serial_in_buffer));
  memclr_(serial_out_buffer, sizeof(serial_out_buffer));
  received_bytes = 0;

  osal_semaphore_reset(sem_hdl);
  tusbh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true); // schedule first transfer
}

void tusbh_cdc_unmounted_cb(uint8_t dev_addr)
{ // application tear-down
  printf("\na CDC device is unmounted\n");
}

void tusbh_cdc_xfer_isr(uint8_t dev_addr, tusb_event_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{
  switch ( pipe_id )
  {
    case CDC_PIPE_DATA_IN:
      switch(event)
      {
        case TUSB_EVENT_XFER_COMPLETE:
          received_bytes = xferred_bytes;
          osal_semaphore_post(sem_hdl);  // notify main task
          break;

        case TUSB_EVENT_XFER_ERROR:
          xferred_bytes = 0; // ignore
          break;

        case TUSB_EVENT_XFER_STALLED:
        default :
          ASSERT(false, VOID_RETURN);
          break;
      }
    break;

    case CDC_PIPE_DATA_OUT:
    case CDC_PIPE_NOTIFICATION:
    default:
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION
//--------------------------------------------------------------------+
void cdc_serial_app_init(void)
{
  sem_hdl = osal_semaphore_create( OSAL_SEM_REF(serial_semaphore) );
  ASSERT_PTR( sem_hdl, VOID_RETURN);

  ASSERT( TUSB_ERROR_NONE == osal_task_create(OSAL_TASK_REF(cdc_serial_app_task)), VOID_RETURN);
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( cdc_serial_app_task ) (void* p_task_para)
{
  // This task can be separated into 2 Task : sending & receiving.
  OSAL_TASK_LOOP_BEGIN

  //------------- send characters got from uart terminal to the first CDC device -------------//
  for(uint8_t dev_addr=1; dev_addr <= TUSB_CFG_HOST_DEVICE_MAX; dev_addr++)
  {
    if ( tusbh_cdc_serial_is_mounted(dev_addr) )
    {
      int ch_tx = getchar();
      if ( ch_tx > 0 )
      { // USB is much faster than serial, here we assume usb is always complete. There could be some characters missing
        serial_out_buffer[0] = (uint8_t) ch_tx;

        if ( !tusbh_cdc_is_busy(dev_addr, CDC_PIPE_DATA_OUT) )
        {
          tusbh_cdc_send(dev_addr, serial_out_buffer, 1, false); // no need for interrupt on serial out pipe
        }
      }
      break; // demo app only communicate with the first CDC-capable device
    }
  }

  //------------- print out received characters -------------//
  tusb_error_t error;
  osal_semaphore_wait(sem_hdl, 100, &error); // timeout to allow getchar from uart terminal can be executed

  if ( TUSB_ERROR_NONE == error)
  {
    for(uint8_t i=0; i<received_bytes; i++)
    {
      printf("%c", serial_in_buffer[i]);
    }

    for(uint8_t dev_addr=1; dev_addr <= TUSB_CFG_HOST_DEVICE_MAX; dev_addr++)
    {
      if ( tusbh_cdc_serial_is_mounted(dev_addr) )
      {
        tusbh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true);

        break; // demo app only communicate with the first CDC-capable device
      }
    }
  }

  OSAL_TASK_LOOP_END
}

#endif
