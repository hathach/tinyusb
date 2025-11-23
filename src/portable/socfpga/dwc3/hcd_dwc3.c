/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji Kitayama
 * Portions copyrighted (c) 2021 Roland Winistoerfer
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
 * This file is part of the TinyUSB stack.
 */

#include <math.h>
#include "tusb.h"
#include "socfpga_rst_mngr.h"
#include "host/hcd.h"

#include "dwc3.h"
#include "xhci_commands.h"
#include "xhci_endpoints.h"
#include "xhci_interrupts.h"
#include "xhci_doorbell.h"
#include "hcd_dwc3.h"
#include "socfpga_usb.h"
#include "tusb_private.h"
#include "osal_log.h"

#define USB3_HS_PORT (1)
#define USB3_SS_PORT (2)

static void hcd_xhci_set_configuration();
static void hcd_dwc3_update_device_address( uint8_t daddr );

Usb3_Handle_t *Usb3handle;
uint8_t device_addr = 0;

static struct xhci_int_desc usb3_int_desc;
static tusb_control_request_t ctrl_req;
static int usb_set_config = 0;

bool hcd_dwc3_init( uint8_t rhport, const tusb_rhport_init_t *rh_init )
{
    xhci_int_ptr_t usb3_int_ptr = &usb3_int_desc;
    (void) rh_init;
    int ret;

    if (rhport == USB3_SS_PORT)
    {
        usb3_int_ptr->int_handler = tusb_int_handler;
        if (register_usb3ISR(usb3_int_ptr) == false)
        {
            return false;
        }

        if (rstmgr_deassert_reset(RST_USB1) != 0)
        {
            ERROR("\r\n Unable to deassdert the usb3 reset \r\n");
            return false;
        }

        Usb3handle = alloc_usb_port();
        if (Usb3handle == NULL)
        {
            ERROR("Cannot allocate memory!!!");
            return false;
        }

        ret = init_hcd_params();
        if (ret != 0)
        {
            ERROR("hcd error -%d", ret);
            return false;
        }

        ret = dwc3_init();
        if (ret != 0)
        {
            ERROR("dwc3 Error -%d!!!", ret);
            return false;
        }

        xhci_init(&Usb3handle->xhci_priv);
    }

    return true;
}
bool hcd_dwc3_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr)
{
  return true;
}

void hcd_dwc3_int_enable( uint8_t rhport )
{

    switch (rhport)
    {
    case USB3_HS_PORT:
        enable_xhci_interrupts();
        break;

    default: /* do nothing */
        break;
    }
}

void hcd_dwc3_int_disable( uint8_t rhport )
{
    (void) rhport;
    /* disable_xhci_interrupts(); */
}

/*--------------------------------------------------------------------+
 * Port API
 *--------------------------------------------------------------------+*/
bool hcd_dwc3_port_connect_status( uint8_t rhport )
{

    bool ret;

    ret = xhci_port_status(rhport);

    return ret;
}

void hcd_dwc3_port_reset( uint8_t rhport )
{

    (void) rhport;
    reset_usb_port(rhport);
}

static bool hcd_enable_slot( void )
{
    enable_slot_command(Usb3handle->xhci_priv.xcr_ring);

    if (wait_for_command_completion_event(&Usb3handle->xhci_priv,
            ENABLE_SLOT_CMD) != 0)
    {
        return false;
    }

    return true;
}

static bool hcd_send_address_cmd( void )
{
    int ret;

    update_device_dev_speed(&Usb3handle->xhci_priv);

    ret = init_input_device_context(&Usb3handle->xhci_priv);
    if (ret != 0)
    {
        ERROR("XHCI Error -%d!!!", ret);
        return false;
    }

    update_dcbaa_entry(&Usb3handle->xhci_priv);

    set_device_address(&Usb3handle->xhci_priv);

    if (wait_for_command_completion_event(&Usb3handle->xhci_priv,
            ADDRESS_DEVICE_CMD) != 0)
    {
        return false;
    }

    update_device_address(&Usb3handle->xhci_priv);

    if (Usb3handle->xhci_priv.dev_data.dev_addr == 0U)
    {
        return false; /* device address can not be zero after address command is sent */
    }

    display_xhci_device_params(&Usb3handle->xhci_priv.dev_data);
    display_ip_context(Usb3handle->xhci_priv.ip_ctx);
    display_op_context(Usb3handle->xhci_priv.op_ctx);
    display_event_trbs(&Usb3handle->xhci_priv);

    return true;
}

