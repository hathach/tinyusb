/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stdio.h>
#include <setjmp.h>

//stdint.h is often automatically included.
//Unity uses it to guess at the sizes of integer types, etc.
#ifdef UNITY_USE_LIMITS_H
#include <limits.h>
#endif
#ifndef UNITY_EXCLUDE_STDINT_H
#include <stdint.h>
#endif

//-------------------------------------------------------
// Guess Widths If Not Specified
//-------------------------------------------------------

// If the INT Width hasn't been specified,
// We first try to guess based on UINT_MAX (if it exists)
// Otherwise we fall back on assuming 32-bit
#ifndef UNITY_INT_WIDTH
  #ifdef UINT_MAX
    #if (UINT_MAX == 0xFFFF)
      #define UNITY_INT_WIDTH (16)
    #elif (UINT_MAX == 0xFFFFFFFF)
      #define UNITY_INT_WIDTH (32)
    #elif (UINT_MAX == 0xFFFFFFFFFFFFFFFF)
      #define UNITY_INT_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #else
      #define UNITY_INT_WIDTH (32)
    #endif
  #else
    #define UNITY_INT_WIDTH (32)
  #endif
#endif

// If the Long Width hasn't been specified,
// We first try to guess based on ULONG_MAX (if it exists)
// Otherwise we fall back on assuming 32-bit
#ifndef UNITY_LONG_WIDTH
  #ifdef ULONG_MAX
    #if (ULONG_MAX == 0xFFFF)
      #define UNITY_LONG_WIDTH (16)
    #elif (ULONG_MAX == 0xFFFFFFFF)
      #define UNITY_LONG_WIDTH (32)
    #elif (ULONG_MAX == 0xFFFFFFFFFFFFFFFF)
      #define UNITY_LONG_WIDTH (64)
    #else
      #define UNITY_LONG_WIDTH (32)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #else
    #define UNITY_LONG_WIDTH (32)
  #endif
#endif

// If the Pointer Width hasn't been specified,
// We first try to guess based on INTPTR_MAX (if it exists)
// Otherwise we fall back on assuming 32-bit
#ifndef UNITY_POINTER_WIDTH
  #ifdef UINTPTR_MAX
    #if (UINTPTR_MAX <= 0xFFFF)
      #define UNITY_POINTER_WIDTH (16)
    #elif (UINTPTR_MAX <= 0xFFFFFFFF)
      #define UNITY_POINTER_WIDTH (32)
    #elif (UINTPTR_MAX <= 0xFFFFFFFFFFFFFFFF)
      #define UNITY_POINTER_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_POINTER_WIDTH
  #ifdef INTPTR_MAX
    #if (INTPTR_MAX <= 0x7FFF)
      #define UNITY_POINTER_WIDTH (16)
    #elif (INTPTR_MAX <= 0x7FFFFFFF)
      #define UNITY_POINTER_WIDTH (32)
    #elif (INTPTR_MAX <= 0x7FFFFFFFFFFFFFFF)
      #define UNITY_POINTER_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_POINTER_WIDTH
  #define UNITY_POINTER_WIDTH (32)
#endif

//-------------------------------------------------------
// Int Support
//-------------------------------------------------------

#if (UNITY_INT_WIDTH == 32)
    typedef unsigned char   _UU8;
    typedef unsigned short  _UU16;
    typedef unsigned int    _UU32;
    typedef signed char     _US8;
    typedef signed short    _US16;
    typedef signed int      _US32;
#elif (UNITY_INT_WIDTH == 16)
    typedef unsigned char   _UU8;
    typedef unsigned int    _UU16;
    typedef unsigned long   _UU32;
    typedef signed char     _US8;
    typedef signed int      _US16;
    typedef signed long     _US32;
#else
    #error Invalid UNITY_INT_WIDTH specified! (16 or 32 are supported)
#endif

//-------------------------------------------------------
// 64-bit Support
//-------------------------------------------------------

#ifndef UNITY_SUPPORT_64

//No 64-bit Support
typedef _UU32 _U_UINT;
typedef _US32 _U_SINT;

