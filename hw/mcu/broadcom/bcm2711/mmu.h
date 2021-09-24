#pragma once


// From: https://github.com/s-matyukevich/raspberry-pi-os/blob/master/docs/lesson06/rpi-os.md
/*
 * Memory region attributes:
 *
 *   n = AttrIndx[2:0]
 *            n    MAIR
 *   DEVICE_nGnRnE    000    00000000
 *   NORMAL_NC        001    01000100
 */
#define MT_DEVICE_nGnRnE         0x0
#define MT_NORMAL_NC            0x1
#define MT_DEVICE_nGnRnE_FLAGS        0x00
#define MT_NORMAL_NC_FLAGS          0xff
#define MAIR_VALUE            (MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NC))


#define TCR_T0SZ            (64 - 36) 
#define TCR_PS              (0x01 << 16) // 36-bit physical address
#define TCR_TG0_4K          (0 << 14)
#define TCR_SH0_OUTER_SHAREABLE (0x2 << 12)
#define TCR_VALUE           (TCR_T0SZ | TCR_PS | TCR_TG0_4K | TCR_SH0_OUTER_SHAREABLE)

#define ENTRY_TYPE_TABLE_DESCRIPTOR 0x11
#define ENTRY_TYPE_BLOCK_ENTRY 0x01
#define ENTRY_TYPE_TABLE_ENTRY 0x11
#define ENTRY_TYPE_INVALID 0x00

#define MM_DESCRIPTOR_VALID (0x1)

#define MM_DESCRIPTOR_BLOCK (0x0 << 1)
#define MM_DESCRIPTOR_TABLE (0x1 << 1)

// Block attributes
#define MM_DESCRIPTOR_EXECUTE_NEVER (0x1ull << 54)
#define MM_DESCRIPTOR_CONTIGUOUS    (0x1ull << 52)
#define MM_DESCRIPTOR_ACCESS_FLAG   (0x1ull << 10)

#define MM_DESCRIPTOR_MAIR_INDEX(index) (index << 2)

#define MM_TTBR_CNP (0x1)

void setup_mmu_flat_map(void);