static void hcd_dwc3_update_device_address( uint8_t daddr )
{
    device_addr = 1U;
}

void hcd_dwc3_port_reset_end( uint8_t rhport )
{

    (void) rhport;
    TU_ASSERT(usb_port_reset_end(rhport),);

    hcd_enable_slot();

    hcd_send_address_cmd();
}

tusb_speed_t hcd_dwc3_port_speed_get( uint8_t rhport )
{

    (void) rhport;
    uint8_t dev_speed;
    tusb_speed_t ret;

    dev_speed = get_xhc_port_speed(rhport);
    switch (dev_speed)
    {
    case 4:
        ret = TUSB_SPEED_SS;
        break;
    case 3:
        ret = TUSB_SPEED_HIGH;
        break;
    case 2:
        ret = TUSB_SPEED_FULL;
        break;
    case 1:
        ret = TUSB_SPEED_LOW;
        break;
    default:
        ret = TUSB_SPEED_INVALID;
        break;
    }
    return ret;
}

void hcd_dwc3_device_close( uint8_t rhport, uint8_t dev_addr )
{
#if 0
    (void) dev_addr;

    uint32_t slotid;

    if (rhport == USB3_SS_PORT)
    {
        /* Issue WR for usb3 ports only */
        xhci_warm_reset(rhport);
    }

    slotid = Usb3handle->xhci_priv.dev_data.slot_id;

    if (slotid != 0U)
    {
        disable_slot_command(Usb3handle->xhci_priv.xcr_ring, slotid);

        if (wait_for_command_completion_event(&Usb3handle->xhci_priv,
                DISABLE_SLOT_CMD) != 0)
        {
            ERROR("xHCI command failed");
            return;
        }

        dealloc_usb_port(Usb3handle);
        device_addr = 0U;
    }
#endif
}

/*--------------------------------------------------------------------+
 * Endpoints API
 *--------------------------------------------------------------------+*/
static void hcd_xhci_set_configuration()
{
        update_xhc_slot_context(&Usb3handle->xhci_priv);

        init_xhc_endpoint_context(&Usb3handle->xhci_priv, BULK_OUT);

        init_xhc_endpoint_context(&Usb3handle->xhci_priv, BULK_IN);

        configure_endpoint(&Usb3handle->xhci_priv);

        if (wait_for_command_completion_event(&Usb3handle->xhci_priv,
                CONFIGURE_ENDPOINT_CMD) != 0)
        {
            return;
        }
}

bool hcd_dwc3_setup_send( uint8_t rhport, uint8_t daddr,
        uint8_t const setup_packet[ 8 ] )
{
    memcpy(&ctrl_req, &setup_packet[0], sizeof(ctrl_req));

    if ((tusb_request_code_t) setup_packet[ 1 ] == TUSB_REQ_SET_CONFIGURATION)
	{
	  hcd_xhci_set_configuration();
	  usb_set_config = 1;
	}

    if ((tusb_request_code_t) setup_packet[ 1 ] == TUSB_REQ_SET_ADDRESS)
	{
	  hcd_dwc3_update_device_address(ctrl_req.wValue);
	}
    hcd_event_xfer_complete(daddr, 0, 8, XFER_RESULT_SUCCESS, true);

    return true;
}

bool hcd_dwc3_edpt_open( uint8_t rhport, uint8_t daddr,
        tusb_desc_endpoint_t const *ep_desc )
{
    (void) rhport;
    (void) daddr;
    (void) ep_desc;

    return true;
}

