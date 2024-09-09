/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Mitsumine Suzu (verylowfreq)
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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && CFG_TUH_WCH_USBIP_USBFS

#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "bsp/board_api.h"

#include "ch32v20x.h"
#include "ch32v20x_usb.h"


#define USBFS_RX_BUF_LEN 64
#define USBFS_TX_BUF_LEN 64
__attribute__((aligned(4))) static uint8_t USBFS_RX_Buf[USBFS_RX_BUF_LEN];
__attribute__((aligned(4))) static uint8_t USBFS_TX_Buf[USBFS_TX_BUF_LEN];

#define USB_XFER_TIMEOUT_MILLIS 500

#define PANIC(...) do { printf("\r\nPANIC: " __VA_ARGS__); while (true) { } } while (false)

#define LOG_CH32_USBFSH(...) TU_LOG3(__VA_ARGS__)

// Busywait for delay microseconds/nanoseconds
// static void loopdelay(uint32_t count)
// {
//     volatile uint32_t c = count / 3;
//     // while (c-- != 0);
//     asm volatile(
//       "1:                     \n" // loop label
//       "    addi  %0, %0, -1   \n" // c--
//       "    bne   %0, zero, 1b \n" // if (c != 0) goto loop
//       : "+r"(c)            // c is input/output operand
//   );
// }


// Endpoint status
typedef struct usb_edpt
{
    // Is this a valid struct
    bool configured;

    uint8_t dev_addr;
    uint8_t ep_addr;
    uint16_t max_packet_size;

    // Data toggle (0 or not 0) for DATA0/1
    uint8_t data_toggle;

    // Xfer started time in millis for timeout
    uint32_t current_xfer_packet_start_millis;
    uint8_t* current_xfer_buffer;
    uint16_t current_xfer_bufferlen;
    uint16_t current_xfer_xferred_len;

} usb_edpt_t;


static usb_edpt_t usb_edpt_list[8] = { };


static usb_edpt_t* get_edpt_record(uint8_t dev_addr, uint8_t ep_addr)
{
    for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++)
    {
        usb_edpt_t* cur = &usb_edpt_list[i];
        if (cur->configured && cur->dev_addr == dev_addr && cur->ep_addr == ep_addr)
        {
            return cur;
        }
    }
    return NULL;
}

static usb_edpt_t* get_empty_record_slot(void)
{
    for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++)
    {
        if (!usb_edpt_list[i].configured)
        {
            return &usb_edpt_list[i];
        }
    }
    return NULL;
}

static usb_edpt_t* add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size)
{
    usb_edpt_t* slot = get_empty_record_slot();
    TU_ASSERT(slot != NULL, NULL);

    slot->dev_addr = dev_addr;
    slot->ep_addr = ep_addr;
    slot->max_packet_size = max_packet_size;
    slot->data_toggle = 0;
    slot->current_xfer_packet_start_millis = 0;
    slot->current_xfer_buffer = NULL;
    slot->current_xfer_bufferlen = 0;
    slot->current_xfer_xferred_len = 0;

    slot->configured = true;

    return slot;
}

static usb_edpt_t* get_or_add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size)
{
    usb_edpt_t* ret = get_edpt_record(dev_addr, ep_addr);
    if (ret != NULL)
    {
        return ret;
    }
    else
    {
        return add_edpt_record(dev_addr, ep_addr, max_packet_size);
    }
}


static void remove_edpt_record_for_device(uint8_t dev_addr)
{
    for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++)
    {
        if (usb_edpt_list[i].configured && usb_edpt_list[i].dev_addr == dev_addr)
        {
            usb_edpt_list[i].configured = false;
        }
    }
}


