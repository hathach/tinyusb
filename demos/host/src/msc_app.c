/**************************************************************************/
/*!
    @file     msc_app.c
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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "mouse_app.h"

#if TUSB_CFG_OS != TUSB_OS_NONE
#include "app_os_prio.h"
#endif

#if TUSB_CFG_HOST_MSC

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
void tusbh_msc_mounted_cb(uint8_t dev_addr)
{
  printf("an msc device is mounted\n");

  // SCSI VendorID[8] & ProductID[16] from Inquiry Command
  uint8_t const* p_vendor  = tusbh_msc_get_vendor_name(dev_addr);
  uint8_t const* p_product = tusbh_msc_get_product_name(dev_addr);

  printf("Vendor  Name: ");
  for(uint8_t i=0; i<8; i++) putchar(p_vendor[i]);
  putchar('\n');

  printf("Product Name: ");
  for(uint8_t i=0; i<16; i++) putchar(p_product[i]);
  putchar('\n');

  uint32_t last_lba, block_size;
  tusbh_msc_get_capacity(dev_addr, &last_lba, &block_size);
  printf("Disk Size: %d MB\n", (last_lba+1)/ ((1024*1024)/block_size) );
  printf("LBA 0-0x%X, Block Size: %d\n", last_lba, block_size);
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void msc_app_init(void)
{

}

//------------- main task -------------//
OSAL_TASK_FUNCTION( msc_app_task ) (void* p_task_para)
{

}

#else
// dummy implementation to remove #ifdef in main.c
void msc_app_init(void) { }
OSAL_TASK_FUNCTION( msc_app_task ) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN
  OSAL_TASK_LOOP_END
}


#endif
