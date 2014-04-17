/**************************************************************************/
/*!
    @file     test_hidh_keyboard.c
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
//  dev_addr = RANDOM(TUSB_CFG_HOST_DEVICE_MAX)+1;
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
