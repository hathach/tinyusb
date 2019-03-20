/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

//#include "stdlib.h"
//#include "unity.h"
//#include "type_helper.h"
//#include "tusb_option.h"
//#include "tusb_errors.h"
//
//#include "mock_osal.h"
//#include "hcd.h"
//#include "usbh.h"
//#include "tusb.h"
//#include "hid_host.h"
////#include "ehci_controller_fake.h"
//
//#include "descriptor_test.h"
//
//uint8_t dev_addr;
//uint8_t hostid;
//
void setUp(void)
{
//  dev_addr = RANDOM(CFG_TUSB_HOST_DEVICE_MAX)+1;
//  hostid = RANDOM(CONTROLLER_HOST_NUMBER) + TEST_CONTROLLER_HOST_START_INDEX;
//
////  ehci_controller_init();
//  tusb_init();
//
}
//
void tearDown(void)
{
}
//
//osal_semaphore_handle_t sem_create_stub(osal_semaphore_t * const sem, int num_call)
//{
//  (*p_sem) = 0;
//  return (osal_semaphore_handle_t) p_sem;
//}
//void sem_wait_stub(osal_semaphore_handle_t const sem_hdl, uint32_t msec, tusb_error_t *p_error, int num_call)
//{
//
//}
//tusb_error_t sem_post_stub(osal_semaphore_handle_t const sem_hdl, int num_call)
//{
//  (*sem_hdl)++;
//
//  return TUSB_ERROR_NONE;
//}
//void sem_reset_stub(osal_semaphore_handle_t const sem_hdl, int num_call)
//{
//  (*sem_hdl) = 0;
//}
//
//osal_queue_handle_t  queue_create_stub  (osal_queue_t *p_queue, int num_call)
//{
//  p_queue->count = p_queue->wr_idx = p_queue->rd_idx = 0;
//  return (osal_queue_handle_t) p_queue;
//}
//void                 queue_receive_stub (osal_queue_handle_t const queue_hdl, uint32_t *p_data, uint32_t msec, tusb_error_t *p_error, int num_call)
//{
//
//}
//tusb_error_t         queue_send_stub    (osal_queue_handle_t const queue_hdl, uint32_t data, int num_call)
//{
//  //TODO mutex lock hal_interrupt_disable
//
//  queue_hdl->buffer[queue_hdl->wr_idx] = data;
//  queue_hdl->wr_idx = (queue_hdl->wr_idx + 1) % queue_hdl->depth;
//
//  if (queue_hdl->depth == queue_hdl->count) // queue is full, 1st rd is overwritten
//  {
//    queue_hdl->rd_idx = queue_hdl->wr_idx; // keep full state
//  }else
//  {
//    queue_hdl->count++;
//  }
//
//  //TODO mutex unlock hal_interrupt_enable
//
//  return TUSB_ERROR_NONE;
//}
//void queue_flush_stub(osal_queue_handle_t const queue_hdl, int num_call)
//{
//  queue_hdl->count = queue_hdl->rd_idx = queue_hdl->wr_idx = 0;
//}
//
//void test_(void)
//{
//  ehci_controller_device_plug(hostid, TUSB_SPEED_HIGH);
//
//  tusb_task_runner(); // get 8-byte descriptor
//  ehci_controller_control_xfer_proceed(0, &desc_device);
//
//  tusb_task_runner(); // get 8-byte descriptor
//}
