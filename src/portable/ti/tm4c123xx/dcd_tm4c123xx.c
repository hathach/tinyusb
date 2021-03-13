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
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && (CFG_TUSB_MCU == OPT_MCU_TM4C123)
#include "device/dcd.h"
#include "dcd_tm4c123xx.h"
#include "TM4C123.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_ENDPOINT_MAX 32

typedef struct TU_ATTR_ALIGNED(4)
{
  //------------- Word 0 -------------//
  uint32_t next;

  //------------- Word 1 -------------//
  uint16_t atle_mode       : 2; // 00: normal, 01: ATLE (auto length extraction)
  uint16_t next_valid      : 1;
  uint16_t                 : 1; ///< reserved
  uint16_t isochronous     : 1; // is an iso endpoint
  uint16_t max_packet_size : 11;

  volatile uint16_t buflen; // bytes for non-iso, number of packets for iso endpoint

  //------------- Word 2 -------------//
  volatile uint32_t buffer;

  //------------- Word 3 -------------//
  volatile uint16_t retired                : 1; // initialized to zero
  volatile uint16_t status                 : 4;
  volatile uint16_t iso_last_packet_valid  : 1;
  volatile uint16_t atle_lsb_extracted     : 1;	// used in ATLE mode
  volatile uint16_t atle_msb_extracted     : 1;	// used in ATLE mode
  volatile uint16_t atle_mess_len_position : 6; // used in ATLE mode
  uint16_t                                 : 2;

  volatile uint16_t present_count;  // For non-iso : The number of bytes transferred by the DMA engine
                                    // For iso : number of packets

  //------------- Word 4 -------------//
  //	uint32_t iso_packet_size_addr;		// iso only, can be omitted for non-iso
}dma_desc_t;

TU_VERIFY_STATIC( sizeof(dma_desc_t) == 16, "size is not correct"); // TODO not support ISO for now

typedef struct
{
  // must be 128 byte aligned
  volatile dma_desc_t* udca[DCD_ENDPOINT_MAX];

  // TODO DMA does not support control transfer (0-1 are not used, offset to reduce memory)
  dma_desc_t dd[DCD_ENDPOINT_MAX];

  struct
  {
    uint8_t* out_buffer;
    uint8_t  out_bytes;
    volatile bool out_received; // indicate if data is already received in endpoint

    uint8_t  in_bytes;
  } control;

} dcd_data_t;

/* TODO : Remove attribute post implementation */
__attribute__((used)) CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(128) static dcd_data_t _dcd;

//--------------------------------------------------------------------+
// Internal IP Specific functions
//--------------------------------------------------------------------+

#define USB_IE_SUSPEND_Msk (0x01u)
#define USB_IE_RESUME_Msk  (0x02u)
#define USB_IE_RESET_Msk   (0x04u)
#define USB_IE_SOF_Msk     (0x08u)
#define USB_IE_DISCON_Msk  (0x20u)

#define USB_CSRL0_RXRDYC        0x00000040  // RXRDY Clear
#define USB_CSRL0_STALL         0x00000020  // Send Stall
#define USB_CSRL0_SETEND        0x00000010  // Setup End
#define USB_CSRL0_ERROR         0x00000010  // Error
#define USB_CSRL0_DATAEND       0x00000008  // Data End

typedef enum
{
    //
    // The USB device is waiting on a request from the host controller on
    // endpoint zero.
    //
    EpIdle=0,

    //
    // The USB device is sending data back to the host due to an IN request.
    //
    EpTx,

    //
    // The USB device is receiving data from the host due to an OUT
    // request from the host.
    //
    EpRx,

    //
    // The USB device has completed the IN or OUT request and is now waiting
    // for the host to acknowledge the end of the IN/OUT transaction.  This
    // is the status phase for a USB control transaction.
    //
    EpStatus,

    //
    // This endpoint has signaled a stall condition and is waiting for the
    // stall to be acknowledged by the host controller.
    //
    EpStall
}
ep0_state_t;