/** Enable or disable USBFS Host function */
static void hardware_init_host(bool enabled)
{
    // Reset USBOTG module
    USBOTG_H_FS->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;

    osal_task_delay(1);
    USBOTG_H_FS->BASE_CTRL = 0;

    if (!enabled)
    {
        // Disable all feature
        USBOTG_H_FS->BASE_CTRL = 0;
    }
    else
    {
        // Enable USB Host features
        NVIC_DisableIRQ(USBFS_IRQn);
        USBOTG_H_FS->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBOTG_H_FS->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
        USBOTG_H_FS->HOST_RX_DMA = (uint32_t)USBFS_RX_Buf;
        USBOTG_H_FS->HOST_TX_DMA = (uint32_t)USBFS_TX_Buf;
        USBOTG_H_FS->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    }
}

static bool hardware_start_xfer(uint8_t pid, uint8_t ep_addr, uint8_t data_toggle)
{
    LOG_CH32_USBFSH("hardware_start_xfer(pid=%s(0x%02x), ep_addr=0x%02x, toggle=%d)\r\n", 
        pid == USB_PID_IN ? "IN" : pid == USB_PID_OUT ? "OUT" : pid == USB_PID_SETUP ? "SETUP" : "(other)",
        pid, ep_addr, data_toggle);

    if (pid == USB_PID_IN)
    { // FIXME: long delay needed (at release build) about 30msec
        // loopdelay(SystemCoreClock / 1000 * 30);
    }

    uint8_t pid_edpt = (pid << 4) | (tu_edpt_number(ep_addr) & 0x0f);
    USBOTG_H_FS->HOST_TX_CTRL = (data_toggle != 0) ? USBFS_UH_T_TOG : 0;
    USBOTG_H_FS->HOST_RX_CTRL = (data_toggle != 0) ? USBFS_UH_R_TOG : 0;
    USBOTG_H_FS->HOST_EP_PID = pid_edpt;
    USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;
    return true;
}


/** Set device address to communicate */
static void update_device_address(uint8_t dev_addr)
{
    // Keep the bit of GP_BIT. Other 7bits are actual device address.
    USBOTG_H_FS->DEV_ADDR = (USBOTG_H_FS->DEV_ADDR & USBFS_UDA_GP_BIT) | (dev_addr & USBFS_USB_ADDR_MASK);
}

/** Set port speed */
static void update_port_speed(tusb_speed_t speed)
{
    LOG_CH32_USBFSH("update_port_speed(%s)\r\n", speed == TUSB_SPEED_FULL ? "Full" : speed == TUSB_SPEED_LOW ? "Low" : "(invalid)");
    switch (speed) {
    case TUSB_SPEED_LOW:
        USBOTG_H_FS->BASE_CTRL |= USBFS_UC_LOW_SPEED;
        USBOTG_H_FS->HOST_CTRL |= USBFS_UH_LOW_SPEED;
        USBOTG_H_FS->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
        return;
    case TUSB_SPEED_FULL:
        USBOTG_H_FS->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
        USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
        USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
        return;
    default:
        PANIC("update_port_speed(%d)\r\n", speed);
    }
}

static bool hardware_device_attached(void)
{
    return USBOTG_H_FS->MIS_ST & USBFS_UMS_DEV_ATTACH;
}


//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport)
{
    (void)rhport;
    hardware_init_host(true);

    return true;
}

bool hcd_deinit(uint8_t rhport)
{
    (void)rhport;
    hardware_init_host(false);

    return true;
}

void hcd_port_reset(uint8_t rhport)
{
    (void)rhport;
    LOG_CH32_USBFSH("hcd_port_reset()\r\n");
    NVIC_DisableIRQ(USBFS_IRQn);
    update_device_address( 0x00 );

    USBOTG_H_FS->HOST_CTRL |= USBFS_UH_BUS_RESET;
    osal_task_delay(15);
    USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    osal_task_delay(2);

    if ((USBOTG_H_FS->HOST_CTRL & USBFS_UH_PORT_EN) == 0)
    {
        if (hcd_port_speed_get(0) == TUSB_SPEED_LOW)
        {
            update_port_speed(TUSB_SPEED_LOW);
        }
    }

    USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
    USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;

    return;
}

