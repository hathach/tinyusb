/**************************************************************************/
/*!
    @file     test_osal_none.c
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

#ifdef TUSB_CFG_OS
#undef TUSB_CFG_OS
#endif

void setUp(void)
{
}

void tearDown(void)
{

}

#if 0 // TODO enable
#include "unity.h"
#include "tusb_errors.h"
#include "osal_none.h"

#define QUEUE_DEPTH 10

uint32_t statements[10];

OSAL_SEM_DEF(sem);
osal_semaphore_handle_t sem_hdl;

OSAL_QUEUE_DEF(queue, QUEUE_DEPTH, uint32_t);
osal_queue_handle_t queue_hdl;

OSAL_MUTEX_DEF(mutex);
osal_mutex_handle_t mutex_hdl;

void setUp(void)
{
  memset(statements, 0, sizeof(statements));
  sem_hdl   = osal_semaphore_create (OSAL_SEM_REF(sem));
  queue_hdl = osal_queue_create     (OSAL_QUEUE_REF(queue));
  mutex_hdl = osal_mutex_create     (OSAL_MUTEX_REF(mutex));
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
// Mutex
//--------------------------------------------------------------------+
void test_mutex_create(void)
{
  TEST_ASSERT_EQUAL_PTR(&mutex, mutex_hdl);
  TEST_ASSERT_EQUAL(1, mutex);
}

void test_mutex_release(void)
{
  osal_mutex_release(mutex_hdl);
  TEST_ASSERT_EQUAL(1, mutex);
}

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
  osal_queue_send(queue_hdl, &item);
  TEST_ASSERT_EQUAL(1, queue_hdl->count);
  TEST_ASSERT_EQUAL_MEMORY(&item, queue_hdl->buffer + (queue_hdl->rd_idx * queue_hdl->item_size), 4);
}
// blocking service such as semaphore wait need to be invoked within a task's loop

//--------------------------------------------------------------------+
// TASK SEMAPHORE
//--------------------------------------------------------------------+
tusb_error_t sample_task_semaphore(void)
{
  tusb_error_t error = TUSB_ERROR_NONE;

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
  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 - 1  ; i++) // not enough time
    osal_tick_tock();
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(0, statements[4]);
  osal_tick_tock();
  sample_task_semaphore();

  // reach end of task loop, back to beginning
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(2, statements[0]);
}

//--------------------------------------------------------------------+
// TASK MUTEX
//--------------------------------------------------------------------+
tusb_error_t mutex_sample_task1(void) // occupy mutex and not release it
{
  tusb_error_t error = TUSB_ERROR_NONE;

  OSAL_TASK_LOOP_BEGIN

  statements[0]++;

  osal_mutex_wait(mutex_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  statements[2]++;

  OSAL_TASK_LOOP_END
}

tusb_error_t mutex_sample_task2(void)
{
  tusb_error_t error = TUSB_ERROR_NONE;

  OSAL_TASK_LOOP_BEGIN

  statements[1]++;

  osal_mutex_wait(mutex_hdl, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  statements[3]++;

  osal_mutex_wait(mutex_hdl, OSAL_TIMEOUT_NORMAL, &error);
  statements[5]++;

  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TIMEOUT, error);

  OSAL_TASK_LOOP_END
}

void test_task_with_mutex(void)
{
  // several invoke before mutex is available
  mutex_sample_task1();
  for(uint32_t i=0; i<10; i++) {
    mutex_sample_task2();
  }

  TEST_ASSERT_EQUAL(1, statements[0]);
  TEST_ASSERT_EQUAL(1, statements[2]);

  TEST_ASSERT_EQUAL(1, statements[1]);
  TEST_ASSERT_EQUAL(0, statements[3]);

  // invoke after posting mutex
  osal_mutex_release(mutex_hdl);
  for(uint32_t i=0; i<10; i++) {
    mutex_sample_task2();
  }
  TEST_ASSERT_EQUAL(1, statements[3]);
  TEST_ASSERT_EQUAL(0, statements[5]);

  // timeout
  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 - 1 ; i++){ // one tick less
    osal_tick_tock();
  }
  mutex_sample_task2();
  TEST_ASSERT_EQUAL(0, statements[5]);
  osal_tick_tock();

  mutex_sample_task2();
  TEST_ASSERT_EQUAL(1, statements[5]);
}

//--------------------------------------------------------------------+
// TASK QUEUE
//--------------------------------------------------------------------+
tusb_error_t sample_task_with_queue(void)
{
  uint32_t data;
  tusb_error_t error;

  OSAL_TASK_LOOP_BEGIN

  statements[0]++;

  osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  TEST_ASSERT_EQUAL(0x1111, data);
  statements[1]++;

  osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  TEST_ASSERT_EQUAL(0x2222, data);
  statements[2]++;

  osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  TEST_ASSERT_EQUAL(0x3333, data);
  statements[3]++;

  osal_queue_receive(queue_hdl, &data, OSAL_TIMEOUT_NORMAL, &error);
  statements[4]++;
  TEST_ASSERT_EQUAL(TUSB_ERROR_OSAL_TIMEOUT, error);

  OSAL_TASK_LOOP_END
}

void test_task_with_queue(void)
{
  uint32_t i = 0;
  uint32_t item;

  sample_task_with_queue();
  // several invoke before queue is available
  for(i=0; i<10; i++)
    sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[0]);

  // invoke after sending to queue
  item = 0x1111;
  osal_queue_send(queue_hdl, &item);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[1]);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[1]);

  item = 0x2222;
  osal_queue_send(queue_hdl, &item);
  item = 0x3333;
  osal_queue_send(queue_hdl, &item);
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(1, statements[2]);
  TEST_ASSERT_EQUAL(1, statements[3]);

  // timeout
  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 - 1 ; i++) // not enough time
    osal_tick_tock();
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(0, statements[4]);
  osal_tick_tock();
  sample_task_with_queue();

  // reach end of task loop, back to beginning
  sample_task_with_queue();
  TEST_ASSERT_EQUAL(2, statements[0]);
}

//--------------------------------------------------------------------+
// TASK DELAY
//--------------------------------------------------------------------+
tusb_error_t sample_task_with_delay(void)
{
  tusb_error_t error;

  OSAL_TASK_LOOP_BEGIN

  osal_task_delay(1000);

  statements[0]++;

  OSAL_TASK_LOOP_END
}

void test_task_with_delay(void)
{
  sample_task_with_delay();
  TEST_ASSERT_EQUAL(0, statements[0]);

  for(uint32_t i=0; i<TUSB_CFG_OS_TICKS_PER_SECOND*1000; i++) // not enough time
    osal_tick_tock();

  sample_task_with_delay();
  TEST_ASSERT_EQUAL(1, statements[0]);
}

//--------------------------------------------------------------------+
// TASK FLOW CONTROL
//--------------------------------------------------------------------+
void flow_control_error_handler(void)
{
  statements[5]++;
}

tusb_error_t sample_flow_control_subtask2(void)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  statements[0]++;

  osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_NORMAL, &error);
  SUBTASK_ASSERT(TUSB_ERROR_NONE == error);
  statements[1]++;

  osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_NORMAL, &error);
  SUBTASK_ASSERT_STATUS(error);
  statements[2]++;

  osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_NORMAL, &error);
  SUBTASK_ASSERT_STATUS_WITH_HANDLER(error, flow_control_error_handler());
  statements[3]++;

  OSAL_SUBTASK_END
}

tusb_error_t sample_flow_control_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  sample_flow_control_subtask2();

  OSAL_SUBTASK_END
}

tusb_error_t sample_task_flow_control(void)
{
  OSAL_TASK_LOOP_BEGIN

  sample_flow_control_subtask();

  OSAL_TASK_LOOP_END
}

void test_task_flow_control_assert(void)
{
  sample_task_flow_control();
  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 + 1; i++) osal_tick_tock();
  sample_task_flow_control();
  TEST_ASSERT_EQUAL(0, statements[1]);
}

void test_task_flow_control_assert_status(void)
{
  for (uint8_t i=0; i<1; i++) osal_semaphore_post(sem_hdl);

  sample_task_flow_control();

  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 + 1; i++) osal_tick_tock();
  sample_task_flow_control();

  TEST_ASSERT_EQUAL(0, statements[2]);
}

void test_task_flow_control_assert_status_hanlder(void)
{
  for (uint8_t i=0; i<2; i++) osal_semaphore_post(sem_hdl);

  sample_task_flow_control();

  for(uint32_t i=0; i<(OSAL_TIMEOUT_NORMAL*TUSB_CFG_OS_TICKS_PER_SECOND)/1000 + 1; i++) osal_tick_tock();
  sample_task_flow_control();

  TEST_ASSERT_EQUAL(0, statements[3]);
  TEST_ASSERT_EQUAL(1, statements[5]);
}
#endif