bool hcd_dwc3_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
    const uint8_t ep_num = tu_edpt_number(ep_addr);
    const unsigned dir = (uint32_t) tu_edpt_dir(ep_addr);

    // There is no separate data stage for xHCI controller. Hence skip the tinyusb enumeration step for data stage
    if( buffer == NULL && (buflen == 0) && (usb_set_config == 0))
    {
	  if( usb_set_config == 1 )
	  {
        usb_set_config = 0;
	  }
      hcd_event_xfer_complete(daddr, ep_num, 8, XFER_RESULT_SUCCESS, true);
      return true;
    }

    if( ep_num == 0 )
    {
      configure_setup_stage(&Usb3handle->xhci_priv, buffer, (usb_control_request_t *)&ctrl_req);
      ring_xhci_ep0_db(&Usb3handle->xhci_priv.op_regs);
	  if( buffer != NULL )
	  {
        usb_dcache_invalidate(buffer, buflen);
      }
    }
    else
    {
	  if( dir == TUSB_DIR_OUT )
	  {
        usb_dcache_clean(buffer, buflen); 
	  }
      endpoint_transfer(&Usb3handle->xhci_priv, (int) ep_num, (uint8_t) dir, buffer, buflen);
    }

    return true;
}

bool hcd_dwc3_edpt_abort_xfer( uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr )
{
    (void) rhport;
    (void) dev_addr;
    (void) ep_addr;

    return false;
}

void hcd_dwc3_int_handler( uint8_t rhport, bool in_isr )
{

    (void) rhport;
    /*
     * array to convert XHCI DCI to corresponding endpoint address for the
     * tinyusb stack
     */
    const uint8_t DCI2EP[ 30 ] =
    {
        0x00, 0x01, 0x81, 0x02, 0x82, 0x3, 0x83,
        0x04, 0x84, 0x05, 0x85, 0x06, 0x86
    };
    uint8_t ep_num;
    int ep_dci;

    xhci_event_trb_type_t trb_id;

    xpsc_event_t psc_event;
    xtc_event_t tr_event;
    xcc_event_t cc_event;
    xhci_psceg_params_t rh_params;

    xhci_event_trb_t event_data;

    event_data = get_xhc_event(&Usb3handle->xhci_priv);

    trb_id =
            (xhci_event_trb_type_t) event_data.event_trb.trb_control_field.
            trb_type;

    switch (trb_id)
    {
    case TRANSFER_EVENT:
        tr_event = event_data.tr_event;
        if (tr_event.tc_status_params.compl_code == (uint32_t)EVENT_SUCCESS)
        {
            uint32_t xfer_bytes = tr_event.tc_status_params.transfer_len;
            ep_dci = (int) event_data.tr_event.tc_ctrl_params.ep_dci;
            ep_num = DCI2EP[ ep_dci - 1 ];
            hcd_event_xfer_complete(device_addr, ep_num, xfer_bytes, XFER_RESULT_SUCCESS,
                    true);
        }
        break;

    case PORT_STATUS_CHANGE_EVENT:
        psc_event = event_data.psc_event;
        rh_params = handle_psceg_event(psc_event);

        /* RH port id should be always less than maximum supported port */
        if ((rh_params.rhport < 1U) || (rh_params.rhport >
                Usb3handle->xhci_priv.xhc_cap_ptr->hcsparams1_params.max_ports))
        {
            break;
        }
        if (rh_params.dev_attach_flag == 1)
        {
            hcd_event_device_attach(rh_params.rhport, in_isr);
            update_device_rh_params(&Usb3handle->xhci_priv, rh_params);
        }
        else if (rh_params.dev_attach_flag == -1)
        {
            hcd_event_device_remove(rh_params.rhport, in_isr);
        }
        else
        {
            /* Nothing to be handled for tinyusb stack */
        }
        break;

    case COMMAND_COMPLETION_EVENT:
        cc_event = event_data.cc_event;
        xhci_command_event_complete(cc_event);
        break;

    default: /* do nothing */
        break;
    }

}