void hcd_port_reset_end(uint8_t rhport)
{
    (void)rhport;
    LOG_CH32_USBFSH("hcd_port_reset_end()\r\n");
    // Suppress the attached event
    USBOTG_H_FS->INT_FG |= USBFS_UIF_DETECT;
    NVIC_EnableIRQ(USBFS_IRQn);

    return;
}

bool hcd_port_connect_status(uint8_t rhport)
{
    (void)rhport;

    return hardware_device_attached();
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
    (void)rhport;
    if (USBOTG_H_FS->MIS_ST & USBFS_UMS_DM_LEVEL)
    {
        return TUSB_SPEED_LOW;
    }
    else
    {
        return TUSB_SPEED_FULL;
    }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
    (void)rhport;
    LOG_CH32_USBFSH("hcd_device_close(%d, 0x%02x)\r\n", rhport, dev_addr);
    remove_edpt_record_for_device(dev_addr);

    return;
}

uint32_t hcd_frame_number(uint8_t rhport)
{
    (void)rhport;

    return board_millis();
}

void hcd_int_enable(uint8_t rhport)
{
    (void)rhport;
    NVIC_EnableIRQ(USBFS_IRQn);

    return;
}

void hcd_int_disable(uint8_t rhport)
{
    (void)rhport;
    NVIC_DisableIRQ(USBFS_IRQn);
    
    return;
}

