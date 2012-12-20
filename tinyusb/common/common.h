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

#include "compiler/compiler.h"
#include "tusb_cfg.h"
#include "errors.h"
#include "mcu/mcu.h"

#include "hal/hal.h"
#include "core/tusb_types.h"
#include "core/std_descriptors.h"

/// min value
#define MIN_(x, y) (((x) < (y)) ? (x) : (y))

/// max value
#define MAX_(x, y) (((x) > (y)) ? (x) : (y))

/// n-th Bit
#define BIT_(n) (1 << (n))

/// set n-th bit of x to 1
#define BIT_SET_(x, n) ( (x) | BIT_(n) )

/// clear n-th bit of x
#define BIT_CLR_(x, n) ( (x) & (~BIT_(n)) )

/// add hex represenation
#define HEX_(n) 0x##n##LU

//  internal macro of B8, B16, B32
#define _B8__(x) ((x&0x0000000FLU)?1:0) \
                +((x&0x000000F0LU)?2:0) \
                +((x&0x00000F00LU)?4:0) \
                +((x&0x0000F000LU)?8:0) \
                +((x&0x000F0000LU)?16:0) \
                +((x&0x00F00000LU)?32:0) \
                +((x&0x0F000000LU)?64:0) \
                +((x&0xF0000000LU)?128:0)

#define B8_(d) ((unsigned char)_B8__(HEX_(d)))
#define B16_(dmsb,dlsb) (((unsigned short)B8(dmsb)<<8) + B8(dlsb))
#define B32_(dmsb,db2,db3,dlsb) \
            (((unsigned long)B8(dmsb)<<24) \
            + ((unsigned long)B8(db2)<<16) \
            + ((unsigned long)B8(db3)<<8) \
            + B8(dlsb))



//#if ( defined CFG_PRINTF_UART || defined CFG_PRINTF_USBCDC || defined CFG_PRINTF_DEBUG )
#if TUSB_CFG_DEBUG
  #define PRINTF(...)	printf(__VA_ARGS__)
#else
  #define PRINTF(...)
#endif

#define ASSERT_MESSAGE(condition, value, message) \
	do{\
	  if (!(condition)) {\
			PRINTF("Assert at %s %s line %d: %s\n", __BASE_FILE__, __PRETTY_FUNCTION__, __LINE__, message); \
			return (value);\
		}\
	}while(0)

#define ASSERT(condition, value)  ASSERT_MESSAGE(condition, value, NULL)

#define ASSERT_ERROR_MESSAGE(sts, message) \
	do{\
	  TUSB_Error_t status = (TUSB_Error_t)(sts);\
	  if (tERROR_NONE != status) {\
	    PRINTF("Assert at %s line %d: %s %s\n", __BASE_FILE__, __PRETTY_FUNCTION__, __LINE__, TUSB_ErrorStr[status], message); \
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
