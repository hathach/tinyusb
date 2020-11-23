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
static scsi_inquiry_resp_t inquiry_resp;
static scsi_read_capacity10_resp_t capacity_resp;

uint32_t block_size;
uint32_t block_count;

bool capacity_complete_cb(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
  (void) dev_addr;
  (void) cbw;

  if (csw->status != 0)
  {
    printf("Read Capacity (10) failed\r\n");
    return false;
  }

  // Capacity response field: Block size and Last LBA are both Big-Endian
  block_count = tu_ntohl(capacity_resp.last_lba) + 1;
  block_size = tu_ntohl(capacity_resp.block_size);

  printf("Disk Size: %lu MB\r\n", block_count / ((1024*1024)/block_size));
  printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

  return true;
}

bool inquiry_complete_cb(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
  if (csw->status != 0)
  {
    printf("Inquiry failed\r\n");
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

  // Read capacity of device
  tuh_msc_read_capacity(dev_addr, cbw->lun, &capacity_resp, capacity_complete_cb);

  return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mounted_cb(uint8_t dev_addr)
{
  printf("A MassStorage device is mounted\r\n");

  block_size = block_count = 0;

  uint8_t const lun = 0;
  tuh_msc_scsi_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb);
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
  (void) dev_addr;
  printf("A MassStorage device is unmounted\r\n");

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

//void tuh_msc_scsi_complete_cb(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
//{
//  (void) dev_addr;
//  (void) cbw;
//  (void) csw;
//}

#endif
