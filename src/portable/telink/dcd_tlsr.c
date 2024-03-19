/*
 * The MIT License (MIT)
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

#include "tusb_option.h"
#include "device/dcd.h"
#include "driver.h"

/**
 * @brief Control endpoint and data endpoint[0-7]. The control endpoint is a separate set of registers.
 *
 */
#define EP_MAX 9

typedef struct
{
    unsigned char *buffer;
    unsigned short total_len;
    unsigned short queued_len;
    unsigned short max_size;
    bool isochronous;
    bool short_packet;
    bool stall_flag;
} xfer_ctl_t;

static xfer_ctl_t xfer_status[EP_MAX][2];

#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]

static uint8_t _setup_packet[CFG_TUD_ENDPOINT0_SIZE];
extern void tud_task_ext(uint32_t timeout_ms, bool in_isr);

void dcd_init(uint8_t rhport)
{
    (void)rhport;

    /* enable global interrupt. */
#if (MCU_CORE_B91 || MCU_CORE_B92)
    core_interrupt_enable();
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    irq_enable();
#endif

    usbhw_enable_manual_interrupt(FLD_CTRL_EP_AUTO_STD | FLD_CTRL_EP_AUTO_DESC | FLD_CTRL_EP_AUTO_CFG);

    usbhw_set_irq_mask(USB_IRQ_RESET_MASK | USB_IRQ_SUSPEND_MASK);

    usb_set_pin_en();
}

void dcd_int_enable(uint8_t rhport)
{
    (void)rhport;

#if (MCU_CORE_B91 || MCU_CORE_B92)
    plic_interrupt_enable(IRQ_USB_CTRL_EP_SETUP);
    plic_interrupt_enable(IRQ_USB_CTRL_EP_DATA);
    plic_interrupt_enable(IRQ_USB_CTRL_EP_STATUS);
    plic_interrupt_enable(IRQ_USB_CTRL_EP_SETINF);
    plic_interrupt_enable(IRQ_USB_RESET);
    plic_interrupt_enable(IRQ_USB_ENDPOINT);
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    // TODO
#endif
}

void dcd_int_disable(uint8_t rhport)
{
    (void)rhport;

#if (MCU_CORE_B91 || MCU_CORE_B92)
    plic_interrupt_disable(IRQ_USB_CTRL_EP_SETUP);
    plic_interrupt_disable(IRQ_USB_CTRL_EP_DATA);
    plic_interrupt_disable(IRQ_USB_CTRL_EP_STATUS);
    plic_interrupt_disable(IRQ_USB_CTRL_EP_SETINF);
    plic_interrupt_disable(IRQ_USB_RESET);
    plic_interrupt_disable(IRQ_USB_ENDPOINT);
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    // TODO
#endif
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
    (void)rhport;
    (void)dev_addr;
}

void dcd_remote_wakeup(uint8_t rhport)
{
    (void)rhport;
    // TODO
}

void dcd_connect(uint8_t rhport)
{
    (void)rhport;

    usb_dp_pullup_en(1);
}

void dcd_disconnect(uint8_t rhport)
{
    (void)rhport;

    usb_dp_pullup_en(0);
}

void dcd_sof_enable(uint8_t rhport, bool en)
{
    (void)rhport;
    (void)en;
    // TODO
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *desc_edpt)
{
    (void)rhport;
    (void)desc_edpt;

    uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);

    reg_usb_edp_en |= BIT(epnum & 0x07); // endpoint 8 means endpoint 0.

    return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
    (void)rhport;

    reg_usb_edp_en = 0;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
    (void)rhport;
    (void)ep_addr;
    (void)buffer;
    (void)total_bytes;

    unsigned char const epnum = tu_edpt_number(ep_addr);
    unsigned char const dir = tu_edpt_dir(ep_addr);
    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    xfer->buffer = buffer;
    xfer->total_len = total_bytes;
    xfer->queued_len = 0;
    xfer->short_packet = false;

    return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
    (void)rhport;
    (void)ep_addr;

    unsigned char const epnum = tu_edpt_number(ep_addr);
    unsigned char const dir = tu_edpt_dir(ep_addr);
    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    xfer->stall_flag = 1;
    if (0 == epnum)
    {
        usbhw_write_ctrl_ep_ctrl(FLD_EP_DAT_STALL);
    }
    else
    {
        usbhw_data_ep_stall(epnum);
    }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
    (void)rhport;
    (void)ep_addr;

    unsigned char const epnum = tu_edpt_number(ep_addr);
    unsigned char const dir = tu_edpt_dir(ep_addr);
    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    xfer->stall_flag = 0;

    if (0 == epnum)
    {
        usbhw_write_ctrl_ep_ctrl(FLD_EP_DAT_ACK);
    }
    else
    {
        usbhw_data_ep_ack(epnum);
    }
}

