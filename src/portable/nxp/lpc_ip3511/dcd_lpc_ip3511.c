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

/* Since 2012 starting with LPC11uxx, NXP start to use common USB Device Controller with code name LPC IP3511
 * for almost their new MCUs. Currently supported and tested families are
 * - LPC11U68, LPC11U37
 * - LPC1347
 * - LPC51U68
 * - LPC54114
 * - LPC55s69
 *
 * For similar controller of other families, this file may require some minimal changes to work with.
 * Previous MCUs such as LPC17xx, LPC40xx, LPC18xx, LPC43xx have their own driver implementation.
 */

#if TUSB_OPT_DEVICE_ENABLED && ( CFG_TUSB_MCU == OPT_MCU_LPC11UXX || \
                                 CFG_TUSB_MCU == OPT_MCU_LPC13XX  || \
                                 CFG_TUSB_MCU == OPT_MCU_LPC15XX  || \
                                 CFG_TUSB_MCU == OPT_MCU_LPC51UXX || \
                                 CFG_TUSB_MCU == OPT_MCU_LPC54XXX || \
                                 CFG_TUSB_MCU == OPT_MCU_LPC55XX)

#if CFG_TUSB_MCU == OPT_MCU_LPC11UXX || CFG_TUSB_MCU == OPT_MCU_LPC13XX || CFG_TUSB_MCU == OPT_MCU_LPC15XX
  // LPC 11Uxx, 13xx, 15xx use lpcopen
  #include "chip.h"
  #define DCD_REGS        LPC_USB

#elif CFG_TUSB_MCU == OPT_MCU_LPC51UXX || CFG_TUSB_MCU == OPT_MCU_LPC54XXX || \
      CFG_TUSB_MCU == OPT_MCU_LPC55XX // TODO 55xx has dual usb controllers
  #include "fsl_device_registers.h"
  #define DCD_REGS        USB0

#endif

#include "device/dcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Number of endpoints
// - 11 13 15 51 54 has 5x2 endpoints
// - 18/43 usb0 & 55s usb1 (HS) has 6x2 endpoints
// - 18/43 usb1 & 55s usb0 (FS) has 4x2 endpoints
#define EP_COUNT 10

// only SRAM1 & USB RAM can be used for transfer.
// Used to set DATABUFSTART which is 22-bit aligned
// 2000 0000 to 203F FFFF
#define SRAM_REGION   0x20000000

/* Although device controller are the same. Somehow only LPC134x can execute
 * DMA with 1023 bytes for Bulk/Control. Others (11u, 51u, 54xxx) can only work
 * with max 64 bytes
 */
enum {
  #if CFG_TUSB_MCU == OPT_MCU_LPC13XX
    DMA_NBYTES_MAX = 1023
  #else
    DMA_NBYTES_MAX = 64
  #endif
};

enum {
  INT_SOF_MASK           = TU_BIT(30),
  INT_DEVICE_STATUS_MASK = TU_BIT(31)
};

enum {
  CMDSTAT_DEVICE_ADDR_MASK    = TU_BIT(7 )-1,
  CMDSTAT_DEVICE_ENABLE_MASK  = TU_BIT(7 ),
  CMDSTAT_SETUP_RECEIVED_MASK = TU_BIT(8 ),
  CMDSTAT_DEVICE_CONNECT_MASK = TU_BIT(16), ///< reflect the soft-connect only, does not reflect the actual attached state
  CMDSTAT_DEVICE_SUSPEND_MASK = TU_BIT(17),
  CMDSTAT_CONNECT_CHANGE_MASK = TU_BIT(24),
  CMDSTAT_SUSPEND_CHANGE_MASK = TU_BIT(25),
  CMDSTAT_RESET_CHANGE_MASK   = TU_BIT(26),
  CMDSTAT_VBUS_DEBOUNCED_MASK = TU_BIT(28),
};

typedef struct TU_ATTR_PACKED
{
  // Bits 21:6 (aligned 64) used in conjunction with bit 31:22 of DATABUFSTART
  volatile uint16_t buffer_offset;

  volatile uint16_t nbytes : 10 ;
  uint16_t is_iso          : 1  ;
  uint16_t toggle_mode     : 1  ;
  uint16_t toggle_reset    : 1  ;
  uint16_t stall           : 1  ;
  uint16_t disable         : 1  ;
  volatile uint16_t active : 1  ;
}ep_cmd_sts_t;

TU_VERIFY_STATIC( sizeof(ep_cmd_sts_t) == 4, "size is not correct" );

typedef struct
{
  uint16_t total_bytes;
  uint16_t xferred_bytes;

  uint16_t nbytes;
}xfer_dma_t;

