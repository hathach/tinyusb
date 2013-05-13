/**************************************************************************/
/*!
    @file     type_helper.h
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

#ifndef _TUSB_TYPE_HELPER_H_
#define _TUSB_TYPE_HELPER_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define TEST_ASSERT_PIPE_HANDLE(expected, actual)\
    TEST_ASSERT_EQUAL(expected.dev_addr, actual.dev_addr);\
    TEST_ASSERT_EQUAL(expected.xfer_type, actual.xfer_type);\
    TEST_ASSERT_EQUAL(expected.index, actual.index);\

#define TEST_ASSERT_MEM_ZERO(buffer, size)\
  do{\
    for (uint32_t i=0; i<size; i++)\
      TEST_ASSERT_EQUAL_HEX8(0, ((uint8_t*)buffer)[i]);\
  }while(0)

#define TEST_ASSERT_STATUS( actual )\
  TEST_ASSERT_EQUAL( TUSB_ERROR_NONE, (actual) )
#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TYPE_HELPER_H_ */

/** @} */