#if (MCU_CORE_B91 || MCU_CORE_B92)
_attribute_ram_code_sec_ void usb_ctrl_ep_setup_irq_handler(void)
{
    usbhw_clr_ctrl_ep_irq(FLD_CTRL_EP_IRQ_SETUP);
    xfer_status[0][TUSB_DIR_OUT].stall_flag = 0;
    xfer_status[0][TUSB_DIR_IN].stall_flag = 0;

    usbhw_reset_ctrl_ep_ptr();
    for (int i = 0; i < CFG_TUD_ENDPOINT0_SIZE; i++)
    {
        _setup_packet[i] = reg_ctrl_ep_dat;
    }

    dcd_event_setup_received(0, (uint8_t *)&_setup_packet[0], true);
    tud_task_ext(0, true);

    xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
    if (xfer->total_len != xfer->queued_len)
    {
        uint8_t *base = (xfer->buffer + xfer->queued_len);
        uint16_t remaining = xfer->total_len - xfer->queued_len;
        uint8_t xfer_size = (xfer->max_size < xfer->total_len) ? xfer->max_size : remaining;

        usbhw_reset_ctrl_ep_ptr();
        for (unsigned short i = 0; i < xfer_size; i++)
        {
            usbhw_write_ctrl_ep_data(base[i]);
        }
        xfer->queued_len += xfer_size;
        dcd_event_xfer_complete(0, 0 | TUSB_DIR_IN_MASK, xfer_size, XFER_RESULT_SUCCESS, true);
        tud_task_ext(0, true);
    }
    if (!xfer->stall_flag)
    {
        usbhw_write_ctrl_ep_ctrl(FLD_EP_DAT_ACK);
    }
}
PLIC_ISR_REGISTER(usb_ctrl_ep_setup_irq_handler, IRQ_USB_CTRL_EP_SETUP);

_attribute_ram_code_sec_ void usb_ctrl_ep_data_irq_handler(void)
{
    usbhw_clr_ctrl_ep_irq(FLD_CTRL_EP_IRQ_DATA);

    xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
    if (xfer->total_len != xfer->queued_len)
    {
        uint8_t *base = (xfer->buffer + xfer->queued_len);
        uint16_t remaining = xfer->total_len - xfer->queued_len;
        uint8_t xfer_size = (xfer->max_size < xfer->total_len) ? xfer->max_size : remaining;

        usbhw_reset_ctrl_ep_ptr();
        for (unsigned short i = 0; i < xfer_size; i++)
        {
            usbhw_write_ctrl_ep_data(base[i]);
        }
        xfer->queued_len += xfer_size;
        dcd_event_xfer_complete(0, 0 | TUSB_DIR_IN_MASK, xfer_size, XFER_RESULT_SUCCESS, true);
        tud_task_ext(0, true);
    }

    if (!xfer->stall_flag)
    {
        usbhw_write_ctrl_ep_ctrl(FLD_EP_DAT_ACK);
    }
}
PLIC_ISR_REGISTER(usb_ctrl_ep_data_irq_handler, IRQ_USB_CTRL_EP_DATA);

_attribute_ram_code_sec_ void usb_ctrl_ep_status_irq_handler(void)
{
    usbhw_clr_ctrl_ep_irq(FLD_CTRL_EP_IRQ_STA);

    xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
    if (!xfer->stall_flag)
    {
        usbhw_write_ctrl_ep_ctrl(FLD_EP_STA_ACK);
    }
}
PLIC_ISR_REGISTER(usb_ctrl_ep_status_irq_handler, IRQ_USB_CTRL_EP_STATUS);

_attribute_ram_code_sec_ void usb_ctrl_ep_setinf_irq_handler(void)
{
    usbhw_clr_ctrl_ep_irq(FLD_CTRL_EP_IRQ_INTF);
}
PLIC_ISR_REGISTER(usb_ctrl_ep_setinf_irq_handler, IRQ_USB_CTRL_EP_SETINF);

_attribute_ram_code_sec_ void usb_pwdn_irq_handler(void)
{
    usbhw_clr_irq_status(USB_IRQ_SUSPEND_STATUS);
}
PLIC_ISR_REGISTER(usb_pwdn_irq_handler, IRQ_USB_PWDN);

static void bus_reset(void)
{
    xfer_status[0][TUSB_DIR_OUT].max_size = CFG_TUD_ENDPOINT0_SIZE;
    xfer_status[0][TUSB_DIR_IN].max_size = CFG_TUD_ENDPOINT0_SIZE;
}

_attribute_ram_code_sec_ void usb_reset_irq_handler(void)
{
    usbhw_clr_irq_status(USB_IRQ_RESET_STATUS); /* Clear USB reset flag */
    bus_reset();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
}
PLIC_ISR_REGISTER(usb_reset_irq_handler, IRQ_USB_RESET);

_attribute_ram_code_sec_ void usb_endpoint_irq_handler(void)
{
    unsigned char irq = usbhw_get_eps_irq();

    if (irq & FLD_USB_EDP8_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP8_IRQ);
    }
    if (irq & FLD_USB_EDP7_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP7_IRQ);
    }
    if (irq & FLD_USB_EDP6_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP6_IRQ);
    }
    if (irq & FLD_USB_EDP5_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP5_IRQ);
    }
    if (irq & FLD_USB_EDP4_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP4_IRQ);
    }
    if (irq & FLD_USB_EDP3_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP3_IRQ);
    }
    if (irq & FLD_USB_EDP2_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP2_IRQ);
    }
    if (irq & FLD_USB_EDP1_IRQ)
    {
        usbhw_clr_eps_irq(FLD_USB_EDP1_IRQ);
    }
}
PLIC_ISR_REGISTER(usb_endpoint_irq_handler, IRQ_USB_ENDPOINT);
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
_attribute_ram_code_sec_noinline_ void irq_handler(void)
{
    // TODO
}
#endif
