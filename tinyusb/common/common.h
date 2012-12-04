/*
 * common.h
 *
 *  Created on: Nov 26, 2012
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
 * This file is part of the tinyUSB stack.
 */

/** \file
 *  \brief Common Header File
 *
 *  \note TBD
 */

/** \defgroup Group_Common Common Files
 * @{
 *
 *  \defgroup Group_CommonH common.h
 *
 *  @{
 */

#ifndef _TUSB_COMMON_H_
#define _TUSB_COMMON_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "tusb_cfg.h"
#include "arch/arch.h"
#include "hal/hal.h"
#include "compiler/compiler.h"
#include "errors.h"
#include "core/tusb_types.h"
#include "core/std_descriptors.h"

/// min value
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

/// max value
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

/// n-th Bit
#ifndef BIT
#define BIT(n) (1 << (n))
#endif

/// set n-th bit of x to 1
#ifndef BIT_SET
#define BIT_SET(x, n) ( (x) | BIT(n) )
#endif

/// clear n-th bit of x
#ifndef BIT_CLR
#define BIT_CLR(x, n) ( (x) & (~BIT(n)) )
#endif

//#if ( defined CFG_PRINTF_UART || defined CFG_PRINTF_USBCDC || defined CFG_PRINTF_DEBUG )
#if CFG_TUSB_DEBUG_LEVEL
  #define PRINTF(...)	printf(__VA_ARGS__)
#else
  #define PRINTF(...)
#endif

#define ASSERT_MESSAGE(condition, value, message) \
	do{\
	  if (!(condition)) {\
			PRINTF("Assert at %s line %d: %s\n", __func__, __LINE__, message); \
			return (value);\
		}\
	}while(0)

#define ASSERT(condition, value)  ASSERT_MESSAGE(condition, value, NULL)

#define ASSERT_ERROR_MESSAGE(sts, message) \
	do{\
	  TUSB_Error_t status = (TUSB_Error_t)(sts);\
	  if (tERROR_NONE != status) {\
	    PRINTF("Assert at %s line %d: %s %s\n", __func__, __LINE__, TUSB_ErrorStr[status], message); \
	    return status;\
	  }\
	}while(0)

#define ASSERT_ERROR(sts)		ASSERT_ERROR_MESSAGE(sts, NULL)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMMON_H_ */

/**  @} */
/**  @} */
