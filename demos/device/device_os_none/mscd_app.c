/**************************************************************************/
/*!
    @file     mscd_app.c
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

#include "mscd_app.h"

#if TUSB_CFG_DEVICE_MSC
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum
{
  DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

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

static scsi_inquiry_data_t mscd_inquiry_data TUSB_CFG_ATTR_USBRAM =
{
    .is_removable         = 1,
    .version              = 2,
    .response_data_format = 2,
    .vendor_id            = "tinyusb",
    .product_id           = "MSC Example",
    .product_revision     = "0.01"
};

static scsi_read_capacity10_data_t mscd_read_capacity10_data TUSB_CFG_ATTR_USBRAM =
{
    .last_lba   = __le2be(DISK_BLOCK_NUM-1), // read capacity
    .block_size = __le2be(DISK_BLOCK_SIZE)
};

static scsi_sense_fixed_data_t mscd_sense_data TUSB_CFG_ATTR_USBRAM =
{
    .response_code        = 0x70,
    .sense_key            = 0, // no errors
    .additional_sense_len = sizeof(scsi_sense_fixed_data_t) - 8
};

static scsi_read_format_capacity_data_t mscd_format_capacity_data TUSB_CFG_ATTR_USBRAM =
{
    .list_length     = 8,
    .block_num       = __le2be(DISK_BLOCK_NUM), // write capacity
    .descriptor_type = 2, // TODO formatted media, refractor to const
    .block_size_u16  = __h2be_16(DISK_BLOCK_SIZE)
};

static scsi_mode_parameters_t msc_dev_mode_para TUSB_CFG_ATTR_USBRAM =
{
    .mode_data_length        = 3,
    .medium_type             = 0,
    .device_specific_para    = 0,
    .block_descriptor_length = 0
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
uint8_t mscd_app_ramdisk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
static uint16_t read10(uint8_t coreid, uint8_t lun, scsi_read10_t* p_read10, void** pp_buffer)
{
  uint8_t block_count = __be2h_16(p_read10->block_count);

  (*pp_buffer) = mscd_app_ramdisk[ __be2le(p_read10->lba)];

  return block_count*DISK_BLOCK_SIZE;
}

static uint16_t write10(uint8_t coreid, uint8_t lun, scsi_read10_t* p_read10, void** pp_buffer)
{
  uint8_t block_count = __be2h_16(p_read10->block_count);
  (*pp_buffer) = mscd_app_ramdisk[ __be2le(p_read10->lba)];

  return block_count*DISK_BLOCK_SIZE;
}

msc_csw_status_t tusbd_msc_scsi_received_isr (uint8_t coreid, uint8_t lun, uint8_t scsi_cmd[16], void ** pp_buffer, uint16_t* p_length)
{
  switch (scsi_cmd[0])
  {
    case SCSI_CMD_INQUIRY:
      (*pp_buffer) = &mscd_inquiry_data;
      (*p_length)  = sizeof(scsi_inquiry_data_t);
    break;

    case SCSI_CMD_READ_CAPACITY_10:
      (*pp_buffer) = &mscd_read_capacity10_data;
      (*p_length)  = sizeof(scsi_read_capacity10_data_t);
    break;

    case SCSI_CMD_REQUEST_SENSE:
      (*pp_buffer) = &mscd_sense_data;
      (*p_length)  = sizeof(scsi_sense_fixed_data_t);
    break;

    case SCSI_CMD_READ_FORMAT_CAPACITY:
      (*pp_buffer) = &mscd_format_capacity_data;
      (*p_length)  = sizeof(scsi_read_format_capacity_data_t);
    break;

    case SCSI_CMD_MODE_SENSE_6:
      (*pp_buffer) = &msc_dev_mode_para;
      (*p_length)  = sizeof(msc_dev_mode_para);
    break;

    case SCSI_CMD_TEST_UNIT_READY:
      (*pp_buffer) = NULL;
      (*p_length) = 0;
    break;

    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      (*pp_buffer) = NULL;
      (*p_length) = 0;
    break;

    case SCSI_CMD_READ_10:
      (*p_length)  = read10(coreid, lun, (scsi_read10_t*) scsi_cmd, pp_buffer);
    break;

    case SCSI_CMD_WRITE_10:
      (*p_length)  = write10(coreid, lun, (scsi_read10_t*) scsi_cmd, pp_buffer);
    break;

    default: return MSC_CSW_STATUS_FAILED;
  }

  return MSC_CSW_STATUS_PASSED;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void fat12_fs_init(uint8_t mscd_app_ramdisk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE]);

void msc_dev_app_init (void)
{
  fat12_fs_init(mscd_app_ramdisk);
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
void fat12_fs_init(uint8_t mscd_app_ramdisk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE])
{
  uint8_t const readme_contents[] =
"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at https://github.com/hathach/tinyusb";

  //------------- Boot Sector -------------//
  fat12_boot_sector_t* p_boot_fat = (fat12_boot_sector_t* ) mscd_app_ramdisk[0];
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
  memcpy(mscd_app_ramdisk[1], "\xF8\xFF\xFF\xFF\x0F", 5);

  //------------- Root Directory -------------//
  fat_directory_t* p_entry = (fat_directory_t*) mscd_app_ramdisk[2];

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
  memcpy(mscd_app_ramdisk[3], readme_contents, sizeof(readme_contents)-1);

}

#endif
