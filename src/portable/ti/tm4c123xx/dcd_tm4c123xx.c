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
/* TODO : Remove attribute post implementation */
__attribute__((used)) static void bus_reset(void)
{

}

void dcd_init(uint8_t rhport)
{
  (void)rhport;
  /* Apply D+/D- Terminations */ 
    USB0->POWER |= (1<<6); 
}

void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;
}

void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)dev_addr;
  (void)rhport;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void)rhport;
}

void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;
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

/* TODO : Remove attribute post implementation */
__attribute__((used)) static void control_ep_write(void const * buffer, uint8_t len)
{
  (void)buffer;
  (void)len;
}

/* TODO : Remove attribute post implementation */
__attribute__((used)) static uint8_t control_ep_read(void * buffer, uint8_t len)
{
  (void)buffer;
  (void)len;
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
  (void)rhport;
  (void)buffer;
  (void)dir;
  (void)len;
  return 1;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void)rhport;
  (void)ep_addr;
  (void)buffer;
  (void)total_bytes;
  return 1;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

// handle control xfer (slave mode)
/* TODO : Remove attribute post implementation */
__attribute__((used))static void control_xfer_isr(uint8_t rhport, uint32_t ep_int_status)
{
  (void)rhport;
  (void)ep_int_status;
}

// handle bus event signal
/* TODO : Remove attribute post implementation */
__attribute__((used)) static void bus_event_isr(uint8_t rhport)
{
  (void)rhport;
}


// Helper to complete a DMA descriptor for non-control transfer
/* TODO : Remove attribute post implementation */
__attribute__((used)) static void dd_complete_isr(uint8_t rhport, uint8_t ep_id)
{
  (void)rhport;
  (void)ep_id;
}

// main USB IRQ handler
void dcd_int_handler(uint8_t rhport)
{
  (void)rhport;
}

#endif
