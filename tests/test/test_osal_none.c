/*
 * test_osal_none.c
 *
 *  Created on: Jan 22, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#ifdef TUSB_CFG_OS
#undef TUSB_CFG_OS
#endif

#include "unity.h"
#include "osal_none.h"

#define QUEUE_DEPTH 10

uint32_t statements[10];

osal_semaphore_t sem;
osal_semaphore_handle_t sem_hdl;

OSAL_DEF_QUEUE(queue, QUEUE_DEPTH, uin32_t);
osal_queue_handle_t queue_hdl;

void setUp(void)
{
  memset(statements, 0, sizeof(statements));
  sem_hdl = osal_semaphore_create(&sem);
  queue_hdl = osal_queue_create(&queue);
}

void tearDown(void)
{

}

//--------------------------------------------------------------------+
// Semaphore
//--------------------------------------------------------------------+
void test_semaphore_create(void)
{
  TEST_ASSERT_EQUAL_PTR(&sem, sem_hdl);
  TEST_ASSERT_EQUAL(0, sem);
}

void test_semaphore_post(void)
{
  osal_semaphore_post(sem_hdl);
  TEST_ASSERT_EQUAL(1, sem);
}
// blocking service such as semaphore wait need to be invoked within a task's loop

//--------------------------------------------------------------------+
// Queue
//--------------------------------------------------------------------+
void test_queue_create(void)
{
  TEST_ASSERT_EQUAL_PTR(&queue, queue_hdl);
  TEST_ASSERT_EQUAL(QUEUE_DEPTH, queue_hdl->depth);
  TEST_ASSERT_EQUAL_PTR(queue_buffer, queue_hdl->buffer);
  TEST_ASSERT_EQUAL(0, queue_hdl->count);
  TEST_ASSERT_EQUAL(0, queue_hdl->wr_idx);
  TEST_ASSERT_EQUAL(0, queue_hdl->rd_idx);
}

void test_queue_send(void)
{
  uint32_t const item = 0x1234;
  osal_queue_send(queue_hdl, item);
  TEST_ASSERT_EQUAL(1, queue_hdl->count);
  TEST_ASSERT_EQUAL(item, queue_hdl->buffer[queue_hdl->rd_idx]);
}
// blocking service such as semaphore wait need to be invoked within a task's loop

//--------------------------------------------------------------------+
// TASK
//--------------------------------------------------------------------+
void sample_task_semaphore(void)
{
  tusb_error_t error = TUSB_ERROR_NONE;
  OSAL_TASK_LOOP
  {
    OSAL_TASK_LOOP_BEGIN

    statements[0]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
    statements[1]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
    statements[2]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
    statements[3]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_NORMAL, &error);
    statements[4]++;
    TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TIMEOUT, error);

    OSAL_TASK_LOOP_END
  }
}

void test_task_with_semaphore(void)
{
  // several invoke before sempahore is available
  for(uint32_t i=0; i<10; i++)
    sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[0]);

  // invoke after posting semaphore
  osal_semaphore_post(sem_hdl);
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[1]);

  // post 2 consecutive times
  osal_semaphore_post(sem_hdl);
  osal_semaphore_post(sem_hdl);
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[2]);
  TEST_ASSERT_EQUAL(1, statements[3]);

  // timeout
  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 ; i++) // not enough time
    osal_tick_tock();
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(0, statements[4]);
  osal_tick_tock();
  sample_task_semaphore();

  // reach end of task loop, back to beginning
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(2, statements[0]);
}

void sample_task_with_queue(void)
{
  uint32_t data;
  OSAL_TASK_LOOP
  {
    OSAL_TASK_LOOP_BEGIN

    statements[0]++;

    osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(0x1111, data);
    statements[1]++;

    osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(0x2222, data);
    statements[2]++;

    osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(0x3333, data);
    statements[3]++;

    OSAL_TASK_LOOP_END
  }
}

void test_task_with_queue(void)
{
  uint32_t i = 0;

  sample_task_with_queue();
  // several invoke before queue is available
  for(i=0; i<10; i++)
    sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[0]);

  // invoke after posting semaphore
  osal_queue_send(queue_hdl, 0x1111);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[1]);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[1]);

  osal_queue_send(queue_hdl, 0x2222);
  osal_queue_send(queue_hdl, 0x3333);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[2]);
  TEST_ASSERT_EQUAL(1, statements[3]);

}