ep0_state_t Ep0State;
uint32_t dev_address=0; 

/* TODO : Remove attribute post implementation */
__attribute__((used)) 
static void enable_usb_interrupts(uint32_t mask)
{
    USB0->IE |= mask; 
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) 
static void disable_usb_interrupts(uint32_t mask)
{
    USB0->IE &= ~mask; 
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) 
static void enable_ep_interrupts(uint32_t mask)
{
    USB0->TXIE |= mask; 
    USB0->RXIE |= (mask & (~0x01));
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) 
static uint32_t read_ep_intr_stat(void)
{
    return ((USB0->RXIS << 8) | (USB0->TXIS)) ; 
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) 
static void ack_usb_xfer(uint8_t EpNum, bool isLast)
{
    if(EpNum==0)
    {
        USB0->CSRL0 = USB_CSRL0_RXRDYC | (isLast ? USB_CSRL0_DATAEND : 0);
    }
}

//--------------------------------------------------------------------+
// SIE Command
//--------------------------------------------------------------------+
/* TODO : Remove attribute post implementation */
__attribute__((used)) static void sie_cmd_code (sie_cmdphase_t phase, uint8_t code_data)
{
  (void)phase;
  (void)code_data;
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) static void sie_write (uint8_t cmd_code, uint8_t data_len, uint8_t data)
{
  (void)cmd_code;
  (void)data_len;
  (void)data;
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) static uint8_t sie_read (uint8_t cmd_code)
{
  (void)cmd_code;
  return 1;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
/* TODO : Remove attribute post implementation */
__attribute__((used)) static inline uint8_t ep_addr2idx(uint8_t ep_addr)
{
  (void)ep_addr;
  return 1;
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) static void set_ep_size(uint8_t ep_id, uint16_t max_packet_size)
{
  (void)max_packet_size;
  (void)ep_id;
}


//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
#if 0 
static void bus_reset(void)
{

    /* Although the IP does all this automatically on detecing a Reset */ 

    /* Clear EP Interrupts */
    dummy = USB0->TXIS;
    dummy = USB0->RXIS;

    /* Clear USB IP Interrupts */
    dummy = USB0->IS;

    /* Enable USB Interrupts */
    enable_usb_interrupts((1u<<0) | (1u<<1) | (1u<<2) | (1u<<3) | (1u<<5));

    /* Enable Endpoint Interrupts */
    enable_ep_interrupts(0xFF);
}
#endif 

void dcd_init(uint8_t rhport)
{
    (void)rhport;
    
    uint32_t dummy;

    /* Set Device Mode */
    USB0->GPCS = 0x03;

    /* Clear EP Interrupts */
    read_ep_intr_stat(); 

    /* Clear USB IP Interrupts */
    dummy = USB0->IS;
    dummy++; 

    /* Enable USB Interrupts */ 
    enable_usb_interrupts((1u<<0) | (1u<<1) | (1u<<2) | (1u<<3) | (1u<<5));

    /* Enable Endpoint Interrupts */ 
    enable_ep_interrupts(0xFF); 

    /* Apply D+/D- Terminations */ 
    dcd_connect(0); 

    NVIC_ClearPendingIRQ(USB0_IRQn);
}

void dcd_int_enable(uint8_t rhport)
{
	NVIC_ClearPendingIRQ(USB0_IRQn);
    (void)rhport;
    NVIC_EnableIRQ(USB0_IRQn); 
}

void dcd_int_disable(uint8_t rhport)
{
    (void)rhport;
    NVIC_DisableIRQ(USB0_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  
    dev_address = dev_addr | (1u<<31) ; 
    ack_usb_xfer(0,true); 
  (void)rhport;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
}

void dcd_connect(uint8_t rhport)
{
    (void)rhport;

    /* Apply D+/D- Terminations */
    USB0->POWER |= (1<<6);
}

void dcd_disconnect(uint8_t rhport)
{
    (void)rhport;
    
    /* Remove D+/D- Terminations */
    USB0->POWER &= ~(1<<6);

}

//--------------------------------------------------------------------+
// CONTROL HELPER
//--------------------------------------------------------------------+
/* TODO : Remove attribute post implementation */
__attribute__((used)) static inline uint8_t byte2dword(uint8_t bytes)
{
  (void)bytes;
  return 1;
}

static void control_ep_write(void const * buffer, uint8_t len)
{
    uint8_t const *data = buffer;

    for(uint16_t i=0; i<len; i++)
    {
        *((volatile uint8_t*)&USB0->FIFO0) = data[i] ;
    }

    while(USB0->CSRL0 & 0x02) ;

    USB0->CSRL0 = 0x0A ;
}


static uint8_t control_ep_read(uint32_t * buffer)
{
    uint32_t size = 0 ; 
    /* Check if a packet was received */ 
    if(USB0->CSRL0 & 0x01)
    {
        /* Read packet length */ 
        size = USB0->COUNT0; 
        
        /* Read packet */
        for(uint32_t i = 0; i<size; i++)
        {
            buffer[i] = USB0->FIFO0; 
        }
        
        Ep0State=EpStatus; 
    }
  return 1;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
 (void)rhport;
 (void)p_endpoint_desc;
 return 1;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
 (void)rhport;
 (void)ep_addr;
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
 (void)rhport;
 (void)ep_addr;
}

/* TODO : Remove attribute post implementation */
__attribute__((used))static bool control_xact(uint8_t rhport, uint8_t dir, uint8_t * buffer, uint8_t len)
{
    if(dir)
    {
        _dcd.control.in_bytes = len;
        control_ep_write(buffer, len);
    }

    return true; 
  (void)rhport;
  return 1;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
    if( tu_edpt_number(ep_addr) == 0)
    {
        return control_xact(rhport, tu_edpt_dir(ep_addr), buffer, (uint8_t) total_bytes);
    }

  //(void)rhport;
  //(void)ep_addr;
  //(void)buffer;
  //(void)total_bytes;
  return 1;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

void USB0_Handler(void)
{
	dcd_int_handler(0);  
}


static void handle_control_ep(uint8_t rhport)
{
     
    uint32_t control_packet[16]; 
    
    switch(Ep0State)
    {
        case EpIdle:
        {
            /* TODO: Add check if Setup req was received */
            control_ep_read(control_packet);
            
            dcd_event_setup_received(rhport, (uint8_t*)control_packet, true);
            
            break; 
        }

        case EpStatus:
        {
            Ep0State=EpIdle; 

            if(dev_address & (1u<<31))
            {
                USB0->FADDR = (uint8_t)dev_address; 
                dev_address &= ~(1u<<31); 
            }
            else
            {
                /* ACK the data received (expecting a data stage next) */
                ack_usb_xfer(0, false); 
            }
        }

        case EpTx:
        case EpRx:
        case EpStall: 
        default: break; 

    }
                 
}

// handle bus event signal

static void bus_event_isr(uint8_t rhport)
{
    /* Check for USB Signal Interrupts */ 
    uint32_t usb_signal_stat = USB0->IS; 

    if(usb_signal_stat & USB_IE_RESET_Msk)
    {
        dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, true);
    }

    if(usb_signal_stat & USB_IE_SUSPEND_Msk)
    {
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
    }

    if(usb_signal_stat & USB_IE_RESUME_Msk)
    {
        dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
    }
    
    if(usb_signal_stat & USB_IE_SOF_Msk)
    {
      /* Do nothing as the stack skips it */  
    }

    if(usb_signal_stat & USB_IE_DISCON_Msk)
    {
        dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
    }
}


// main USB IRQ handler
void dcd_int_handler(uint8_t rhport)
{
    bus_event_isr(rhport);
    
    uint32_t ep_intr = read_ep_intr_stat(); 

    if ((ep_intr & 0x01))
    {
        handle_control_ep(rhport);
    }
}

#endif
