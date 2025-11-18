/*
 * Copyright (c) 2024, Intel Corporation.
 *
 * SPDX-License-Identifier: MIT
 *
 * HAL implementation for USB3
 */
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include "hcd_dwc3.h"
#include "tusb_config.h"
#include "osal_log.h"

/* tinyusb specific queue initialization method */
OSAL_QUEUE_DEF(usbh_int_set, xhci_event_params, XHCI_QUEUE_SZ, xcc_event_t);

static QueueHandle_t xhci_queue;

Usb3_Handle_t* alloc_usb_port( void )
{
    int ret;
    Usb3_Handle_t *xHandle = (Usb3_Handle_t*) pvPortAlignedAlloc(64,
            sizeof(Usb3_Handle_t));
    if ( xHandle == NULL )
    {
        ERROR("Cannot allocate memory");
        return NULL;
    }

    ret = is_ptr_mem_aligned((uint64_t) xHandle, 64);
    if ( ret != 0 )
    {
        ERROR("Cannot allocate memory");
        return NULL;
    }

    return xHandle;
}

void dealloc_usb_port( Usb3_Handle_t *handle )
{
    deallocate_xhci_context(&handle->xhci_priv);
}

int init_hcd_params( void )
{
    xhci_queue = osal_queue_create(&xhci_event_params);
    if ( xhci_queue == NULL )
    {
        ERROR("Error in creating queue !!!");
        return -ENOMEM;
    }
    return 0;
}

int wait_for_command_completion_event( struct xhci_data *xhci_ptr, int type )
{
    xcc_event_t event;
    while ( true )
    {
        if ( osal_queue_receive(xhci_queue, &event, UINT32_MAX) == pdTRUE )
        {
            DEBUG("CC event values are");
            DEBUG("pointer   : %lx", event.cmd_trb_ptr);
            DEBUG("Status field  : %x", event.status_field);
            DEBUG("Control fields : %x", event.control_field);
            break;
        }
    }

    if ( ((event.status_field & 0xff000000U) >> 24) != 1U )
    {
        return -1;
    }

    switch (type)
    {
    case ENABLE_SLOT_CMD:
        /* Get slot id from the CC event trb */
        update_device_slot_id(&xhci_ptr->dev_data,
                (uint8_t) ((event.control_field & 0xff000000U) >> 24));
        break;

    default: /*do nothing*/
        break;
    }

    xhci_ptr->xcr_ring->xcr_dequeue_ptr = (xhci_trb_t*) (event.cmd_trb_ptr);

    return 0;
}

void xhci_command_event_complete( xcc_event_t event )
{
    (void) osal_queue_send(xhci_queue, &event);
}

void reset_usb_port( uint8_t rhport )
{
    /* USB3 HS port only needs reset */
    if ( rhport == (uint32_t) SOCFPGA_USB3_HS_PORT )
    {
        reset_xhci_port(rhport);
    }
}

bool usb_port_reset_end( uint8_t rhport )
{
    if ( rhport == (uint32_t) SOCFPGA_USB3_HS_PORT )
    {
        return is_xhci_port_reset_end(rhport);
    }

    return true;
}