void hcd_int_handler(uint8_t rhport, bool in_isr)
{
    (void)rhport;
    (void)in_isr;

    if (USBOTG_H_FS->INT_FG & USBFS_UIF_DETECT)
    {
        // Clear the flag
        USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT;
        // Read the detection state
        bool attached = hardware_device_attached();
        LOG_CH32_USBFSH("hcd_int_handler() attached = %d\r\n", attached  ? 1 : 0);
        if (attached)
        {
            hcd_event_device_attach(rhport, true);
        }
        else
        {
            hcd_event_device_remove(rhport, true);
        }
        return;
    }

    if (USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER)
    {
        // Copy PID and Endpoint
        uint8_t pid_edpt = USBOTG_H_FS->HOST_EP_PID;
        uint8_t status = USBOTG_H_FS->INT_ST;
        // Clear register to stop transfer
        USBOTG_H_FS->HOST_EP_PID = 0;
        // Clear the flag
        USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;

        LOG_CH32_USBFSH("hcd_int_handler() pid_edpt=0x%02x\r\n", pid_edpt);

        uint8_t request_pid = pid_edpt >> 4;
        uint8_t response_pid = USBOTG_H_FS->INT_ST & USBFS_UIS_H_RES_MASK;
        uint8_t dev_addr = USBOTG_H_FS->DEV_ADDR;
        uint8_t ep_addr = pid_edpt & 0x0f;
        if (request_pid == USB_PID_IN)
        {
            ep_addr |= 0x80;
        }

        usb_edpt_t* edpt_info = get_edpt_record(dev_addr, ep_addr);
        if (edpt_info == NULL)
        {
            PANIC("\r\nget_edpt_record() returned NULL in USBHD_IRQHandler\r\n");
        }

        if (status & USBFS_UIS_TOG_OK)
        {
            edpt_info->data_toggle ^= 0x01;

            switch (request_pid)
            {
                case USB_PID_SETUP:
                case USB_PID_OUT:
                    {
                        uint16_t xferred_len = edpt_info->current_xfer_bufferlen;
                        hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, XFER_RESULT_SUCCESS, true);
                        return;
                    }
                case USB_PID_IN:
                    {
                        uint16_t received_len = USBOTG_H_FS->RX_LEN;
                        edpt_info->current_xfer_xferred_len += received_len;
                        uint16_t xferred_len = edpt_info->current_xfer_xferred_len;
                        LOG_CH32_USBFSH("Read %d bytes\r\n", received_len);
                        // if (received_len > 0 && (edpt_info->current_xfer_buffer == NULL || edpt_info->current_xfer_bufferlen == 0)) {
                        //     PANIC("Data received but buffer not set\r\n");
                        // }
                        memcpy(edpt_info->current_xfer_buffer, USBFS_RX_Buf, received_len);
                        edpt_info->current_xfer_buffer += received_len;
                        if ((received_len < edpt_info->max_packet_size) || (xferred_len == edpt_info->current_xfer_bufferlen))
                        {
                            // USB device sent all data.
                            LOG_CH32_USBFSH("USB_PID_IN completed\r\n");
                            hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, XFER_RESULT_SUCCESS, true);
                            return;
                        }
                        else
                        {
                            // USB device may send more data.
                            LOG_CH32_USBFSH("Read more data\r\n");
                            hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
                            return;
                        }
                    }
                default:
                    {
                        PANIC("Unknown PID: 0x%02x\n", request_pid);
                    }
            }
        }
        else
        {
            if (response_pid == USB_PID_STALL)
            {
                LOG_CH32_USBFSH("Data toggle mismatched and STALL\r\n");
                hcd_edpt_clear_stall(0, dev_addr, ep_addr);
                edpt_info->data_toggle = 0;
                hardware_start_xfer(request_pid, ep_addr, 0);
                return;
            }
            else if (response_pid == USB_PID_NAK)
            {
                LOG_CH32_USBFSH("Data toggle mismatched and NAK\r\n");
                uint32_t elapsed_time = board_millis() - edpt_info->current_xfer_packet_start_millis;
                if (elapsed_time > USB_XFER_TIMEOUT_MILLIS)
                {
                    hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, true);
                }
                else
                {
                    hardware_start_xfer(request_pid, ep_addr, edpt_info->data_toggle);
                }
                return;
            }
            else if (response_pid == USB_PID_DATA0 || response_pid == USB_PID_DATA1)
            {
                LOG_CH32_USBFSH("Data toggle mismatched and DATA0/1 (not STALL). RX_LEN=%d\r\n", USBOTG_H_FS->RX_LEN);
                hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, true);
                return;
            }
            else
            {
                LOG_CH32_USBFSH("\r\nIn USBHD_IRQHandler, unexpected response PID: 0x%02x\r\n", response_pid);
                hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, true);
                return;
            }
        }
    }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
    (void)rhport;
    uint8_t ep_addr = ep_desc->bEndpointAddress;
    uint8_t ep_num = tu_edpt_number(ep_addr);
    uint16_t max_packet_size = ep_desc->wMaxPacketSize;
    LOG_CH32_USBFSH("hcd_edpt_open(rhport=%d, dev_addr=0x%02x, %p) EndpointAdderss=0x%02x,maxPacketSize=%d\r\n", rhport, dev_addr, ep_desc, ep_addr, max_packet_size);

    if (ep_num == 0x00)
    {
        TU_ASSERT(get_or_add_edpt_record(dev_addr, 0x00, max_packet_size) != NULL, false);
        TU_ASSERT(get_or_add_edpt_record(dev_addr, 0x80, max_packet_size) != NULL, false);
    }
    else
    {
        TU_ASSERT(get_or_add_edpt_record(dev_addr, ep_addr, max_packet_size) != NULL, false);
    }

    update_device_address(dev_addr);

    if (dev_addr == 0x00 && ep_num == 0x00)
    {
        // It assumes first open for the device, so make the port enable
        tusb_speed_t device_speed = hcd_port_speed_get(rhport);
        update_port_speed(device_speed);
        USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
        USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;
    }

    return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
    (void)rhport;

    usb_edpt_t* edpt_info = get_edpt_record(dev_addr, ep_addr);
    if (edpt_info == NULL)
    {
        PANIC("get_edpt_record() returned NULL in hcd_edpt_xfer()\r\n");
    }

    edpt_info->current_xfer_buffer = buffer;
    edpt_info->current_xfer_bufferlen = buflen;

    edpt_info->current_xfer_packet_start_millis = board_millis();
    edpt_info->current_xfer_xferred_len = 0;

    if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN)
    {
        LOG_CH32_USBFSH("hcd_edpt_xfer(): READ, ep_addr=0x%02x, len=%d\r\n", ep_addr, buflen);
        return hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
    }
    else
    {
        LOG_CH32_USBFSH("hcd_edpt_xfer(): WRITE, ep_addr=0x%02x, len=%d\r\n", ep_addr, buflen);
        USBOTG_H_FS->HOST_TX_LEN = buflen;
        memcpy(USBFS_TX_Buf, buffer, buflen);
        return hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
    }
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    (void) rhport;
    (void) dev_addr;
    (void) ep_addr;
    LOG_CH32_USBFSH("hcd_edpt_abort_xfer(%d, 0x%02x, 0x%02x)\r\n", rhport, dev_addr, ep_addr);

    return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
    (void)rhport;
    LOG_CH32_USBFSH("hcd_setup_send(rhport=%d, dev_addr=0x%02x, %p)\r\n", rhport, dev_addr, setup_packet);


    usb_edpt_t* edpt_info_tx = get_edpt_record(dev_addr, 0x00);
    usb_edpt_t* edpt_info_rx = get_edpt_record(dev_addr, 0x80);
    TU_ASSERT(edpt_info_tx != NULL, false);
    TU_ASSERT(edpt_info_rx != NULL, false);

    // Initialize data toggle (SETUP always starts with DATA0)
    // Data toggle for OUT is toggled in hcd_int_handler()
    edpt_info_tx->data_toggle = 0;
    // Data toggle for IN must be set 0x01 manually.
    edpt_info_rx->data_toggle = 0x01;
    const uint16_t setup_packet_datalen = 8;
    memcpy(USBFS_TX_Buf, setup_packet, setup_packet_datalen);
    USBOTG_H_FS->HOST_TX_LEN = setup_packet_datalen;

    edpt_info_tx->current_xfer_packet_start_millis = board_millis();
    edpt_info_tx->current_xfer_buffer = USBFS_TX_Buf;
    edpt_info_tx->current_xfer_bufferlen = setup_packet_datalen;
    edpt_info_tx->current_xfer_xferred_len = 0;

    hardware_start_xfer(USB_PID_SETUP, 0, 0);

    return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    (void) rhport;
    (void) dev_addr;
    LOG_CH32_USBFSH("hcd_edpt_clear_stall(rhport=%d, dev_addr=0x%02x, ep_addr=0x%02x)\r\n", rhport, dev_addr, ep_addr);
    // PANIC("\r\nstall\r\n");
    uint8_t edpt_num = tu_edpt_number(ep_addr);
    uint8_t setup_request_clear_stall[8] = {
        0x02, 0x01, 0x00, 0x00, edpt_num, 0x00, 0x00, 0x00
    };
    memcpy(USBFS_TX_Buf, setup_request_clear_stall, 8);
    USBOTG_H_FS->HOST_TX_LEN = 8;

    hcd_int_disable(0);

    USBOTG_H_FS->HOST_EP_PID = (USB_PID_SETUP << 4) | 0x00;
    USBOTG_H_FS->INT_FG |= USBFS_UIF_TRANSFER;
    while ((USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) == 0) { }
    USBOTG_H_FS->HOST_EP_PID = 0;
    uint8_t response_pid = USBOTG_H_FS->INT_ST & USBFS_UIS_H_RES_MASK;
    (void)response_pid;
    LOG_CH32_USBFSH("hcd_edpt_clear_stall() response pid=0x%02x\r\n", response_pid);

    hcd_int_enable(0);

    return true;
}

#endif
