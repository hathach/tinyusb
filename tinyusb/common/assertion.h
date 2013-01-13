/*
 * assertion.h
 *
 *  Created on: Jan 11, 2013
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

#ifndef _TUSB_ASSERTION_H_
#define _TUSB_ASSERTION_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "tusb_option.h"

  //#if ( defined CFG_PRINTF_UART || defined CFG_PRINTF_USBCDC || defined CFG_PRINTF_DEBUG )
#if TUSB_CFG_DEBUG
  #define _PRINTF(...)	printf(__VA_ARGS__)
#else
  #define _PRINTF(...)
#endif

//--------------------------------------------------------------------+
// Assert Helper
//--------------------------------------------------------------------+
#define ASSERT_FILENAME  __BASE_FILE__
#define ASSERT_FUNCTION  __PRETTY_FUNCTION__
//#define ASSERT_STATEMENT _PRINTF("Assert at %s line %d: %s %s\n", ASSERT_FILENAME, ASSERT_FUNCTION, __LINE__);
#define ASSERT_STATEMENT _PRINTF("assert at %s: %s :%d :\n", ASSERT_FILENAME, ASSERT_FUNCTION, __LINE__)
#define ASSERT_DEFINE(ASSERT_TEST, error, format, __VA_ARGS__) \
  do{\
	  if (!(ASSERT_TEST)) {\
	    _PRINTF("Assert at %s: %s:%d: " format "\n", ASSERT_FILENAME, ASSERT_FUNCTION, __LINE__, __VA_ARGS__);\
	    return error;\
	  }\
	}while(0)

//--------------------------------------------------------------------+
// TUSB_Error_t Status Assert
//--------------------------------------------------------------------+
#define ASSERT_STATUS_MESSAGE(sts, message) \
	do{\
	  TUSB_Error_t status = (TUSB_Error_t)(sts);\
	  if (tERROR_NONE != status) {\
	    _PRINTF("Assert at %s line %d: %s %s\n", ASSERT_FILENAME, ASSERT_FUNCTION, __LINE__, TUSB_ErrorStr[status], message); \
	    return status;\
	  }\
	}while(0)

#define ASSERT_STATUS(sts)		ASSERT_STATUS_MESSAGE(sts, NULL)

//--------------------------------------------------------------------+
// Logical Assert
//--------------------------------------------------------------------+
#define TEST_TRUE(condition)  (condition)
#define TEST_FALSE(condition)  (!condition)

#define ASSERT(condition, error       ) ASSERT_TRUE(condition, error)
#define ASSERT_TRUE(condition, error  ) ASSERT_DEFINE(TEST_TRUE(condition), error, "%s", "evaluated to false")
#define ASSERT_FALSE(condition, error ) ASSERT_DEFINE(TEST_FALSE(condition), error, "%s", "evaluated to true")

//--------------------------------------------------------------------+
// Integer Assert
//--------------------------------------------------------------------+
#define TEST_INT_EQUAL

#define ASSERT_INT(expected, actual)  ASSERT_INT_EQUAL(expected, actual)
#define ASSERT_INT_EQUAL(expected, actual)  \
    uint32 exp = (expected);\
    uint32 act = (actual);\
    ASSERT_DEFINE(TEST_INT_EQUAL(condition), error, "expected %d, actual %d", exp, act)

#define ASSERT_INT_WITHIN(lower, upper, actual)


#ifdef __cplusplus
}
#endif

#endif /* _TUSB_ASSERTION_H_ */

/** @} */
