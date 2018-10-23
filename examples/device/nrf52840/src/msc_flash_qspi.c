/**************************************************************************/
/*!
 @file    msc_flash_qspi.c
 @author  hathach (tinyusb.org)

 @section LICENSE

 Software License Agreement (BSD License)

 Copyright (c) 2018, hathach (tinyusb.org)
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

#include "msc_app.h"

#if CFG_TUD_MSC && defined (BOARD_MSC_FLASH_QSPI)

void flash_read (void *dst, uint32_t src, int len);
void flash_write (uint32_t dst, const void *src, int len);
void flash_flush (void);

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

//------------- IMPLEMENTATION -------------//
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  uint32_t addr = lba * CFG_TUD_MSC_BLOCK_SZ + offset;

  flash_read(buffer, addr, bufsize);
  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  uint32_t addr = lba * CFG_TUD_MSC_BLOCK_SZ + offset;

  flash_write(addr, buffer, bufsize);

  return bufsize;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void tud_msc_write10_complete_cb (uint8_t lun)
{
  (void) lun;

  // flush pending cache when write10 is complete
  flash_flush();
}

//--------------------------------------------------------------------+
// Flash caching
//--------------------------------------------------------------------+
#define FLASH_PAGE_SIZE    4096

#define NO_CACHE 0xffffffff

static uint32_t _fl_addr = NO_CACHE;
static uint8_t _fl_buf[FLASH_PAGE_SIZE] __attribute__((aligned(4)));

void flash_flush (void)
{
  if ( _fl_addr == NO_CACHE ) return;

  TU_ASSERT(NRFX_SUCCESS == nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, _fl_addr),);
  TU_ASSERT(NRFX_SUCCESS == nrfx_qspi_write(_fl_buf, FLASH_PAGE_SIZE, _fl_addr),);

  _fl_addr = NO_CACHE;
}

void flash_write (uint32_t dst, const void *src, int len)
{
  uint32_t newAddr = dst & ~(FLASH_PAGE_SIZE - 1);

  if ( newAddr != _fl_addr )
  {
    flash_flush();
    _fl_addr = newAddr;

    flash_read(_fl_buf, newAddr, FLASH_PAGE_SIZE);
  }

  memcpy(_fl_buf + (dst & (FLASH_PAGE_SIZE - 1)), src, len);
}

void flash_read (void *dst, uint32_t src, int len)
{
  TU_ASSERT(NRFX_SUCCESS == nrfx_qspi_read(dst, len, src),);
}


#endif
