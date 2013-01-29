/*
 * osal.h
 *
 *  Created on: Jan 18, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_OSAL_H_
#define _TUSB_OSAL_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"

#define TUSB_OS_NONE     1
#define TUSB_OS_CMSIS    2
#define TUSB_OS_FREERTOS 3
#define TUSB_OS_UCOS     4

#if TUSB_CFG_OS == TUSB_OS_NONE
  #include "osal_none.h"
#else
  #error TUSB_CFG_OS is not defined or OS is not supported yet
#endif

typedef uint32_t osal_status_t; // TODO OSAL port
typedef uint32_t osal_timeout_t; // TODO OSAL port

enum
{
  OSAL_TIMEOUT_WAIT_FOREVER = 0
};

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
typedef uint32_t osal_queue_id_t;
//osal_queue_id_t osal_queue_create(osal_queue_t *queue, uint8_t *buffer);
osal_queue_id_t osal_queue_create(osal_queue_id_t *queue, uint8_t *buffer);
tusb_error_t osal_queue_put(osal_queue_id_t qid, uint32_t data, osal_timeout_t msec);
tusb_error_t osal_queue_get(osal_queue_id_t qid, uint32_t *data, osal_timeout_t msec);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_H_ */

/** @} */