// NOTE data will be transferred as soon as dcd get request by dcd_pipe(_queue)_xfer using double buffering.
// current_td is used to keep track of number of remaining & xferred bytes of the current request.
typedef struct
{
  // 256 byte aligned, 2 for double buffer (not used)
  // Each cmd_sts can only transfer up to DMA_NBYTES_MAX bytes each
  ep_cmd_sts_t ep[EP_COUNT][2];

  xfer_dma_t dma[EP_COUNT];

  TU_ATTR_ALIGNED(64) uint8_t setup_packet[8];
}dcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

// EP list must be 256-byte aligned
CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(256) static dcd_data_t _dcd;

static inline uint16_t get_buf_offset(void const * buffer)
{
  uint32_t addr = (uint32_t) buffer;
  TU_ASSERT( (addr & 0x3f) == 0, 0 );
  return ( (addr >> 6) & 0xFFFFUL ) ;
}

static inline uint8_t ep_addr2id(uint8_t endpoint_addr)
{
  return 2*(endpoint_addr & 0x0F) + ((endpoint_addr & TUSB_DIR_IN_MASK) ? 1 : 0);
}

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
void dcd_init(uint8_t rhport)
{
  (void) rhport;

  DCD_REGS->EPLISTSTART  = (uint32_t) _dcd.ep;
  DCD_REGS->DATABUFSTART = SRAM_REGION; // 22-bit alignment

  DCD_REGS->INTSTAT      = DCD_REGS->INTSTAT; // clear all pending interrupt
  DCD_REGS->INTEN        = INT_DEVICE_STATUS_MASK;
  DCD_REGS->DEVCMDSTAT  |= CMDSTAT_DEVICE_ENABLE_MASK | CMDSTAT_DEVICE_CONNECT_MASK |
                           CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;

  NVIC_ClearPendingIRQ(USB0_IRQn);
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB0_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB0_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  // Response with status first before changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

  DCD_REGS->DEVCMDSTAT &= ~CMDSTAT_DEVICE_ADDR_MASK;
  DCD_REGS->DEVCMDSTAT |= dev_addr;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  DCD_REGS->DEVCMDSTAT |= CMDSTAT_DEVICE_CONNECT_MASK;
}

void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  DCD_REGS->DEVCMDSTAT &= ~CMDSTAT_DEVICE_CONNECT_MASK;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  // TODO cannot able to STALL Control OUT endpoint !!!!! FIXME try some walk-around
  uint8_t const ep_id = ep_addr2id(ep_addr);
  _dcd.ep[ep_id][0].stall = 1;
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const ep_id = ep_addr2id(ep_addr);

  _dcd.ep[ep_id][0].stall        = 0;
  _dcd.ep[ep_id][0].toggle_reset = 1;
  _dcd.ep[ep_id][0].toggle_mode  = 0;
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void) rhport;

  // TODO not support ISO yet
  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) return false;

  //------------- Prepare Queue Head -------------//
  uint8_t ep_id = ep_addr2id(p_endpoint_desc->bEndpointAddress);

  // Check if endpoint is available
  TU_ASSERT( _dcd.ep[ep_id][0].disable && _dcd.ep[ep_id][1].disable );

  tu_memclr(_dcd.ep[ep_id], 2*sizeof(ep_cmd_sts_t));
  _dcd.ep[ep_id][0].is_iso = (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);

  // Enable EP interrupt
  DCD_REGS->INTEN |= TU_BIT(ep_id);

  return true;
}

static void prepare_ep_xfer(uint8_t ep_id, uint16_t buf_offset, uint16_t total_bytes)
{
  uint16_t const nbytes = tu_min16(total_bytes, DMA_NBYTES_MAX);

  _dcd.dma[ep_id].nbytes = nbytes;

  _dcd.ep[ep_id][0].buffer_offset = buf_offset;
  _dcd.ep[ep_id][0].nbytes        = nbytes;
  _dcd.ep[ep_id][0].active        = 1;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const ep_id = ep_addr2id(ep_addr);

  tu_varclr(&_dcd.dma[ep_id]);
  _dcd.dma[ep_id].total_bytes = total_bytes;

  prepare_ep_xfer(ep_id, get_buf_offset(buffer), total_bytes);

  return true;
}

