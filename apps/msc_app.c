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
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "tusb.h"
#include "osal_log.h"
#include "socfpga_usb.h"
#include "hcd_dwc3.h"

#define MSC_BLOCK_SIZE (512)

static volatile int msc_mount_complete = 0;
static scsi_inquiry_resp_t inquiry_resp;
extern SemaphoreHandle_t xDiskIoMutex;
extern SemaphoreHandle_t xDiskIoComplete;
uint32_t dev_block_count;
static uint8_t msc_dev_addr = 0;  /* 0 = no MSC mounted */

static void wait_for_disk_io_fat()
{
    if (xDiskIoComplete != NULL)
    {
        (void) xSemaphoreTake(xDiskIoComplete, portMAX_DELAY);
    }
}

static bool disk_io_complete_fat(uint8_t dev_addr, tuh_msc_complete_data_t const* cb_data)
{
    (void) dev_addr;
    (void) cb_data;

    if (xDiskIoComplete != NULL)
    {
        (void) xSemaphoreGive(xDiskIoComplete);
    }
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

    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    PRINT("A MassStorage device mounted");

    uint8_t const lun = 0;
    msc_dev_addr = dev_addr;
    tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, msc_inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    if (dev_addr == msc_dev_addr) {
        msc_dev_addr = 0;
        msc_mount_complete = 0;
        dev_block_count = 0;
    }

    //for dwc3 controller, deinit sequence needs to be done after a device is unplugged
    tuh_bus_info_t dev_info;
    tuh_bus_info_get(dev_addr, &dev_info);
    if( dev_info.rhport != SOCFPGA_USB2_OTG_PORT )
    {
        hcd_dwc3_device_close(dev_info.rhport);
    }
    PRINT("A MassStorage device is unmounted, address - %d\r\n", dev_addr);
}
bool usb_disk_read(void *buffer, uint32_t lba, uint16_t count)
{
    const uint8_t dev_addr = msc_dev_addr;
    const uint8_t lun = 0U;
    bool read_submitted;

    if( buffer == NULL || count == 0 || lba >= dev_block_count ||
        (uint32_t) count > (dev_block_count - lba) )
    {
        return false;
    }

    const uint32_t block_size = tuh_msc_get_block_size(dev_addr, lun);
    if (block_size == 0)
    {
        return false;
    }

    if (xSemaphoreTake(xDiskIoMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    (void) xSemaphoreTake(xDiskIoComplete, 0);
    // cache invalidate operation is required before read operation for dwc2 controller.
    usb_dcache_clean(buffer, MSC_BLOCK_SIZE*count);
    read_submitted = tuh_msc_read10(dev_addr, lun, buffer, lba, count, disk_io_complete_fat, 0);
    if (!read_submitted)
    {
        (void) xSemaphoreGive(xDiskIoMutex);
        return false;
    }

    wait_for_disk_io_fat();
    (void) xSemaphoreGive(xDiskIoMutex);

    return true;
}

bool usb_disk_write(void *buffer, uint32_t lba, uint16_t count)
{
    const uint8_t dev_addr = msc_dev_addr;
    const uint8_t lun = 0U;
    bool write_submitted;

    if( buffer == NULL || count == 0 || lba >= dev_block_count ||
        (uint32_t) count > (dev_block_count - lba) )
    {
        return false;
    }

    if (xSemaphoreTake(xDiskIoMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    (void) xSemaphoreTake(xDiskIoComplete, 0);
    write_submitted = tuh_msc_write10(dev_addr, lun, buffer, lba, count, disk_io_complete_fat, 0);
    if (!write_submitted)
    {
        (void) xSemaphoreGive(xDiskIoMutex);
        return false;
    }

    wait_for_disk_io_fat();
    (void) xSemaphoreGive(xDiskIoMutex);
    return true;
}

int is_msc_mount_complete(void)
{
    return msc_mount_complete;
}
