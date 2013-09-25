/**************************************************************************/
/*!
    @file     diskio.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "boards/board.h"
#include "tusb.h"

#include "diskio.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
static volatile DSTATUS disk_state = STA_NOINIT;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

//pdrv Specifies the physical drive number.
DSTATUS disk_initialize ( BYTE pdrv )
{
  return disk_state;
}

DSTATUS disk_status (BYTE pdrv)
{
  return disk_state;
}

//pdrv
//    Specifies the physical drive number.
//buff
//    Pointer to the byte array to store the read data. The size of buffer must be in sector size * sector count.
//sector
//    Specifies the start sector number in logical block address (LBA).
//count
//    Specifies number of sectors to read. The value can be 1 to 128. Generally, a multiple sector transfer request
//    must not be split into single sector transactions to the device, or you may not get good read performance.
DRESULT disk_read (BYTE pdrv, BYTE*buff, DWORD sector, BYTE count)
{

}


DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, BYTE count)
{

}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{

}

//DWORD get_fattime (void)
//{
//  union {
//    struct {
//      DWORD second       : 5;
//      DWORD minute       : 6;
//      DWORD hour         : 5;
//      DWORD day_in_month : 5;
//      DWORD month        : 4;
//      DWORD year         : 7;
//    };
//
//    DWORD value
//  } timestamp =
//  {
//      .year = (2013-1980),
//      .month = 10,
//      .day_in_month = 21,
//  };
//}
