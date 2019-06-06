/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

// tu_log2 a value is equivalent to its highest set bit's position
#define BITFIELD_OFFSET_OF_MEMBER(struct_type, member, bitfield_member) \
  ({\
    uint32_t value=0;\
    struct_type str;\
    tu_memclr((void*)&str, sizeof(struct_type));\
    str.member.bitfield_member = 1;\
    memcpy(&value, (void*)&str.member, sizeof(str.member));\
    tu_log2( value );\
  })

#define BITFIELD_OFFSET_OF_UINT32(struct_type, offset, bitfield_member) \
  ({\
    struct_type str;\
    tu_memclr(&str, sizeof(struct_type));\
    str.bitfield_member = 1;\
    tu_log2( ((uint32_t*) &str)[offset] );\
  })

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TYPE_HELPER_H_ */

/** @} */
