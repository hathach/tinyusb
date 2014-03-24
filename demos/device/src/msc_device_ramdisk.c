/**************************************************************************/
/*!
    @file     msc_device_ramdisk.c
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

#include "msc_device_app.h"

#if TUSB_CFG_DEVICE_MSC && defined (MSCD_APP_RAMDISK)

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
TUSB_CFG_ATTR_USBRAM
uint8_t msc_device_ramdisk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] =
{
  //------------- Boot Sector -------------//
  // byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
  // sector_per_cluster = 1; reserved_sectors = 1;
  // fat_num            = 1; fat12_root_entry_num = 16;
  // sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
  // drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
  // filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "tinyusb msc";
  [0] =
  {
      0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00,
      0x01, 0x10, 0x00, 0x10, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 0x74, 0x69, 0x6E, 0x79, 0x75,
      0x73, 0x62, 0x20, 0x6D, 0x73, 0x63, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,
      [510] = 0x55, [511] = 0xAA // FAT magic code
  },

  //------------- FAT12 Table -------------//
  [1] =
  {
      0xF8, 0xFF, 0xFF, 0xFF, 0x0F // // first 2 entries must be F8FF, third entry is cluster end of readme file
  },

  //------------- Root Directory -------------//
  [2] =
  {
      // first entry is volume label
      0x54, 0x49, 0x4E, 0x59, 0x55, 0x53, 0x42, 0x20, 0x4D, 0x53, 0x43, 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // second entry is readme file
      'R' , 'E' , 'A' , 'D' , 'M' , 'E' , ' ' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0xC6, 0x52, 0x6D,
      0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
      sizeof(README_CONTENTS)-1, 0x00, 0x00, 0x00 // readme's filesize (4 Bytes)
  },

  //------------- Readme Content -------------//
  [3] = README_CONTENTS
};

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
uint16_t tusbd_msc_read10_cb (uint8_t coreid, uint8_t lun, void** pp_buffer, uint32_t lba, uint16_t block_count)
{
  (*pp_buffer) = msc_device_ramdisk[lba];

  return min16_of(block_count, DISK_BLOCK_NUM);
}
uint16_t tusbd_msc_write10_cb(uint8_t coreid, uint8_t lun, void** pp_buffer, uint32_t lba, uint16_t block_count)
{
  (*pp_buffer) = msc_device_ramdisk[lba];

  return min16_of(block_count, DISK_BLOCK_NUM);
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+

#if 0 // no need to use fat12 helper
typedef ATTR_PACKED_STRUCT(struct) {
  //------------- common -------------//
  uint8_t  jump_code[3]            ; ///< Assembly instruction to jump to boot code.
  uint8_t  oem_name[8]             ; ///< OEM Name in ASCII.
  uint16_t byte_per_sector         ; ///< Bytes per sector. Allowed values include 512, 1024, 2048, and 4096.
  uint8_t  sector_per_cluster      ; ///< Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller.
  uint16_t reserved_sectors        ; ///< Size in sectors of the reserved area.
  uint8_t  fat_num                 ; ///< Number of FATs. Typically two for redundancy, but according to Microsoft it can be one for some small storage devices.
  uint16_t fat12_root_entry_num    ; ///< Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32 and typically 512 for FAT16.
  uint16_t fat12_sector_num_16     ; ///< 16-bit number of sectors in file system. If the number of sectors is larger than can be represented in this 2-byte value, a 4-byte value exists later in the data structure and this should be 0.
  uint8_t  media_type              ; ///< 0xf8 should be used for fixed disks and 0xf0 for removable.
  uint16_t sector_per_fat          ; ///< 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0.
  uint16_t sector_per_track        ; ///< Sectors per track of storage device.
  uint16_t head_num                ; ///< Number of heads in storage device.
  uint32_t hidden_sectors          ; ///< Number of sectors before the start of partition.
  uint32_t sector_num_32           ; ///< 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0.

  //------------- FAT32 -------------//
  uint8_t  drive_number            ; ///< Physical drive number (0x00 for (first) removable media, 0x80 for (first) fixed disk
  uint8_t  reserved                ;
  uint8_t  extended_boot_signature ; ///< should be 0x29
  uint32_t volume_serial_number    ; ///< Volume serial number, which some versions of Windows will calculate based on the creation date and time.
  uint8_t  volume_label[11]        ;
  uint8_t  filesystem_type[8]      ; ///< File system type label in ASCII, padded with blank (0x20). Standard values include "FAT," "FAT12," and "FAT16," but nothing is required.
  uint8_t  reserved2[448]          ;
  uint16_t fat_signature           ; ///< Signature value (0xAA55).
}fat12_boot_sector_t;

STATIC_ASSERT(sizeof(fat12_boot_sector_t) == 512, "size is not correct");

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t name[11];

  ATTR_PACKED_STRUCT(struct){
    uint8_t readonly       : 1;
    uint8_t hidden         : 1;
    uint8_t system         : 1;
    uint8_t volume_label   : 1;
    uint8_t directory      : 1;
    uint8_t archive        : 1;
  } attr; // Long File Name = 0x0f

  uint8_t reserved;
  uint8_t created_time_tenths_of_seconds;
  uint16_t created_time;
  uint16_t created_date;
  uint16_t accessed_date;
  uint16_t cluster_high;
  uint16_t written_time;
  uint16_t written_date;
  uint16_t cluster_low;
  uint32_t file_size;
}fat_directory_t;

STATIC_ASSERT(sizeof(fat_directory_t) == 32, "size is not correct");

void fat12_fs_init(uint8_t msc_device_ramdisk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE])
{
  uint8_t const readme_contents[] =
"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at https://github.com/hathach/tinyusb";

  //------------- Boot Sector -------------//
  fat12_boot_sector_t* p_boot_fat = (fat12_boot_sector_t* ) msc_device_ramdisk[0];
  memclr_(p_boot_fat, sizeof(fat12_boot_sector_t));

  memcpy(p_boot_fat->jump_code, "\xEB\x3C\x90", 3);
  memcpy(p_boot_fat->oem_name, "MSDOS5.0", 8);
  p_boot_fat->byte_per_sector         = DISK_BLOCK_SIZE;
  p_boot_fat->sector_per_cluster      = 1;
  p_boot_fat->reserved_sectors        = 1;
  p_boot_fat->fat_num                 = 1;
  p_boot_fat->fat12_root_entry_num    = 16;
  p_boot_fat->fat12_sector_num_16     = DISK_BLOCK_NUM;
  p_boot_fat->media_type              = 0xf8; // fixed disk
  p_boot_fat->sector_per_fat          = 1;
  p_boot_fat->sector_per_track        = 1;
  p_boot_fat->head_num                = 1;
  p_boot_fat->hidden_sectors          = 0;

  p_boot_fat->drive_number            = 0x80;
  p_boot_fat->extended_boot_signature = 0x29;
  p_boot_fat->volume_serial_number    = 0x1234;
  memcpy(p_boot_fat->volume_label   , "tinyusb msc", 11);
  memcpy(p_boot_fat->filesystem_type,  "FAT12   ", 8);
  p_boot_fat->fat_signature           = 0xAA55;

  //------------- FAT12 Table (first 2 entries are F8FF, third entry is cluster end of readme file-------------//
  memcpy(msc_device_ramdisk[1], "\xF8\xFF\xFF\xFF\x0F", 5);

  //------------- Root Directory -------------//
  fat_directory_t* p_entry = (fat_directory_t*) msc_device_ramdisk[2];

  // first entry is volume label
  (*p_entry) = (fat_directory_t)
  {
    .name = "TINYUSB MSC",
    .attr.volume_label = 1,
  };

  p_entry += 1; // advance to second entry, second entry is readme file
  (*p_entry) = (fat_directory_t)
  {
    .name = "README  TXT",

    .created_time = 0x6D52,
    .written_time = 0x6D52,

    .created_date = 0x4365,
    .accessed_date = 0x4365,
    .written_date = 0x4365,

    .cluster_high = 0,
    .cluster_low = 2,
    .file_size = sizeof(readme_contents)-1 // exculde NULL
  }; // first entry is volume label

  //------------- Readme Content -------------//
  memcpy(msc_device_ramdisk[3], readme_contents, sizeof(readme_contents)-1);

}
#endif

#endif
