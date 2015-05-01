/**************************************************************************/
/*!
    @file     msc_host_app.c
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
#include "msc_host_app.h"
#include "app_os_prio.h"

#if TUSB_CFG_HOST_MSC

#include "msc_cli.h"
#include "ff.h"
#include "diskio.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(msc_host_app_task, 512, MSC_APP_TASK_PRIO);

TUSB_CFG_ATTR_USBRAM static FATFS fatfs[TUSB_CFG_HOST_DEVICE_MAX];

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tuh_msc_mounted_cb(uint8_t dev_addr)
{
  puts("\na MassStorage device is mounted");

  //------------- Disk Information -------------//
  // SCSI VendorID[8] & ProductID[16] from Inquiry Command
  uint8_t const* p_vendor  = tuh_msc_get_vendor_name(dev_addr);
  uint8_t const* p_product = tuh_msc_get_product_name(dev_addr);

  for(uint8_t i=0; i<8; i++) putchar(p_vendor[i]);

  putchar(' ');
  for(uint8_t i=0; i<16; i++) putchar(p_product[i]);
  putchar('\n');

  uint32_t last_lba, block_size;
  tuh_msc_get_capacity(dev_addr, &last_lba, &block_size);
  printf("Disk Size: %d MB\n", (last_lba+1)/ ((1024*1024)/block_size) );
  printf("LBA 0-0x%X  Block Size: %d\n", last_lba, block_size);

  //------------- file system (only 1 LUN support) -------------//
  uint8_t phy_disk = dev_addr-1;
  disk_initialize(phy_disk);

  if ( disk_is_ready(phy_disk) )
  {
    if ( f_mount(phy_disk, &fatfs[phy_disk]) != FR_OK )
    {
      puts("mount failed");
      return;
    }

    puts("---------------------------------------------------------------------");
    puts("- MASSSTORAGE CLASS CLI IS A IMMATURE CODE. DISK-WRITING COMMANDS");
    puts("- SUCH AS cp(COPY), mkdir(MAKE DIRECTORY) ARE POTENTIAL TO DAMAGE");
    puts("- YOUR USB THUMBDRIVE. USING THOSE COMMANDS ARE AT YOUR OWN RISK.");
    puts("- THE AUTHOR HAS NO RESPONSIBILITY WITH YOUR DEVICE NOR ITS DATA");
    puts("---------------------------------------------------------------------");

    f_chdrive(phy_disk); // change to newly mounted drive
    f_chdir("/"); // root as current dir

    cli_init();
  }
}

void tuh_msc_unmounted_cb(uint8_t dev_addr)
{
  puts("\na MassStorage device is unmounted");

  uint8_t phy_disk = dev_addr-1;

  f_mount(phy_disk, NULL); // unmount disk
  disk_deinitialize(phy_disk);

  if ( phy_disk == f_get_current_drive() )
  { // active drive is unplugged --> change to other drive
    for(uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX; i++)
    {
      if ( disk_is_ready(i) )
      {
        f_chdrive(i);
        cli_init(); // refractor, rename
      }
    }
  }
}

// invoked ISR context
void tuh_msc_isr(uint8_t dev_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  (void) dev_addr;
  (void) event;
  (void) xferred_bytes;
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void msc_host_app_init(void)
{
  ASSERT( TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(msc_host_app_task) ), VOID_RETURN );
  diskio_init();
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( msc_host_app_task, p_task_para)
{
  (void) p_task_para;

  OSAL_TASK_LOOP_BEGIN

  bool is_any_disk_mounted; 
  
  osal_task_delay(10);

  is_any_disk_mounted = false;
  
  for(uint8_t phy_disk=0; phy_disk < TUSB_CFG_HOST_DEVICE_MAX; phy_disk++)
  {
    if ( disk_is_ready(phy_disk) )
    {
      is_any_disk_mounted = true;
      break;
    }
  }

  if ( is_any_disk_mounted )
  {
    int ch = getchar();
    if ( ch > 0 )
    {
      cli_poll( (char) ch);
    }
  }

  OSAL_TASK_LOOP_END
}

#endif
