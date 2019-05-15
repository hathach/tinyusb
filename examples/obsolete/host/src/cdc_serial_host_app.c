/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "cdc_serial_host_app.h"
#include "app_os_prio.h"

#if CFG_TUH_CDC

#define QUEUE_SERIAL_DEPTH   100

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
static osal_semaphore_t sem_hdl;

enum { SERIAL_BUFFER_SIZE = 64 };
CFG_TUSB_MEM_SECTION static uint8_t serial_in_buffer[SERIAL_BUFFER_SIZE];
CFG_TUSB_MEM_SECTION static uint8_t serial_out_buffer[SERIAL_BUFFER_SIZE];

static uint8_t received_bytes; // set by transfer complete callback

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tuh_cdc_mounted_cb(uint8_t dev_addr)
{ // application set-up
  printf("\na CDC device  (address %d) is mounted\n", dev_addr);

  tu_memclr(serial_in_buffer, sizeof(serial_in_buffer));
  tu_memclr(serial_out_buffer, sizeof(serial_out_buffer));
  received_bytes = 0;

  osal_semaphore_reset(sem_hdl);
  tuh_cdc_receive(dev_addr, serial_in_buffer, SERIAL_BUFFER_SIZE, true); // schedule first transfer
}

void tuh_cdc_unmounted_cb(uint8_t dev_addr)
{ // application tear-down
  printf("\na CDC device (address %d) is unmounted \n", dev_addr);
}

// invoked ISR context
void tuh_cdc_xfer_isr(uint8_t dev_addr, xfer_result_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{
  (void) dev_addr; // compiler warnings

  switch ( pipe_id )
  {
    case CDC_PIPE_DATA_IN:
      switch(event)
      {
        case XFER_RESULT_SUCCESS:
          received_bytes = xferred_bytes;
          osal_semaphore_post(sem_hdl);  // notify main task
        break;

        case XFER_RESULT_FAILED:
          received_bytes = 0; // ignore
          tuh_cdc_receive(dev_addr, serial_in_buffer, SERIAL_BUFFER_SIZE, true); // waiting for next data
        break;

        case XFER_RESULT_STALLED:
        default :
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
void cdc_serial_host_app_init(void)
{
  sem_hdl = osal_semaphore_create(1, 0);
  TU_ASSERT( sem_hdl, VOID_RETURN);

  TU_VERIFY( osal_task_create(cdc_serial_host_app_task, "cdc", 128, NULL, CDC_SERIAL_APP_TASK_PRIO), );
}

//------------- main task -------------//
void cdc_serial_host_app_task( void* param )
{
  (void) param;

  OSAL_TASK_BEGIN

  //------------- send characters got from uart terminal to the first CDC device -------------//
  for(uint8_t dev_addr=1; dev_addr <= CFG_TUSB_HOST_DEVICE_MAX; dev_addr++)
  {
    if ( tuh_cdc_serial_is_mounted(dev_addr) )
    {
      int ch_tx = getchar();
      if ( ch_tx > 0 )
      { // USB is much faster than serial, here we assume usb is always complete. There could be some characters missing though
        serial_out_buffer[0] = (uint8_t) ch_tx;

        if ( !tuh_cdc_is_busy(dev_addr, CDC_PIPE_DATA_OUT) )
        {
          tuh_cdc_send(dev_addr, serial_out_buffer, 1, false); // no need for callback on serial out pipe
        }
      }
      break; // demo app only communicate with the first CDC-capable device
    }
  }

  //------------- print out received characters -------------//
  tusb_error_t error;
  osal_semaphore_wait(sem_hdl, 100, &error); // waiting for incoming data

  if ( TUSB_ERROR_NONE == error)
  {
    for(uint8_t i=0; i<received_bytes; i++) putchar(serial_in_buffer[i]);

    for(uint8_t dev_addr=1; dev_addr <= CFG_TUSB_HOST_DEVICE_MAX; dev_addr++)
    {
      if ( tuh_cdc_serial_is_mounted(dev_addr) )
      {
        tuh_cdc_receive(dev_addr, serial_in_buffer, SERIAL_BUFFER_SIZE, true);

        break; // demo app only communicate with the first CDC-capable device
      }
    }
  }

  OSAL_TASK_END
}

#endif
