#include <errno.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tusb.h"
#include "tusb_config.h"
#include "usb_main.h"
#include "msc_app.h"
#include "osal/osal.h"
#include "osal_log.h"

#define USB_OTG_PORT    (0)
#define USB3_HS_PORT    (1)
#define USB3_SS_PORT    (2)

SemaphoreHandle_t xDiskIoMutex = NULL;
SemaphoreHandle_t xDiskIoComplete = NULL;

int usb_wait_to_mount(int timeout)
{
    while (timeout >= 0)
    {
        if (is_msc_mount_complete() == 1)
        {
            break;
        }
        osal_task_delay(1000);
        --timeout;
    }
    if (timeout < 0)
    {
        return -ETIMEDOUT;
    }

    return 0;
}

static bool init_disk_io_sync(void)
{
    if (xDiskIoMutex == NULL)
    {
        PRINT("Creating disk IO mutex");
        xDiskIoMutex = xSemaphoreCreateMutex();
    }

    if (xDiskIoComplete == NULL)
    {
        PRINT("Creating disk IO complete semaphore");
        xDiskIoComplete = xSemaphoreCreateBinary();
    }

    return (xDiskIoMutex != NULL) && (xDiskIoComplete != NULL);
}


void usb_task(void *arg)
{
    (void)arg;

    tusb_rhport_init_t host_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO
    };

    /*initialize host stack for usb otg HS port*/
    if (!tusb_init(USB_OTG_PORT, &host_init))
    {
        ERROR("Error in initialising usb otg port");
        /*suspend the task*/
    }
    else
    {
        PRINT("USB OTG port initialized successfully");
    }

     /*initialize host stack for usb3 SS port*/
    if (!tusb_init(USB3_SS_PORT, &host_init))
    {
        ERROR("Error in initialising usb3 SS port");
    }

    /*initialize host stack for usb3 HS port*/
    if (!tusb_init(USB3_HS_PORT, &host_init))
    {
        ERROR("Error in initialising usb3 HS port");
    }
    else
    {
        PRINT("USB3.1 port initialized successfully");
    }
    
    //create mutex + binary semaphore only once
    if(!init_disk_io_sync())
    {
        ERROR("Failed to initialize disk IO synchronization primitives");
        vTaskDelete(NULL);   
        return;
    }

    while (1)
    {
        tuh_task();
        osal_task_delay(100);
    }
}