//--------------------------------------------------------------------+
// IRQ
//--------------------------------------------------------------------+
static void bus_reset(void)
{
  tu_memclr(&_dcd, sizeof(dcd_data_t));

  // disable all non-control endpoints on bus reset
  for(uint8_t ep_id = 2; ep_id < EP_COUNT; ep_id++)
  {
    _dcd.ep[ep_id][0].disable = _dcd.ep[ep_id][1].disable = 1;
  }

  _dcd.ep[0][1].buffer_offset = get_buf_offset(_dcd.setup_packet);

  DCD_REGS->EPINUSE      = 0;
  DCD_REGS->EPBUFCFG     = 0;
  DCD_REGS->EPSKIP       = 0xFFFFFFFF;

  DCD_REGS->INTSTAT      = DCD_REGS->INTSTAT; // clear all pending interrupt
  DCD_REGS->DEVCMDSTAT  |= CMDSTAT_SETUP_RECEIVED_MASK; // clear setup received interrupt
  DCD_REGS->INTEN        = INT_DEVICE_STATUS_MASK | TU_BIT(0) | TU_BIT(1); // enable device status & control endpoints
}

static void process_xfer_isr(uint32_t int_status)
{
  for(uint8_t ep_id = 0; ep_id < EP_COUNT; ep_id++ )
  {
    if ( tu_bit_test(int_status, ep_id) )
    {
      ep_cmd_sts_t * ep_cs = &_dcd.ep[ep_id][0];
      xfer_dma_t* xfer_dma = &_dcd.dma[ep_id];

      xfer_dma->xferred_bytes += xfer_dma->nbytes - ep_cs->nbytes;

      if ( (ep_cs->nbytes == 0) && (xfer_dma->total_bytes > xfer_dma->xferred_bytes) )
      {
        // There is more data to transfer
        // buff_offset has been already increased by hw to correct value for next transfer
        prepare_ep_xfer(ep_id, ep_cs->buffer_offset, xfer_dma->total_bytes - xfer_dma->xferred_bytes);
      }
      else
      {
        xfer_dma->total_bytes = xfer_dma->xferred_bytes;

        uint8_t const ep_addr = (ep_id / 2) | ((ep_id & 0x01) ? TUSB_DIR_IN_MASK : 0);

        // TODO no way determine if the transfer is failed or not
        dcd_event_xfer_complete(0, ep_addr, xfer_dma->xferred_bytes, XFER_RESULT_SUCCESS, true);
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport; // TODO support multiple USB on supported mcu such as LPC55s69

  uint32_t const cmd_stat = DCD_REGS->DEVCMDSTAT;

  uint32_t int_status = DCD_REGS->INTSTAT & DCD_REGS->INTEN;
  DCD_REGS->INTSTAT = int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  //------------- Device Status -------------//
  if ( int_status & INT_DEVICE_STATUS_MASK )
  {
    DCD_REGS->DEVCMDSTAT |= CMDSTAT_RESET_CHANGE_MASK | CMDSTAT_CONNECT_CHANGE_MASK | CMDSTAT_SUSPEND_CHANGE_MASK;
    if ( cmd_stat & CMDSTAT_RESET_CHANGE_MASK) // bus reset
    {
      bus_reset();
      dcd_event_bus_signal(0, DCD_EVENT_BUS_RESET, true);
    }

    if (cmd_stat & CMDSTAT_CONNECT_CHANGE_MASK)
    {
      // device disconnect
      if (cmd_stat & CMDSTAT_DEVICE_ADDR_MASK)
      {
        // debouncing as this can be set when device is powering
        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
      }
    }

    // TODO support suspend & resume
    if (cmd_stat & CMDSTAT_SUSPEND_CHANGE_MASK)
    {
      if (cmd_stat & CMDSTAT_DEVICE_SUSPEND_MASK)
      { // suspend signal, bus idle for more than 3ms
        // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
        if (cmd_stat & CMDSTAT_DEVICE_ADDR_MASK)
        {
          dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
        }
      }
    }
//        else
//      { // resume signal
//    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
//      }
//    }
  }

  // Setup Receive
  if ( tu_bit_test(int_status, 0) && (cmd_stat & CMDSTAT_SETUP_RECEIVED_MASK) )
  {
    // Follow UM flowchart to clear Active & Stall on both Control IN/OUT endpoints
    _dcd.ep[0][0].active = _dcd.ep[1][0].active = 0;
    _dcd.ep[0][0].stall = _dcd.ep[1][0].stall = 0;

    DCD_REGS->DEVCMDSTAT |= CMDSTAT_SETUP_RECEIVED_MASK;

    dcd_event_setup_received(0, _dcd.setup_packet, true);

    // keep waiting for next setup
    _dcd.ep[0][1].buffer_offset = get_buf_offset(_dcd.setup_packet);

    // clear bit0
    int_status = tu_bit_clear(int_status, 0);
  }

  // Endpoint transfer complete interrupt
  process_xfer_isr(int_status);
}

#endif

