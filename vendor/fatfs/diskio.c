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

#include "tusb.h"

#if TUSB_CFG_HOST_MSC
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "ffconf.h"
#include "diskio.h"
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
// TODO change it to portable init
static DSTATUS disk_state[TUSB_CFG_HOST_DEVICE_MAX];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
static DRESULT wait_for_io_complete(uint8_t usb_addr)
{
  // TODO with RTOS, this should use semaphore instead of blocking
  while ( tuh_msc_is_busy(usb_addr) )
  {
    // TODO should have timeout here
    #if TUSB_CFG_OS != TUSB_OS_NONE
    osal_task_delay(10);
    #endif
  }

  return RES_OK;
}

void diskio_init(void)
{
  memset(disk_state, STA_NOINIT, TUSB_CFG_HOST_DEVICE_MAX);
}

//pdrv Specifies the physical drive number.
DSTATUS disk_initialize ( BYTE pdrv )
{
  disk_state[pdrv] &= (~STA_NOINIT); // clear NOINIT bit
  return disk_state[pdrv];
}

void disk_deinitialize ( BYTE pdrv )
{
  disk_state[pdrv] |= STA_NOINIT; // set NOINIT bit
}

DSTATUS disk_status (BYTE pdrv)
{
  return disk_state[pdrv];
}

//pdrv
//    Specifies the physical drive number -->  == dev_addr-1
//buff
//    Pointer to the byte array to store the read data. The size of buffer must be in sector size * sector count.
//sector
//    Specifies the start sector number in logical block address (LBA).
//count
//    Specifies number of sectors to read. The value can be 1 to 128. Generally, a multiple sector transfer request
//    must not be split into single sector transactions to the device, or you may not get good read performance.
DRESULT disk_read (BYTE pdrv, BYTE*buff, DWORD sector, BYTE count)
{
  uint8_t usb_addr = pdrv+1;

	if ( TUSB_ERROR_NONE != tuh_msc_read10(usb_addr, 0, buff, sector, count) )		return RES_ERROR;

	return wait_for_io_complete(usb_addr);
}


DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, BYTE count)
{
  uint8_t usb_addr = pdrv+1;

	if ( TUSB_ERROR_NONE != tuh_msc_write10(usb_addr, 0, buff, sector, count) )		return RES_ERROR;

	return wait_for_io_complete(usb_addr);
}

/* [IN] Drive number */
/* [IN] Control command code */
/* [I/O] Parameter and data buffer */
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
  (void) buff; (void) pdrv; // compiler warnings

  if (cmd != CTRL_SYNC) return RES_ERROR;
  return RES_OK;
}

static inline uint8_t month2number(char* p_ch) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t month2number(char* p_ch)
{
  char const * const month_str[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  for(uint8_t i=0; i<12; i++)
  {
    if ( strncmp(p_ch, month_str[i], 3) == 0 ) return i+1;
  }

  return 1;
}

static inline uint8_t c2i(char ch) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t c2i(char ch)
{
  return ch - '0';
}

DWORD get_fattime (void)
{
  union {
    struct {
      DWORD second       : 5;
      DWORD minute       : 6;
      DWORD hour         : 5;
      DWORD day_in_month : 5;
      DWORD month        : 4;
      DWORD year         : 7;
    };

    DWORD value;
  } timestamp;

  //------------- Date is compiled date-------------//
  char compile_date[] = __DATE__; // eg. "Sep 26 2013"
  char* p_ch;

  p_ch = strtok (compile_date, " ");
  timestamp.month = month2number(p_ch);

  p_ch = strtok (NULL, " ");
  timestamp.day_in_month = 10*c2i(p_ch[0])+ c2i(p_ch[1]);

  p_ch = strtok (NULL, " ");
  timestamp.year = 1000*c2i(p_ch[0]) + 100*c2i(p_ch[1]) + 10*c2i(p_ch[2]) + c2i(p_ch[3]) - 1980;

  //------------- Time each time this function call --> sec ++ -------------//
  static uint8_t sec = 0;
  static uint8_t min = 0;
  static uint8_t hour = 0;

  if (++sec >= 60)
  {
    sec = 0;
    if (++min >= 60)
    {
      min = 0;
      if (++hour >= 24)
      {
        hour = 0; // assume demo wont call this function more than 24*60*60 times
      }
    }
  }

  timestamp.hour   = hour;
  timestamp.minute = min;
  timestamp.second = sec;

  return timestamp.value;
}

#endif // TUSB_CFG_HOST_MSC
