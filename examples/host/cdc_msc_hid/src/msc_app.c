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
 */

#include "tusb.h"

#if CFG_TUH_MSC

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+


//------------- IMPLEMENTATION -------------//
void tuh_msc_mounted_cb(uint8_t dev_addr)
{
  puts("\na MassStorage device is mounted");

//  //------------- Disk Information -------------//
//  // SCSI VendorID[8] & ProductID[16] from Inquiry Command
//  uint8_t const* p_vendor  = tuh_msc_get_vendor_name(dev_addr);
//  uint8_t const* p_product = tuh_msc_get_product_name(dev_addr);
//
//  for(uint8_t i=0; i<8; i++) putchar(p_vendor[i]);
//
//  putchar(' ');
//  for(uint8_t i=0; i<16; i++) putchar(p_product[i]);
//  putchar('\n');
//
//  uint32_t last_lba, block_size;
//  tuh_msc_get_capacity(dev_addr, &last_lba, &block_size);
//  printf("Disk Size: %d MB\n", (last_lba+1)/ ((1024*1024)/block_size) );
//  printf("LBA 0-0x%X  Block Size: %d\n", last_lba, block_size);
//
//  //------------- file system (only 1 LUN support) -------------//
//  uint8_t phy_disk = dev_addr-1;
//  disk_initialize(phy_disk);
//
//  if ( disk_is_ready(phy_disk) )
//  {
//    if ( f_mount(phy_disk, &fatfs[phy_disk]) != FR_OK )
//    {
//      puts("mount failed");
//      return;
//    }
//
//    puts("---------------------------------------------------------------------");
//    puts("- MASSSTORAGE CLASS CLI IS A IMMATURE CODE. DISK-WRITING COMMANDS");
//    puts("- SUCH AS cp(COPY), mkdir(MAKE DIRECTORY) ARE POTENTIAL TO DAMAGE");
//    puts("- YOUR USB THUMBDRIVE. USING THOSE COMMANDS ARE AT YOUR OWN RISK.");
//    puts("- THE AUTHOR HAS NO RESPONSIBILITY WITH YOUR DEVICE NOR ITS DATA");
//    puts("---------------------------------------------------------------------");
//
//    f_chdrive(phy_disk); // change to newly mounted drive
//    f_chdir("/"); // root as current dir
//
//    cli_init();
//  }
}

void tuh_msc_unmounted_cb(uint8_t dev_addr)
{
  puts("\na MassStorage device is unmounted");

//  uint8_t phy_disk = dev_addr-1;
//
//  f_mount(phy_disk, NULL); // unmount disk
//  disk_deinitialize(phy_disk);
//
//  if ( phy_disk == f_get_current_drive() )
//  { // active drive is unplugged --> change to other drive
//    for(uint8_t i=0; i<CFG_TUSB_HOST_DEVICE_MAX; i++)
//    {
//      if ( disk_is_ready(i) )
//      {
//        f_chdrive(i);
//        cli_init(); // refractor, rename
//      }
//    }
//  }
}

// invoked ISR context
void tuh_msc_isr(uint8_t dev_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  (void) dev_addr;
  (void) event;
  (void) xferred_bytes;
}

#endif
