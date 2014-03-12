/****************************************************************************
 *   $Id:: type.h 6172 2011-01-13 18:22:51Z usb00423                        $
 *   Project: NXP LPC13Uxx software example
 *
 *   Description:
 *     This file contains different type definition.
 *
 ****************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
****************************************************************************/
#ifndef __TYPE_H__
#define __TYPE_H__

#if defined (__GNUC__)
#include <stdint.h>

#else
/* exact-width signed integer types */
typedef   signed          char int8_t;
typedef   signed short     int int16_t;
typedef   signed           int int32_t;
//typedef   signed       __int64 int64_t;

     /* exact-width unsigned integer types */
typedef unsigned          char uint8_t;
typedef unsigned short     int uint16_t;
typedef unsigned           int uint32_t;
//typedef unsigned       __int64 uint64_t;
#endif

#ifndef NULL
#define NULL    ((void *)0)
#endif

#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (1)
#endif

#endif  /* __TYPE_H__ */
