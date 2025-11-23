#include <errno.h>
#include "FreeRTOS.h"
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

int usb3_wait_to_mount(int timeout)
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

void usb3_task(void *arg)
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
        ERROR("Error in initialising usb3 port");
    }

    else
    {    
        /*initialize host stack for usb3 HS port*/
        if (!tusb_init(USB3_HS_PORT, &host_init))
        {
            ERROR("Error in initialising usb3 port");
        }
        else
        {
            PRINT("USB3.1 port initialized successfully");
        }
    }
    while (1)
    {
        tuh_task();
        osal_task_delay(100);
    }
}

