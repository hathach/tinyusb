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

#include "tusb.h"

#if CFG_TUH_MSC
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "ffconf.h"
#include "diskio.h"
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
// TODO change it to portable init
static DSTATUS disk_state[CFG_TUSB_HOST_DEVICE_MAX];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
static DRESULT wait_for_io_complete(uint8_t usb_addr)
{
  // TODO with RTOS, this should use semaphore instead of blocking
  while ( !tuh_msc_ready(usb_addr) )
  {
    // TODO should have timeout here
    #if CFG_TUSB_OS != OPT_OS_NONE
    osal_task_delay(10);
    #endif
  }

  return RES_OK;
}

void diskio_init(void)
{
  memset(disk_state, STA_NOINIT, CFG_TUSB_HOST_DEVICE_MAX);
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

static inline uint8_t month2number(char* p_ch)
{
  char const * const month_str[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  for(uint8_t i=0; i<12; i++)
  {
    if ( strncmp(p_ch, month_str[i], 3) == 0 ) return i+1;
  }

  return 1;
}

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

#endif // CFG_TUH_MSC
