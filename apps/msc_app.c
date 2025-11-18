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

#include <ctype.h>
#include "tusb.h"
#include "osal_log.h"

void fatfs_test( void);

static volatile int msc_mount_complete = 0;
static scsi_inquiry_resp_t inquiry_resp;
static volatile bool status_flag_cb = true;
uint32_t dev_block_count;

static void wait_for_disk_io_fat()
{
    while(status_flag_cb == true)
    {
        tuh_task();
    }
}

static bool disk_io_complete_fat(uint8_t dev_addr, tuh_msc_complete_data_t const* cb_data)
{
    (void) dev_addr;
    (void) cb_data;

    status_flag_cb = false;
    return true;
}

bool msc_inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data)
{
    msc_cbw_t const* cbw = cb_data->cbw;
    msc_csw_t const* csw = cb_data->csw;

    if (csw->status != 0)
    {
        printf("Inquiry failed\r\n");
        return false;
    }

    /* Print out Vendor ID, Product ID and Rev */
    PRINT("%.8s %.16s rev %.4s", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

    /* Get device capacity */
    uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
    uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

    PRINT("Disk Size: %" PRIu32 " GB", block_count / ((1024*1024*1024)/block_size));

    /* MSC mount process completed */
    dev_block_count = block_count;
    msc_mount_complete = 1;
#if 0
	r_buffer = (uint8_t *)pvPortMallocCoherent(512);
	w_buffer = (uint8_t *)pvPortMallocCoherent(512);

	memset(r_buffer, '.', 512);
	memset(w_buffer, '?', 512);

    status_flag_cb = true;
    tuh_msc_write10(dev_addr, 0, w_buffer, 10000, 1, disk_io_complete_fat, 0);
    wait_for_disk_io_fat();

    status_flag_cb = true;
    tuh_msc_read10(dev_addr, 0, r_buffer, 10000, 1, disk_io_complete_fat, 0);
    wait_for_disk_io_fat();
#endif
    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    PRINT("A MassStorage device mounted");

    uint8_t const lun = 0;
    tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, msc_inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    msc_mount_complete = 0;
    PRINT("A MassStorage device is unmounted, address - %d\r\n", dev_addr);
}
bool usb_disk_read(void *buffer, uint32_t lba, uint16_t count)
{
    const uint8_t dev_addr = 1U;
    const uint8_t lun = 0U;
    if( buffer == NULL || lba >= dev_block_count  || count == 0 )
    {
        return false;
    }
	uint8_t *ptr = pvPortMallocCoherent(512*count);
	
    status_flag_cb = true;
    tuh_msc_read10(dev_addr, lun, ptr, lba, count, disk_io_complete_fat, 0);
    wait_for_disk_io_fat();
	memcpy(buffer, ptr, 512*count);

    return true;
}

bool usb_disk_write(void *buffer, uint32_t lba, uint16_t count)
{
    const uint8_t dev_addr = 1U;
    const uint8_t lun = 0U;

    if( buffer == NULL || lba >= dev_block_count  || count == 0 )
    {
        return false;
    }

    status_flag_cb = true;
    tuh_msc_write10(dev_addr, lun, buffer, lba, count, disk_io_complete_fat, 0);
    wait_for_disk_io_fat();
    return true;
}

int is_msc_mount_complete(void)
{
    return msc_mount_complete;
}
