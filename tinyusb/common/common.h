/*
 * common.h
 *
 *  Created on: Nov 26, 2012
 *      Author: hathach (thachha@live.com)
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (thachha@live.com)
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

#ifndef _TUSB_COMMON_H_
#define _TUSB_COMMON_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "arch/arch.h"
#include "compiler/compiler.h"
#include "errors.h"

//#if ( defined CFG_PRINTF_UART || defined CFG_PRINTF_USBCDC || defined CFG_PRINTF_DEBUG )
#if 1
  #define PRINTF_LOCATION(mess)	printf("Assert: %s at line %d: %s\n", __func__, __LINE__, mess)
#else
  #define PRINTF_LOCATION(mess)
#endif

#define ASSERT_MESSAGE(condition, value, message) \
	do{\
	  if (!(condition)) {\
			PRINTF_LOCATION(message);\
			return (value);\
		}\
	}while(0)

#define ASSERT(condition, value)  ASSERT_MESSAGE(condition, value, NULL)

#define ASSERT_STATUS_MESSAGE(sts, message) \
	do{\
	  ErrorCode_t status = (sts);\
	  if (LPC_OK != status) {\
	    PRINTF_LOCATION(message);\
	    return status;\
	  }\
	}while(0)

#define ASSERT_STATUS(sts)		ASSERT_STATUS_MESSAGE(sts, NULL)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMMON_H_ */