#else

//64-bit Support
#if (UNITY_LONG_WIDTH == 32)
    typedef unsigned long long _UU64;
    typedef signed long long   _US64;
#elif (UNITY_LONG_WIDTH == 64)
    typedef unsigned long      _UU64;
    typedef signed long        _US64;
#else
    #error Invalid UNITY_LONG_WIDTH specified! (32 or 64 are supported)
#endif
typedef _UU64 _U_UINT;
typedef _US64 _U_SINT;

#endif

//-------------------------------------------------------
// Pointer Support
//-------------------------------------------------------

#ifndef UNITY_POINTER_WIDTH
#define UNITY_POINTER_WIDTH (32)
#endif /* UNITY_POINTER_WIDTH */

#if (UNITY_POINTER_WIDTH == 32)
    typedef _UU32 _UP;
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX32
#elif (UNITY_POINTER_WIDTH == 64)
#ifndef UNITY_SUPPORT_64
#error "You've Specified 64-bit pointers without enabling 64-bit Support. Define UNITY_SUPPORT_64"
#endif
    typedef _UU64 _UP;
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX64
#elif (UNITY_POINTER_WIDTH == 16)
    typedef _UU16 _UP;
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX16
#else
    #error Invalid UNITY_POINTER_WIDTH specified! (16, 32 or 64 are supported)
#endif

//-------------------------------------------------------
// Float Support
//-------------------------------------------------------

#ifdef UNITY_EXCLUDE_FLOAT 

//No Floating Point Support
#undef UNITY_FLOAT_PRECISION
#undef UNITY_FLOAT_TYPE
#undef UNITY_FLOAT_VERBOSE

#else

//Floating Point Support
#ifndef UNITY_FLOAT_PRECISION
#define UNITY_FLOAT_PRECISION (0.00001f)
#endif
#ifndef UNITY_FLOAT_TYPE
#define UNITY_FLOAT_TYPE float
#endif
typedef UNITY_FLOAT_TYPE _UF;
    
#endif

//-------------------------------------------------------
// Double Float Support
//-------------------------------------------------------

//unlike FLOAT, we DON'T include by default
#ifndef UNITY_EXCLUDE_DOUBLE
#ifndef UNITY_INCLUDE_DOUBLE
#define UNITY_EXCLUDE_DOUBLE
#endif
#endif

#ifdef UNITY_EXCLUDE_DOUBLE 

//No Floating Point Support
#undef UNITY_DOUBLE_PRECISION
#undef UNITY_DOUBLE_TYPE
#undef UNITY_DOUBLE_VERBOSE

#else

//Floating Point Support
#ifndef UNITY_DOUBLE_PRECISION
#define UNITY_DOUBLE_PRECISION (1e-12f) 
#endif
#ifndef UNITY_DOUBLE_TYPE
#define UNITY_DOUBLE_TYPE double
#endif
typedef UNITY_DOUBLE_TYPE _UD;
    
#endif

//-------------------------------------------------------
// Output Method
//-------------------------------------------------------

#ifndef UNITY_OUTPUT_CHAR
//Default to using putchar, which is defined in stdio.h above
#define UNITY_OUTPUT_CHAR(a) putchar(a)
#else
//If defined as something else, make sure we declare it here so it's ready for use
extern int UNITY_OUTPUT_CHAR(int);
#endif

//-------------------------------------------------------
// Footprint
//-------------------------------------------------------

#ifndef UNITY_LINE_TYPE
#define UNITY_LINE_TYPE _U_UINT
#endif

#ifndef UNITY_COUNTER_TYPE
#define UNITY_COUNTER_TYPE _U_UINT
#endif

//-------------------------------------------------------
// Internal Structs Needed
//-------------------------------------------------------

typedef void (*UnityTestFunction)(void);

#define UNITY_DISPLAY_RANGE_INT  (0x10)
#define UNITY_DISPLAY_RANGE_UINT (0x20)
#define UNITY_DISPLAY_RANGE_HEX  (0x40)
#define UNITY_DISPLAY_RANGE_AUTO (0x80)