bool hcd_evaluate_xhci_context( void )
{
    const uint16_t bcd_usb = Usb3handle->xhci_priv.usb_desc.dev_desc.bcdUSB;
    uint16_t desc_max_pkt_size;
    const uint16_t ep0_pkt_size =
            (Usb3handle->xhci_priv.ip_ctx->xe_context[ 0 ].xec_info &
            ~EP_CTX_MAX_PKT_SIZE_MSK) >> EP_CTX_MAX_PKT_SIZE_POS;

    if (bcd_usb >= 0x300U)
    {
        desc_max_pkt_size = (uint16_t) pow(2,
                Usb3handle->xhci_priv.usb_desc.dev_desc.bMaxPacketSize0);
    }
    else
    {
        desc_max_pkt_size =
                Usb3handle->xhci_priv.usb_desc.dev_desc.bMaxPacketSize0;
    }

    if (ep0_pkt_size != desc_max_pkt_size)
    {
        update_endpoint_packetsize(Usb3handle->xhci_priv.ip_ctx,
                desc_max_pkt_size);

        evaluate_endpoint(&Usb3handle->xhci_priv);

        if (wait_for_command_completion_event(&Usb3handle->xhci_priv,
                EVALUATE_CONTEXT_CMD) != 0)
        {
            return false;
        }

        return true;
    }

    return true;
}
bool hcd_dwc3_parse_full_conf_descriptor( tusb_desc_configuration_t *desc_cfg )
{
    const uint8_t usb_speed = Usb3handle->xhci_priv.dev_data.dev_speed;

    uint16_t const total_len = tu_le16toh(desc_cfg->wTotalLength);
    uint8_t const *desc_end = ((uint8_t const*) desc_cfg) + total_len;
    uint8_t const *p_desc = tu_desc_next(desc_cfg);
    uint8_t assoc_itf_count = 1;
    uint32_t err_flag = 0U;

    usb_endpoint_descriptor_t xhci_ep_desc;

    DEBUG("Parsing Complete Configuration Descriptors");

    while (p_desc < desc_end)
    {
        TU_ASSERT(TUSB_DESC_INTERFACE == tu_desc_type(p_desc));

        tusb_desc_interface_t const *desc_itf =
                (tusb_desc_interface_t const*) (uintptr_t) p_desc;

        /* Check if the device belongs to MSC */
        if (desc_itf->bInterfaceClass != 8)
        {
            PRINT("Device does not support MSC");
            PRINT("Enumeration process completed");
            return false;
        }

        uint16_t const drv_len = tu_desc_get_interface_total_len(desc_itf,
                assoc_itf_count, (uint16_t) (desc_end - p_desc));

        tusb_desc_endpoint_t const *ep_desc =
                (tusb_desc_endpoint_t const*) (uintptr_t) tu_desc_next(
                desc_itf);

        for (int i = 0; i < 2; i++)
        {
            TU_ASSERT( TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType &&
                    TUSB_XFER_BULK == ep_desc->bmAttributes.xfer);

            xhci_ep_desc.bLength = ep_desc->bLength;
            xhci_ep_desc.bDescriptorType = ep_desc->bDescriptorType;
            xhci_ep_desc.bEndpointAddress = ep_desc->bEndpointAddress;
            (void) memcpy(&xhci_ep_desc.bmAttributes, &ep_desc->bmAttributes,
                    sizeof(xhci_ep_desc.bmAttributes));
            xhci_ep_desc.wMaxPacketSize = ep_desc->wMaxPacketSize;
            xhci_ep_desc.bInterval = ep_desc->bInterval;

            /* USB3.0 Mode */
            if (usb_speed == XHCI_SPEED_SS)
            {
                ep_desc =
                        (tusb_desc_endpoint_t const*) (uintptr_t) tu_desc_next(
                        ep_desc);

                ep_desc =
                        (tusb_desc_endpoint_t const*) (uintptr_t) tu_desc_next(
                        ep_desc);
            }
            else
            {
                ep_desc =
                        (tusb_desc_endpoint_t const*) (uintptr_t) tu_desc_next(
                        ep_desc);
            }

            if (xhci_parse_endpoint_descriptor(&Usb3handle->xhci_priv,
                    &xhci_ep_desc) != 0)
            {
                ERROR("Failed to parse endpoint descriptor");
                return false;
            }
            err_flag = 1U;
        }

        p_desc += drv_len;
    }

    if (err_flag == 0U)
    {
        return false;
    }

    return true;
}
