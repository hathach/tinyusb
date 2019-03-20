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