typedef enum
{
#if (UNITY_INT_WIDTH == 16)
    UNITY_DISPLAY_STYLE_INT      = 2 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 32)
    UNITY_DISPLAY_STYLE_INT      = 4 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 64)
    UNITY_DISPLAY_STYLE_INT      = 8 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#endif
    UNITY_DISPLAY_STYLE_INT8     = 1 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT16    = 2 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT32    = 4 + UNITY_DISPLAY_RANGE_INT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_INT64    = 8 + UNITY_DISPLAY_RANGE_INT,
#endif

#if (UNITY_INT_WIDTH == 16)
    UNITY_DISPLAY_STYLE_UINT     = 2 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 32)
    UNITY_DISPLAY_STYLE_UINT     = 4 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 64)
    UNITY_DISPLAY_STYLE_UINT     = 8 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#endif
    UNITY_DISPLAY_STYLE_UINT8    = 1 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT16   = 2 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT32   = 4 + UNITY_DISPLAY_RANGE_UINT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_UINT64   = 8 + UNITY_DISPLAY_RANGE_UINT,
#endif
    UNITY_DISPLAY_STYLE_HEX8     = 1 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX16    = 2 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX32    = 4 + UNITY_DISPLAY_RANGE_HEX,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_HEX64    = 8 + UNITY_DISPLAY_RANGE_HEX,
#endif
    UNITY_DISPLAY_STYLE_UNKNOWN
} UNITY_DISPLAY_STYLE_T;

struct _Unity
{
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    UNITY_COUNTER_TYPE TestIgnores;
    UNITY_COUNTER_TYPE CurrentTestFailed;
    UNITY_COUNTER_TYPE CurrentTestIgnored;
    jmp_buf AbortFrame;
};

extern struct _Unity Unity;

//-------------------------------------------------------
// Test Suite Management
//-------------------------------------------------------

void UnityBegin(void);
int  UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

//-------------------------------------------------------
// Test Output
//-------------------------------------------------------

void UnityPrint(const char* string);
void UnityPrintMask(const _U_UINT mask, const _U_UINT number);
void UnityPrintNumberByStyle(const _U_SINT number, const UNITY_DISPLAY_STYLE_T style);
void UnityPrintNumber(const _U_SINT number);
void UnityPrintNumberUnsigned(const _U_UINT number);
void UnityPrintNumberHex(const _U_UINT number, const char nibbles);

#ifdef UNITY_FLOAT_VERBOSE
void UnityPrintFloat(const _UF number);
#endif

//-------------------------------------------------------
// Test Assertion Fuctions
//-------------------------------------------------------
//  Use the macros below this section instead of calling
//  these directly. The macros have a consistent naming
//  convention and will pull in file and line information
//  for you.

void UnityAssertEqualNumber(const _U_SINT expected,
                            const _U_SINT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_DISPLAY_STYLE_T style);

void UnityAssertEqualIntArray(const _U_SINT* expected,
                              const _U_SINT* actual,
                              const _UU32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_DISPLAY_STYLE_T style);

