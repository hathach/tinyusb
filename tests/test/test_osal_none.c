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

#define TUSB_CFG_OS TUSB_OS_NONE
#include "unity.h"
#include "osal.h"

uint32_t statements[10];
osal_semaphore_t sem;
osal_semaphore_handle_t sem_hdl;

void setUp(void)
{
  memset(statements, 0, sizeof(statements));
  sem = 0;
  sem_hdl = osal_semaphore_create(&sem);
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

//--------------------------------------------------------------------+
// Queue
//--------------------------------------------------------------------+
void test_queue_create(void)
{
  TEST_IGNORE();
//  osal_queue_put();
}

//--------------------------------------------------------------------+
// TASK
//--------------------------------------------------------------------+
void sample_task_semaphore(void)
{
  OSAL_TASK_LOOP
  {
    OSAL_TASK_LOOP_BEGIN

    statements[0]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER);
    statements[1]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER);
    statements[2]++;

    osal_semaphore_wait(sem_hdl, OSAL_TIMEOUT_WAIT_FOREVER);
    statements[3]++;

    OSAL_TASK_LOOP_END
  }
}

void test_task_with_semaphore(void)
{
  uint32_t i;
  // several invoke before sempahore is available
  for(i=0; i<10; i++)
    sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[0]);

  // several invoke after posting semaphore
  osal_semaphore_post(sem_hdl);
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[1]);

  osal_semaphore_post(sem_hdl);
  osal_semaphore_post(sem_hdl);
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(1, statements[2]);
  TEST_ASSERT_EQUAL(1, statements[3]);

  // reach end of task loop, back to beginning
  sample_task_semaphore();
  TEST_ASSERT_EQUAL(2, statements[0]);

}



