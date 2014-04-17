/**************************************************************************/
/*!
    @file     test_osal_freeRTOS.c
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

#include "unity.h"
#include "tusb_errors.h"
//#include "freeRTOS.h"
//#include "osal_freeRTOS.h"
//#include "mock_task.h"
//#include "queue.h"
//#include "mock_portmacro.h"
//#include "mock_portable.h"
//#include "mock_list.h"
//
//#define QUEUE_DEPTH 10
//
//uint32_t statements[10];
//
//OSAL_SEM_DEF(sem);
//osal_semaphore_handle_t sem_hdl;
//
//OSAL_QUEUE_DEF(queue, QUEUE_DEPTH, uin32_t);
//osal_queue_handle_t queue_hdl;

void setUp(void)
{
//  memset(statements, 0, sizeof(statements));
//  sem_hdl = osal_semaphore_create(OSAL_SEM_REF(sem));
//  queue_hdl = osal_queue_create(&queue);
}

void tearDown(void)
{

}

void test_(void)
{
  tusb_error_t error = TUSB_ERROR_NONE;
  uint32_t data;

//  TEST_IGNORE();
//
//  osal_semaphore_post(sem_hdl);
//  osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
//
//  uint32_t item = 0x1234;
//  osal_queue_send(queue_hdl, &item);
//  osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_NORMAL, &error);

}