void UnityAssertBits(const _U_SINT mask,
                     const _U_SINT expected,
                     const _U_SINT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualStringArray( const char** expected,
                                  const char** actual,
                                  const _UU32 num_elements,
                                  const char* msg,
                                  const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualMemory( const void* expected,
                             const void* actual,
                             const _UU32 length,
                             const _UU32 num_elements,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber);

void UnityAssertNumbersWithin(const _U_SINT delta,
                              const _U_SINT expected,
                              const _U_SINT actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_DISPLAY_STYLE_T style);

void UnityFail(const char* message, const UNITY_LINE_TYPE line);

void UnityIgnore(const char* message, const UNITY_LINE_TYPE line);

#ifndef UNITY_EXCLUDE_FLOAT
void UnityAssertFloatsWithin(const _UF delta,
                             const _UF expected,
                             const _UF actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualFloatArray(const _UF* expected,
                                const _UF* actual,
                                const _UU32 num_elements,
                                const char* msg,
                                const UNITY_LINE_TYPE lineNumber);
#endif

#ifndef UNITY_EXCLUDE_DOUBLE
void UnityAssertDoublesWithin(const _UD delta,
                              const _UD expected,
                              const _UD actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualDoubleArray(const _UD* expected,
                                 const _UD* actual,
                                 const _UU32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber);
#endif

//-------------------------------------------------------
// Basic Fail and Ignore
//-------------------------------------------------------

#define UNITY_TEST_FAIL(line, message)   UnityFail(   (message), (UNITY_LINE_TYPE)line);
#define UNITY_TEST_IGNORE(line, message) UnityIgnore( (message), (UNITY_LINE_TYPE)line);

//-------------------------------------------------------
// Test Asserts
//-------------------------------------------------------

#define UNITY_TEST_ASSERT(condition, line, message)                                              if (condition) {} else {UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, message);}
#define UNITY_TEST_ASSERT_NULL(pointer, line, message)                                           UNITY_TEST_ASSERT(((pointer) == NULL),  (UNITY_LINE_TYPE)line, message)
#define UNITY_TEST_ASSERT_NOT_NULL(pointer, line, message)                                       UNITY_TEST_ASSERT(((pointer) != NULL),  (UNITY_LINE_TYPE)line, message)

#define UNITY_TEST_ASSERT_EQUAL_INT(expected, actual, line, message)                             UnityAssertEqualNumber((_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT8(expected, actual, line, message)                            UnityAssertEqualNumber((_U_SINT)(_US8 )(expected), (_U_SINT)(_US8 )(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT16(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(_US16)(expected), (_U_SINT)(_US16)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT32(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(_US32)(expected), (_U_SINT)(_US32)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_UINT(expected, actual, line, message)                            UnityAssertEqualNumber((_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT8(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(_US8 )(expected), (_U_SINT)(_US8 )(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT16(expected, actual, line, message)                          UnityAssertEqualNumber((_U_SINT)(_US16)(expected), (_U_SINT)(_US16)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT32(expected, actual, line, message)                          UnityAssertEqualNumber((_U_SINT)(_US32)(expected), (_U_SINT)(_US32)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_HEX8(expected, actual, line, message)                            UnityAssertEqualNumber((_U_SINT)(_US8 )(expected), (_U_SINT)(_US8 )(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_EQUAL_HEX16(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(_US16)(expected), (_U_SINT)(_US16)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_EQUAL_HEX32(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(_US32)(expected), (_U_SINT)(_US32)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX32)
#define UNITY_TEST_ASSERT_BITS(mask, expected, actual, line, message)                            UnityAssertBits((_U_SINT)(mask), (_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line)

#define UNITY_TEST_ASSERT_INT_WITHIN(delta, expected, actual, line, message)                     UnityAssertNumbersWithin((_U_SINT)(delta), (_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_UINT_WITHIN(delta, expected, actual, line, message)                    UnityAssertNumbersWithin((_U_SINT)(delta), (_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_HEX8_WITHIN(delta, expected, actual, line, message)                    UnityAssertNumbersWithin((_U_SINT)(_U_UINT)(_UU8 )(delta), (_U_SINT)(_U_UINT)(_UU8 )(expected), (_U_SINT)(_U_UINT)(_UU8 )(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_HEX16_WITHIN(delta, expected, actual, line, message)                   UnityAssertNumbersWithin((_U_SINT)(_U_UINT)(_UU16)(delta), (_U_SINT)(_U_UINT)(_UU16)(expected), (_U_SINT)(_U_UINT)(_UU16)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_HEX32_WITHIN(delta, expected, actual, line, message)                   UnityAssertNumbersWithin((_U_SINT)(_U_UINT)(_UU32)(delta), (_U_SINT)(_U_UINT)(_UU32)(expected), (_U_SINT)(_U_UINT)(_UU32)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX32)

#define UNITY_TEST_ASSERT_EQUAL_PTR(expected, actual, line, message)                             UnityAssertEqualNumber((_U_SINT)(_UP)(expected), (_U_SINT)(_UP)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_POINTER)
#define UNITY_TEST_ASSERT_EQUAL_STRING(expected, actual, line, message)                          UnityAssertEqualString((const char*)(expected), (const char*)(actual), (message), (UNITY_LINE_TYPE)line)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(expected, actual, len, line, message)                     UnityAssertEqualMemory((void*)(expected), (void*)(actual), (_UU32)(len), 1, (message), (UNITY_LINE_TYPE)line)

#define UNITY_TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements, line, message)         UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY(expected, actual, num_elements, line, message)        UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num_elements, line, message)        UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, num_elements, line, message)      UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, actual, num_elements, line, message)      UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements, line, message)        UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX32)
#define UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num_elements, line, message)         UnityAssertEqualIntArray((const _U_SINT*)(_UP*)(expected), (const _U_SINT*)(_UP*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_POINTER)
#define UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements, line, message)      UnityAssertEqualStringArray((const char**)(expected), (const char**)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num_elements, line, message) UnityAssertEqualMemory((void*)(expected), (void*)(actual), (_UU32)(len), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line)

#ifdef UNITY_SUPPORT_64
#define UNITY_TEST_ASSERT_EQUAL_INT64(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_EQUAL_UINT64(expected, actual, line, message)                          UnityAssertEqualNumber((_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_EQUAL_HEX64(expected, actual, line, message)                           UnityAssertEqualNumber((_U_SINT)(expected), (_U_SINT)(actual), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_EQUAL_INT64_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_EQUAL_UINT64_ARRAY(expected, actual, num_elements, line, message)      UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_EQUAL_HEX64_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualIntArray((const _U_SINT*)(expected), (const _U_SINT*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_HEX64_WITHIN(delta, expected, actual, line, message)                   UnityAssertNumbersWithin((_U_SINT)(delta), (_U_SINT)(expected), (_U_SINT)(actual), NULL, (UNITY_LINE_TYPE)line, UNITY_DISPLAY_STYLE_HEX64)
#endif

#ifdef UNITY_EXCLUDE_FLOAT
#define UNITY_TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual, line, message)                   UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Floating Point Disabled")
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(expected, actual, line, message)                           UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Floating Point Disabled")
#define UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements, line, message)       UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Floating Point Disabled")
#else
#define UNITY_TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual, line, message)                   UnityAssertFloatsWithin((_UF)(delta), (_UF)(expected), (_UF)(actual), (message), (UNITY_LINE_TYPE)line)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(expected, actual, line, message)                           UNITY_TEST_ASSERT_FLOAT_WITHIN((_UF)(expected) * (_UF)UNITY_FLOAT_PRECISION, (_UF)expected, (_UF)actual, (UNITY_LINE_TYPE)line, message)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements, line, message)       UnityAssertEqualFloatArray((_UF*)(expected), (_UF*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line)
#endif

#ifdef UNITY_EXCLUDE_DOUBLE
#define UNITY_TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual, line, message)                  UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Double Precision Disabled")
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE(expected, actual, line, message)                          UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Double Precision Disabled")
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements, line, message)      UNITY_TEST_FAIL((UNITY_LINE_TYPE)line, "Unity Double Precision Disabled")
#else
#define UNITY_TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual, line, message)                  UnityAssertDoublesWithin((_UD)(delta), (_UD)(expected), (_UD)(actual), (message), (UNITY_LINE_TYPE)line)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE(expected, actual, line, message)                          UNITY_TEST_ASSERT_DOUBLE_WITHIN((_UF)(expected) * (_UD)UNITY_DOUBLE_PRECISION, (_UD)expected, (_UD)actual, (UNITY_LINE_TYPE)line, message)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements, line, message)      UnityAssertEqualDoubleArray((_UD*)(expected), (_UD*)(actual), (_UU32)(num_elements), (message), (UNITY_LINE_TYPE)line)
#endif

#endif
