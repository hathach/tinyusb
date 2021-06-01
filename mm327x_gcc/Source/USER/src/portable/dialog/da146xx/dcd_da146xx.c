/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jerzy Kasenberg
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_DA1469X

#include "DA1469xAB.h"

#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#define USE_SOF           0

// Size of RX or TX FIFO.
#define FIFO_SIZE         64

#ifndef TU_DA1469X_FIFO_READ_THRESHOLD
// RX FIFO is 64 bytes. When endpoint size is greater then 64, FIFO warning interrupt
// is enabled to allow read incoming data during frame reception.
// It is possible to stay in interrupt reading whole packet at once, but it may be
// more efficient for MCU to read as much data as possible and when FIFO is hardly
// filled exit interrupt handler waiting for next FIFO warning level interrupt
// or packet end.
// When running at 96MHz code that reads FIFO based on number of bytes stored in
// USB_RXSx_REG.USB_RXCOUNT takes enough time to fill FIFO with two additional bytes.
// Settings this threshold above this allows to leave interrupt handler and wait
// for more bytes to before next ISR. This allows reduce overall ISR time to 1/3
// of time that would be needed if ISR read as fast as possible.
#define TU_DA1469X_FIFO_READ_THRESHOLD  4
#endif

#define EP_MAX            4

#define NFSR_NODE_RESET         0
#define NFSR_NODE_RESUME        1
#define NFSR_NODE_OPERATIONAL   2
#define NFSR_NODE_SUSPEND       3

static TU_ATTR_ALIGNED(4) uint8_t _setup_packet[8];

typedef struct
{
  union
  {
    __IOM uint32_t epc_in;
    __IOM uint32_t USB_EPC0_REG;                 /*!< (@ 0x00000080) Endpoint Control 0 Register  */
    __IOM uint32_t USB_EPC1_REG;                 /*!< (@ 0x000000A0) Endpoint Control Register 1  */
    __IOM uint32_t USB_EPC3_REG;                 /*!< (@ 0x000000C0) Endpoint Control Register 3  */
    __IOM uint32_t USB_EPC5_REG;                 /*!< (@ 0x000000E0) Endpoint Control Register 5  */
  };
  union
  {
    __IOM uint32_t txd;
    __IOM uint32_t USB_TXD0_REG;                 /*!< (@ 0x00000084) Transmit Data 0 Register     */
    __IOM uint32_t USB_TXD1_REG;                 /*!< (@ 0x000000A4) Transmit Data Register 1     */
    __IOM uint32_t USB_TXD2_REG;                 /*!< (@ 0x000000C4) Transmit Data Register 2     */
    __IOM uint32_t USB_TXD3_REG;                 /*!< (@ 0x000000E4) Transmit Data Register 3     */
  };
  union
  {
    __IOM uint32_t txs;
    __IOM uint32_t USB_TXS0_REG;                 /*!< (@ 0x00000088) Transmit Status 0 Register   */
    __IOM uint32_t USB_TXS1_REG;                 /*!< (@ 0x000000A8) Transmit Status Register 1   */
    __IOM uint32_t USB_TXS2_REG;                 /*!< (@ 0x000000C8) Transmit Status Register 2   */
    __IOM uint32_t USB_TXS3_REG;                 /*!< (@ 0x000000E8) Transmit Status Register 3   */
  };
  union
  {
    __IOM uint32_t txc;
    __IOM uint32_t USB_TXC0_REG;                 /*!< (@ 0x0000008C) Transmit command 0 Register  */
    __IOM uint32_t USB_TXC1_REG;                 /*!< (@ 0x000000AC) Transmit Command Register 1  */
    __IOM uint32_t USB_TXC2_REG;                 /*!< (@ 0x000000CC) Transmit Command Register 2  */
    __IOM uint32_t USB_TXC3_REG;                 /*!< (@ 0x000000EC) Transmit Command Register 3  */
  };
  union
  {
    __IOM uint32_t epc_out;
    __IOM uint32_t USB_EP0_NAK_REG;              /*!< (@ 0x00000090) EP0 INNAK and OUTNAK Register */
    __IOM uint32_t USB_EPC2_REG;                 /*!< (@ 0x000000B0) Endpoint Control Register 2   */
    __IOM uint32_t USB_EPC4_REG;                 /*!< (@ 0x000000D0) Endpoint Control Register 4   */
    __IOM uint32_t USB_EPC6_REG;                 /*!< (@ 0x000000F0) Endpoint Control Register 6   */
  };
  union
  {
    __IOM uint32_t rxd;
    __IOM uint32_t USB_RXD0_REG;                 /*!< (@ 0x00000094) Receive Data 0 Register       */
    __IOM uint32_t USB_RXD1_REG;                 /*!< (@ 0x000000B4) Receive Data Register,1       */
    __IOM uint32_t USB_RXD2_REG;                 /*!< (@ 0x000000D4) Receive Data Register 2       */
    __IOM uint32_t USB_RXD3_REG;                 /*!< (@ 0x000000F4) Receive Data Register 3       */
  };
  union
  {
    __IOM uint32_t rxs;
    __IOM uint32_t USB_RXS0_REG;                 /*!< (@ 0x00000098) Receive Status 0 Register     */
    __IOM uint32_t USB_RXS1_REG;                 /*!< (@ 0x000000B8) Receive Status Register 1     */
    __IOM uint32_t USB_RXS2_REG;                 /*!< (@ 0x000000D8) Receive Status Register 2     */
    __IOM uint32_t USB_RXS3_REG;                 /*!< (@ 0x000000F8) Receive Status Register 3     */
  };
  union
  {
    __IOM uint32_t rxc;
    __IOM uint32_t USB_RXC0_REG;                 /*!< (@ 0x0000009C) Receive Command 0 Register    */
    __IOM uint32_t USB_RXC1_REG;                 /*!< (@ 0x000000BC) Receive Command Register 1    */
    __IOM uint32_t USB_RXC2_REG;                 /*!< (@ 0x000000DC) Receive Command Register 2    */
    __IOM uint32_t USB_RXC3_REG;                 /*!< (@ 0x000000FC) Receive Command Register 3    */
  };
} EPx_REGS;

#define EP_REGS(first_ep_reg) (EPx_REGS*)(&USB->first_ep_reg)

// DMA channel pair to use, channel 6 will be used for RX channel 7 for TX direction.
#ifndef TU_DA146XX_DMA_RX_CHANNEL
#define TU_DA146XX_DMA_RX_CHANNEL 6
#endif
#define DA146XX_DMA_USB_MUX       (0x6 << (TU_DA146XX_DMA_RX_CHANNEL * 2))
#define DA146XX_DMA_USB_MUX_MASK  (0xF << (TU_DA146XX_DMA_RX_CHANNEL * 2))

typedef struct
{
  __IOM uint32_t DMAx_A_START_REG;
  __IOM uint32_t DMAx_B_START_REG;
  __IOM uint32_t DMAx_INT_REG;
  __IOM uint32_t DMAx_LEN_REG;
  __IOM uint32_t DMAx_CTRL_REG;
  __IOM uint32_t DMAx_IDX_REG;
  __IM uint32_t RESERVED[2]; // Extend structure size for array like usage, registers for each channel are 0x20 bytes apart.
} da146xx_dma_channel_t;

#define DMA_CHANNEL_REGS(n) ((da146xx_dma_channel_t *)(DMA) + n)
#define RX_DMA_REGS  DMA_CHANNEL_REGS(TU_DA146XX_DMA_RX_CHANNEL)
#define TX_DMA_REGS  DMA_CHANNEL_REGS((TU_DA146XX_DMA_RX_CHANNEL) + 1)

#define RX_DMA_START ((1 << DMA_DMA0_CTRL_REG_DMA_ON_Pos) |\
                      (0 << DMA_DMA0_CTRL_REG_BW_Pos) | \
                      (1 << DMA_DMA0_CTRL_REG_DREQ_MODE_Pos) | \
                      (1 << DMA_DMA0_CTRL_REG_BINC_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_AINC_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_CIRCULAR_Pos) | \
                      (2 << DMA_DMA0_CTRL_REG_DMA_PRIO_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_DMA_IDLE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_DMA_INIT_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_REQ_SENSE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_BURST_MODE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_BUS_ERROR_DETECT_Pos))

#define TX_DMA_START ((1 << DMA_DMA0_CTRL_REG_DMA_ON_Pos) |\
                      (0 << DMA_DMA0_CTRL_REG_BW_Pos) | \
                      (1 << DMA_DMA0_CTRL_REG_DREQ_MODE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_BINC_Pos) | \
                      (1 << DMA_DMA0_CTRL_REG_AINC_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_CIRCULAR_Pos) | \
                      (2 << DMA_DMA0_CTRL_REG_DMA_PRIO_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_DMA_IDLE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_DMA_INIT_Pos) | \
                      (1 << DMA_DMA0_CTRL_REG_REQ_SENSE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_BURST_MODE_Pos) | \
                      (0 << DMA_DMA0_CTRL_REG_BUS_ERROR_DETECT_Pos))

// Dialog register fields and bit mask are very long. Filed masks repeat register names.
// Those convenience macros are a way to reduce complexity of register modification lines.
#define GET_BIT(val, field) (val & field ## _Msk) >> field ## _Pos
#define REG_GET_BIT(reg, field) (USB->reg & USB_ ## reg ## _ ## field ## _Msk)
#define REG_SET_BIT(reg, field) USB->reg |= USB_ ## reg ## _ ## field ## _Msk
#define REG_CLR_BIT(reg, field) USB->reg &= ~USB_ ## reg ## _ ## field ## _Msk
#define REG_SET_VAL(reg, field, val) USB->reg = (USB->reg & ~USB_ ## reg ## _ ## field ## _Msk) | (val << USB_ ## reg ## _ ## field ## _Pos)

typedef struct {
  EPx_REGS * regs;
  uint8_t * buffer;
  // Total length of current transfer
  uint16_t total_len;
  // Bytes transferred so far
  uint16_t transferred;
  uint16_t max_packet_size;
  // Packet size sent or received so far. It is used to modify transferred field
  // after ACK is received or when filling ISO endpoint with size larger then
  // FIFO size.
  uint16_t last_packet_size;
  uint8_t ep_addr;
  // DATA0/1 toggle bit 1 DATA1 is expected or transmitted
  uint8_t data1 : 1;
  // Endpoint is stalled
  uint8_t stall : 1;
  // ISO endpoint
  uint8_t iso : 1;
} xfer_ctl_t;

static struct
{
  bool vbus_present;
  bool in_reset;
  xfer_ctl_t xfer_status[EP_MAX][2];
  // Endpoints that use DMA, one for each direction
  uint8_t dma_ep[2];
} _dcd =
{
  .vbus_present = false,
  .xfer_status =
  {
    { { .regs = EP_REGS(USB_EPC0_REG) }, { .regs = EP_REGS(USB_EPC0_REG) } },
    { { .regs = EP_REGS(USB_EPC1_REG) }, { .regs = EP_REGS(USB_EPC1_REG) } },
    { { .regs = EP_REGS(USB_EPC3_REG) }, { .regs = EP_REGS(USB_EPC3_REG) } },
    { { .regs = EP_REGS(USB_EPC5_REG) }, { .regs = EP_REGS(USB_EPC5_REG) } },
  }
};

// Two endpoint 0 descriptor definition for unified dcd_edpt_open()
static const tusb_desc_endpoint_t ep0OUT_desc =
{
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,

  .bEndpointAddress = 0x00,
  .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
  .bInterval        = 0
};

static const tusb_desc_endpoint_t ep0IN_desc =
{
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,

  .bEndpointAddress = 0x80,
  .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
  .bInterval        = 0
};

#define XFER_CTL_BASE(_ep, _dir) &_dcd.xfer_status[_ep][_dir]

// Function could be called when VBUS change was detected.
void tusb_vbus_changed(bool present)
{
  if (present != _dcd.vbus_present)
  {
    _dcd.vbus_present = present;
    if (present)
    {
      USB->USB_MCTRL_REG = USB_USB_MCTRL_REG_USBEN_Msk;
      USB->USB_NFSR_REG = 0;
      USB->USB_FAR_REG = 0x80;
      USB->USB_NFSR_REG = NFSR_NODE_RESET;
      USB->USB_TXMSK_REG = 0;
      USB->USB_RXMSK_REG = 0;

      USB->USB_MAMSK_REG = USB_USB_MAMSK_REG_USB_M_INTR_Msk |
                           USB_USB_MAMSK_REG_USB_M_ALT_Msk |
                           USB_USB_MAMSK_REG_USB_M_WARN_Msk;
      USB->USB_ALTMSK_REG = USB_USB_ALTMSK_REG_USB_M_RESET_Msk;
    }
    else
    {
      USB->USB_MCTRL_REG = 0;
    }
  }
}

static void fill_tx_fifo(xfer_ctl_t * xfer)
{
  int left_to_send;
  uint8_t const *src;
  EPx_REGS *regs = xfer->regs;
  uint8_t const epnum = tu_edpt_number(xfer->ep_addr);

  src = &xfer->buffer[xfer->transferred];
  left_to_send = xfer->total_len - xfer->transferred;
  if (left_to_send > xfer->max_packet_size - xfer->last_packet_size)
  {
    left_to_send = xfer->max_packet_size - xfer->last_packet_size;
  }

  // Loop checks TCOUNT all the time since this value is saturated to 31
  // and can't be read just once before.
  while ((regs->txs & USB_USB_TXS1_REG_USB_TCOUNT_Msk) > 0 && left_to_send > 0)
  {
    regs->txd = *src++;
    xfer->last_packet_size++;
    left_to_send--;
  }
  if (epnum != 0)
  {
    if (left_to_send > 0)
    {
      // Max packet size is set to value greater then FIFO. Enable fifo level warning
      // to handle larger packets.
      regs->txc |= (3 << USB_USB_TXC1_REG_USB_TFWL_Pos);
      USB->USB_FWMSK_REG |= 1 << (epnum - 1 + USB_USB_FWMSK_REG_USB_M_TXWARN31_Pos);
    }
    else
    {
      xfer->regs->txc &= ~USB_USB_TXC1_REG_USB_TFWL_Msk;
      USB->USB_FWMSK_REG &= ~(1 << (epnum - 1 + USB_USB_FWMSK_REG_USB_M_TXWARN31_Pos));
      // Whole packet already in fifo, no need to refill it later.  Mark last.
      regs->txc |= USB_USB_TXC1_REG_USB_LAST_Msk;
    }
  }
}

static bool try_allocate_dma(uint8_t epnum, uint8_t dir)
{
  // TODO: Disable interrupts while checking
  if (_dcd.dma_ep[dir] == 0)
  {
    _dcd.dma_ep[dir] = epnum;
    if (dir == TUSB_DIR_OUT)
      USB->USB_DMA_CTRL_REG = (USB->USB_DMA_CTRL_REG & ~USB_USB_DMA_CTRL_REG_USB_DMA_RX_Msk) |
        ((epnum - 1) << USB_USB_DMA_CTRL_REG_USB_DMA_RX_Pos);
    else
      USB->USB_DMA_CTRL_REG = (USB->USB_DMA_CTRL_REG & ~USB_USB_DMA_CTRL_REG_USB_DMA_TX_Msk) |
        ((epnum - 1) << USB_USB_DMA_CTRL_REG_USB_DMA_TX_Pos);
    USB->USB_DMA_CTRL_REG |= USB_USB_DMA_CTRL_REG_USB_DMA_EN_Msk;
  }
  return _dcd.dma_ep[dir] == epnum;
}

static void start_rx_dma(volatile void *src, void *dst, uint16_t size)
{
  // Setup SRC and DST registers
  RX_DMA_REGS->DMAx_A_START_REG = (uint32_t)src;
  RX_DMA_REGS->DMAx_B_START_REG = (uint32_t)dst;
  // Don't need DMA interrupt, read end is determined by RX_LAST or RX_ERR events.
  RX_DMA_REGS->DMAx_INT_REG = size - 1;
  RX_DMA_REGS->DMAx_LEN_REG = size - 1;
  RX_DMA_REGS->DMAx_CTRL_REG = RX_DMA_START;
}

static void start_rx_packet(xfer_ctl_t *xfer)
{
  uint8_t const epnum = tu_edpt_number(xfer->ep_addr);
  uint16_t remaining = xfer->total_len - xfer->transferred;
  uint16_t size = tu_min16(remaining, xfer->max_packet_size);

  xfer->last_packet_size = 0;
  if (xfer->max_packet_size > FIFO_SIZE && remaining > FIFO_SIZE)
  {
    if (try_allocate_dma(epnum, TUSB_DIR_OUT))
    {
      start_rx_dma(&xfer->regs->rxd, xfer->buffer + xfer->transferred, size);
    }
    else
    {
      // Other endpoint is using DMA in that direction, fall back to interrupts.
      // For endpoint size greater then FIFO size enable FIFO level warning interrupt
      // when FIFO has less then 17 bytes free.
      xfer->regs->rxc |= USB_USB_RXC1_REG_USB_RFWL_Msk;
      USB->USB_FWMSK_REG |= 1 << (epnum - 1 + USB_USB_FWMSK_REG_USB_M_RXWARN31_Pos);
    }
  }
  else if (epnum != 0)
  {
    // If max_packet_size would fit in FIFO no need for FIFO level warning interrupt.
    xfer->regs->rxc &= ~USB_USB_RXC1_REG_USB_RFWL_Msk;
    USB->USB_FWMSK_REG &= ~(1 << (epnum - 1 + USB_USB_FWMSK_REG_USB_M_RXWARN31_Pos));
  }
  xfer->regs->rxc |= USB_USB_RXC1_REG_USB_RX_EN_Msk;
}

static void start_tx_dma(void *src, volatile void *dst, uint16_t size)
{
  // Setup SRC and DST registers
  TX_DMA_REGS->DMAx_A_START_REG = (uint32_t)src;
  TX_DMA_REGS->DMAx_B_START_REG = (uint32_t)dst;
  // Interrupt not needed
  TX_DMA_REGS->DMAx_INT_REG = size;
  TX_DMA_REGS->DMAx_LEN_REG = size - 1;
  TX_DMA_REGS->DMAx_CTRL_REG = TX_DMA_START;
}

static void start_tx_packet(xfer_ctl_t *xfer)
{
  uint8_t const epnum = tu_edpt_number(xfer->ep_addr);
  uint16_t remaining = xfer->total_len - xfer->transferred;
  uint16_t size = tu_min16(remaining, xfer->max_packet_size);
  EPx_REGS *regs = xfer->regs;

  xfer->last_packet_size = 0;

  regs->txc = USB_USB_TXC1_REG_USB_FLUSH_Msk;
  regs->txc = USB_USB_TXC1_REG_USB_IGN_ISOMSK_Msk;
  if (xfer->data1) xfer->regs->txc |= USB_USB_TXC1_REG_USB_TOGGLE_TX_Msk;

  if (xfer->max_packet_size > FIFO_SIZE && remaining > FIFO_SIZE && try_allocate_dma(epnum, TUSB_DIR_IN))
  {
    // Whole packet will be put in FIFO by DMA. Set LAST bit before start.
    start_tx_dma(xfer->buffer + xfer->transferred, &regs->txd, size);
    regs->txc |= USB_USB_TXC1_REG_USB_LAST_Msk;
  }
  else
  {
    fill_tx_fifo(xfer);
  }
  regs->txc |= USB_USB_TXC1_REG_USB_TX_EN_Msk;
}

static void read_rx_fifo(xfer_ctl_t *xfer, uint16_t bytes_in_fifo)
{
  EPx_REGS *regs = xfer->regs;
  uint16_t remaining = xfer->total_len - xfer->transferred - xfer->last_packet_size;
  uint16_t receive_this_time = bytes_in_fifo;

  if (remaining < bytes_in_fifo) receive_this_time = remaining;

  uint8_t *buf = xfer->buffer + xfer->transferred + xfer->last_packet_size;

  for (int i = 0; i < receive_this_time; ++i) buf[i] = regs->rxd;

  xfer->last_packet_size += receive_this_time;
}

static void handle_ep0_rx(void)
{
  int fifo_bytes;
  uint32_t rxs0 = USB->USB_RXS0_REG;

  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_OUT);

  fifo_bytes = GET_BIT(rxs0, USB_USB_RXS0_REG_USB_RCOUNT);
  if (rxs0 & USB_USB_RXS0_REG_USB_SETUP_Msk)
  {
    xfer_ctl_t *xfer_in = XFER_CTL_BASE(0, TUSB_DIR_IN);
    // Setup packet is in
    for (int i = 0; i < fifo_bytes; ++i) _setup_packet[i] = USB->USB_RXD0_REG;

    xfer->stall = 0;
    xfer->data1 = 1;
    xfer_in->stall = 0;
    xfer_in->data1 = 1;
    REG_SET_BIT(USB_TXC0_REG, USB_TOGGLE_TX0);
    REG_CLR_BIT(USB_EPC0_REG, USB_STALL);
    dcd_event_setup_received(0, _setup_packet,true);
  }
  else
  {
    if (GET_BIT(rxs0, USB_USB_RXS0_REG_USB_TOGGLE_RX0) != xfer->data1)
    {
      // Toggle bit does not match discard packet
      REG_SET_BIT(USB_RXC0_REG, USB_FLUSH);
      xfer->last_packet_size = 0;
    }
    else
    {
      read_rx_fifo(xfer, fifo_bytes);
      if (rxs0 & USB_USB_RXS0_REG_USB_RX_LAST_Msk)
      {
        xfer->transferred += xfer->last_packet_size;
        xfer->data1 ^= 1;

        if (xfer->total_len == xfer->transferred || xfer->last_packet_size < xfer->max_packet_size)
        {
          dcd_event_xfer_complete(0, 0, xfer->transferred, XFER_RESULT_SUCCESS, true);
        }
        else
        {
          // Re-enable reception
          REG_SET_BIT(USB_RXC0_REG, USB_RX_EN);
        }
        xfer->last_packet_size = 0;
      }
    }
  }
}

static void handle_ep0_tx(void)
{
  uint32_t txs0;
  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
  EPx_REGS *regs = xfer->regs;

  txs0 = regs->USB_TXS0_REG;

  if (GET_BIT(txs0, USB_USB_TXS0_REG_USB_TX_DONE))
  {
    // ACK received
    if (GET_BIT(txs0, USB_USB_TXS0_REG_USB_ACK_STAT))
    {
      xfer->transferred += xfer->last_packet_size;
      xfer->last_packet_size = 0;
      xfer->data1 ^= 1;
      REG_SET_VAL(USB_TXC0_REG, USB_TOGGLE_TX0, xfer->data1);
      if (xfer->transferred == xfer->total_len)
      {
        dcd_event_xfer_complete(0, 0 | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        return;
      }
    }
    else
    {
      // Start from the beginning
      xfer->last_packet_size = 0;
    }
    fill_tx_fifo(xfer);
  }
}

static void handle_epx_rx_ev(uint8_t ep)
{
  uint32_t rxs;
  int fifo_bytes;
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep, TUSB_DIR_OUT);

  EPx_REGS *regs = xfer->regs;

  do
  {
    rxs = regs->rxs;

    if (GET_BIT(rxs, USB_USB_RXS1_REG_USB_RX_ERR))
    {
      regs->rxc |= USB_USB_RXC1_REG_USB_FLUSH_Msk;
      xfer->last_packet_size = 0;
      if (_dcd.dma_ep[TUSB_DIR_OUT] == ep)
      {
        // Stop DMA
        RX_DMA_REGS->DMAx_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        // Restart DMA since packet was dropped, all parameters should still work.
        RX_DMA_REGS->DMAx_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;
      }
      break;
    }
    else
    {
      if (_dcd.dma_ep[TUSB_DIR_OUT] == ep)
      {
        // Disable DMA and update last_packet_size with what DMA reported.
        RX_DMA_REGS->DMAx_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        xfer->last_packet_size = RX_DMA_REGS->DMAx_IDX_REG;
        // When DMA did not finished (packet was smaller then MPS), DMAx_IDX_REG holds exact number of bytes transmitted.
        // When DMA finished value in DMAx_IDX_REG is one less then actual number of transmitted bytes.
        if (xfer->last_packet_size == RX_DMA_REGS->DMAx_LEN_REG) xfer->last_packet_size++;
        // Release DMA to use by other endpoints.
        _dcd.dma_ep[TUSB_DIR_OUT] = 0;
      }
      fifo_bytes = GET_BIT(rxs, USB_USB_RXS1_REG_USB_RXCOUNT);
      // FIFO maybe empty if DMA read it before or it's final iteration and function already read all that was to read.
      if (fifo_bytes > 0)
      {
        read_rx_fifo(xfer, fifo_bytes);
      }
      if (GET_BIT(rxs, USB_USB_RXS1_REG_USB_RX_LAST))
      {
        if (!xfer->iso && GET_BIT(rxs, USB_USB_RXS1_REG_USB_TOGGLE_RX) != xfer->data1)
        {
          // Toggle bit does not match discard packet
          regs->rxc |= USB_USB_RXC1_REG_USB_FLUSH_Msk;
        }
        else
        {
          xfer->data1 ^= 1;
          xfer->transferred += xfer->last_packet_size;
          if (xfer->total_len == xfer->transferred || xfer->last_packet_size < xfer->max_packet_size || xfer->iso)
          {
            dcd_event_xfer_complete(0, xfer->ep_addr, xfer->transferred, XFER_RESULT_SUCCESS, true);
          }
          else
          {
            // Re-enable reception
            start_rx_packet(xfer);
          }
        }
        xfer->last_packet_size = 0;
      }
    }
  } while (fifo_bytes > TU_DA1469X_FIFO_READ_THRESHOLD);
}

static void handle_rx_ev(void)
{
  if (USB->USB_RXEV_REG & 1)
    handle_epx_rx_ev(1);
  if (USB->USB_RXEV_REG & 2)
    handle_epx_rx_ev(2);
  if (USB->USB_RXEV_REG & 4)
    handle_epx_rx_ev(3);
}

static void handle_epx_tx_ev(xfer_ctl_t *xfer)
{
  uint8_t const epnum = tu_edpt_number(xfer->ep_addr);
  uint32_t txs;
  EPx_REGS *regs = xfer->regs;

  txs = regs->txs;

  if (GET_BIT(txs, USB_USB_TXS1_REG_USB_TX_DONE))
  {
    if (_dcd.dma_ep[TUSB_DIR_IN] == epnum)
    {
      // Disable DMA and update last_packet_size with what DMA reported.
      TX_DMA_REGS->DMAx_CTRL_REG &= ~DMA_DMA1_CTRL_REG_DMA_ON_Msk;
      xfer->last_packet_size = TX_DMA_REGS->DMAx_IDX_REG + 1;
      // Release DMA to used by other endpoints.
      _dcd.dma_ep[TUSB_DIR_IN] = 0;
    }
    if (GET_BIT(txs, USB_USB_TXS1_REG_USB_ACK_STAT))
    {
      // ACK received, update transfer state and DATA0/1 bit
      xfer->transferred += xfer->last_packet_size;
      xfer->last_packet_size = 0;
      xfer->data1 ^= 1;

      if (xfer->transferred == xfer->total_len)
      {
        dcd_event_xfer_complete(0, xfer->ep_addr, xfer->total_len, XFER_RESULT_SUCCESS, true);
        return;
      }
    }
  }
  if (txs & USB_USB_TXS1_REG_USB_TX_URUN_Msk)
  {
    TU_LOG1("EP %d FIFO underrun\n", epnum);
  }
  // Start next or repeated packet.
  start_tx_packet(xfer);
}

static void handle_tx_ev(void)
{
  if (USB->USB_TXEV_REG & 1)
    handle_epx_tx_ev(XFER_CTL_BASE(1, TUSB_DIR_IN));
  if (USB->USB_TXEV_REG & 2)
    handle_epx_tx_ev(XFER_CTL_BASE(2, TUSB_DIR_IN));
  if (USB->USB_TXEV_REG & 4)
    handle_epx_tx_ev(XFER_CTL_BASE(3, TUSB_DIR_IN));
}

static void handle_bus_reset(void)
{
  USB->USB_NFSR_REG = 0;
  USB->USB_FAR_REG = 0x80;
  USB->USB_ALTMSK_REG = 0;
  USB->USB_NFSR_REG = NFSR_NODE_RESET;
  USB->USB_TXMSK_REG = 0;
  USB->USB_RXMSK_REG = 0;
  (void)USB->USB_ALTEV_REG;
  _dcd.in_reset = true;

  dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
  USB->USB_DMA_CTRL_REG = 0;

  USB->USB_MAMSK_REG = USB_USB_MAMSK_REG_USB_M_INTR_Msk |
#if USE_SOF
                       USB_USB_MAMSK_REG_USB_M_FRAME_Msk |
#endif
                       USB_USB_MAMSK_REG_USB_M_WARN_Msk |
                       USB_USB_MAMSK_REG_USB_M_ALT_Msk;
  USB->USB_NFSR_REG = NFSR_NODE_OPERATIONAL;
  USB->USB_ALTMSK_REG = USB_USB_ALTMSK_REG_USB_M_SD3_Msk |
                        USB_USB_ALTMSK_REG_USB_M_RESUME_Msk;
  // There is no information about end of reset state
  // USB_FRAME event will be used to enable reset detection again
  REG_SET_BIT(USB_MAEV_REG, USB_FRAME);
  dcd_edpt_open (0, &ep0OUT_desc);
  dcd_edpt_open (0, &ep0IN_desc);
}

static void handle_alt_ev(void)
{
  uint32_t alt_ev = USB->USB_ALTEV_REG;

  if (GET_BIT(alt_ev, USB_USB_ALTEV_REG_USB_RESET))
  {
    handle_bus_reset();
  }
  else
  {
    if (GET_BIT(alt_ev, USB_USB_ALTEV_REG_USB_RESUME))
    {
      USB->USB_NFSR_REG = NFSR_NODE_OPERATIONAL;
      USB->USB_ALTMSK_REG &= ~USB_USB_ALTMSK_REG_USB_M_RESUME_Msk;
      USB->USB_ALTMSK_REG |= USB_USB_ALTMSK_REG_USB_M_SD3_Msk;
      dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
    }
    if (GET_BIT(alt_ev, USB_USB_ALTEV_REG_USB_SD3))
    {
      USB->USB_NFSR_REG = NFSR_NODE_SUSPEND;
      USB->USB_ALTMSK_REG |= USB_USB_ALTMSK_REG_USB_M_RESUME_Msk;
      USB->USB_ALTMSK_REG &= ~USB_USB_ALTMSK_REG_USB_M_SD3_Msk | USB_USB_ALTMSK_REG_USB_M_SD5_Msk;
      dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
    }
  }
}

static void handle_epx_tx_warn_ev(uint8_t ep)
{
  fill_tx_fifo(XFER_CTL_BASE(ep, TUSB_DIR_IN));
}

static void handle_fifo_warning(void)
{
  uint32_t fifo_warning = USB->USB_FWEV_REG;

  if (fifo_warning & 0x01)
    handle_epx_tx_warn_ev(1);
  if (fifo_warning & 0x02)
    handle_epx_tx_warn_ev(2);
  if (fifo_warning & 0x04)
    handle_epx_tx_warn_ev(3);
  if (fifo_warning & 0x10)
    handle_epx_rx_ev(1);
  if (fifo_warning & 0x20)
    handle_epx_rx_ev(2);
  if (fifo_warning & 0x40)
    handle_epx_rx_ev(3);
}

static void handle_ep0_nak(void)
{
  uint32_t ep0_nak = USB->USB_EP0_NAK_REG;

  if (REG_GET_BIT(USB_EPC0_REG, USB_STALL))
  {
    if (GET_BIT(ep0_nak, USB_USB_EP0_NAK_REG_USB_EP0_INNAK))
    {
      // EP0 is stalled and NAK was sent, it means that RX is enabled
      // Disable RX for now.
      REG_CLR_BIT(USB_RXC0_REG, USB_RX_EN);
      REG_SET_BIT(USB_TXC0_REG, USB_TX_EN);
    }
    if (GET_BIT(ep0_nak, USB_USB_EP0_NAK_REG_USB_EP0_OUTNAK))
    {
      REG_SET_BIT(USB_RXC0_REG, USB_RX_EN);
    }
  }
  else
  {
    REG_CLR_BIT(USB_MAMSK_REG, USB_M_EP0_NAK);
  }
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init(uint8_t rhport)
{
  USB->USB_MCTRL_REG = USB_USB_MCTRL_REG_USBEN_Msk;
  tusb_vbus_changed((CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_VBUS_AVAILABLE_Msk) != 0);

  dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;

  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;

  NVIC_DisableIRQ(USB_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;

  // Set default address for one ZLP
  USB->USB_EPC0_REG = USB_USB_EPC0_REG_USB_DEF_Msk;
  USB->USB_FAR_REG = (dev_addr & USB_USB_FAR_REG_USB_AD_Msk) | USB_USB_FAR_REG_USB_AD_EN_Msk;
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void)rhport;

  REG_SET_BIT(USB_MCTRL_REG, USB_NAT);

  // Select chosen DMA to be triggered by USB.
  DMA->DMA_REQ_MUX_REG = (DMA->DMA_REQ_MUX_REG & ~DA146XX_DMA_USB_MUX_MASK) | DA146XX_DMA_USB_MUX;
}

void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;

  REG_CLR_BIT(USB_MCTRL_REG, USB_NAT);
}


/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void)rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);
  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  uint8_t iso_mask = 0;

  TU_ASSERT(epnum < EP_MAX);

  xfer->max_packet_size = desc_edpt->wMaxPacketSize.size;
  xfer->ep_addr = desc_edpt->bEndpointAddress;
  xfer->data1 = 0;
  xfer->iso = 0;

  if (epnum != 0 && desc_edpt->bmAttributes.xfer == 1)
  {
    iso_mask = USB_USB_EPC1_REG_USB_ISO_Msk;
    xfer->iso = 1;
  }

  if (epnum == 0)
  {
    USB->USB_MAMSK_REG |= USB_USB_MAMSK_REG_USB_M_EP0_RX_Msk |
                          USB_USB_MAMSK_REG_USB_M_EP0_TX_Msk;
  }
  else
  {
    if (dir == TUSB_DIR_OUT)
    {
      xfer->regs->epc_out = epnum | USB_USB_EPC1_REG_USB_EP_EN_Msk | iso_mask;
      USB->USB_RXMSK_REG |= 0x101 << (epnum - 1);
      REG_SET_BIT(USB_MAMSK_REG, USB_M_RX_EV);
    }
    else
    {
      xfer->regs->epc_in = epnum | USB_USB_EPC1_REG_USB_EP_EN_Msk | iso_mask;
      USB->USB_TXMSK_REG |= 0x101 << (epnum - 1);
      REG_SET_BIT(USB_MAMSK_REG, USB_M_TX_EV);
    }
  }

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);

  (void)rhport;

  TU_ASSERT(epnum < EP_MAX,);

  if (epnum == 0)
  {
    USB->USB_MAMSK_REG &= ~(USB_USB_MAMSK_REG_USB_M_EP0_RX_Msk |
                            USB_USB_MAMSK_REG_USB_M_EP0_TX_Msk);
  }
  else
  {
    if (dir == TUSB_DIR_OUT)
    {
      xfer->regs->rxc = USB_USB_RXC1_REG_USB_FLUSH_Msk;
      xfer->regs->epc_out = 0;
      USB->USB_RXMSK_REG &= ~(0x101 << (epnum - 1));
      // Release DMA if needed
      if (_dcd.dma_ep[TUSB_DIR_OUT] == epnum)
      {
        RX_DMA_REGS->DMAx_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        _dcd.dma_ep[TUSB_DIR_OUT] = 0;
      }
    }
    else
    {
      xfer->regs->txc = USB_USB_TXC1_REG_USB_FLUSH_Msk;
      xfer->regs->epc_in = 0;
      USB->USB_TXMSK_REG &= ~(0x101 << (epnum - 1));
      // Release DMA if needed
      if (_dcd.dma_ep[TUSB_DIR_IN] == epnum)
      {
        TX_DMA_REGS->DMAx_CTRL_REG &= ~DMA_DMA1_CTRL_REG_DMA_ON_Msk;
        _dcd.dma_ep[TUSB_DIR_IN] = 0;
      }
    }
  }
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);

  (void)rhport;

  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->last_packet_size = 0;
  xfer->transferred = 0;

  if (dir == TUSB_DIR_OUT)
  {
    start_rx_packet(xfer);
  }
  else // IN
  {
    start_tx_packet(xfer);
  }

  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  (void)rhport;

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->stall = 1;

  if (epnum == 0)
  {
    // EP0 has just one registers to control stall for IN and OUT
    REG_SET_BIT(USB_EPC0_REG, USB_STALL);
    if (dir == TUSB_DIR_OUT)
    {
      xfer->regs->USB_RXC0_REG = USB_USB_RXC0_REG_USB_RX_EN_Msk;
    }
    else
    {
      if (xfer->regs->USB_RXC0_REG & USB_USB_RXC0_REG_USB_RX_EN_Msk)
      {
        // If RX is also enabled TX will not be stalled since RX has
        // higher priority. Enable NAK interrupt to handle stall.
        REG_SET_BIT(USB_MAMSK_REG, USB_M_EP0_NAK);
      }
      else
      {
        xfer->regs->USB_TXC0_REG |= USB_USB_TXC0_REG_USB_TX_EN_Msk;
      }
    }
  }
  else
  {
    if (dir == TUSB_DIR_OUT)
    {
      xfer->regs->epc_out |= USB_USB_EPC1_REG_USB_STALL_Msk;
      xfer->regs->rxc |= USB_USB_RXC1_REG_USB_RX_EN_Msk;
    }
    else
    {
      xfer->regs->epc_in |= USB_USB_EPC1_REG_USB_STALL_Msk;
      xfer->regs->txc |= USB_USB_TXC1_REG_USB_TX_EN_Msk | USB_USB_TXC1_REG_USB_LAST_Msk;
    }
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  (void)rhport;

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);

  // Clear stall is called in response to Clear Feature ENDPOINT_HALT, reset toggle
  xfer->data1 = 0;
  xfer->stall = 0;

  if (dir == TUSB_DIR_OUT)
  {
    xfer->regs->epc_out &= ~USB_USB_EPC1_REG_USB_STALL_Msk;
  }
  else
  {
    xfer->regs->epc_in &= ~USB_USB_EPC1_REG_USB_STALL_Msk;
  }
  if (epnum == 0)
  {
    REG_CLR_BIT(USB_MAMSK_REG, USB_M_EP0_NAK);
  }
}

/*------------------------------------------------------------------*/
/* Interrupt Handler
 *------------------------------------------------------------------*/

void dcd_int_handler(uint8_t rhport)
{
  uint32_t int_status = USB->USB_MAEV_REG;

  (void)rhport;

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_WARN))
  {
    handle_fifo_warning();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_CH_EV))
  {
    // TODO: for now just clear interrupt
    (void)USB->USB_CHARGER_STAT_REG;
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_EP0_NAK))
  {
    handle_ep0_nak();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_EP0_RX))
  {
    handle_ep0_rx();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_EP0_TX))
  {
    handle_ep0_tx();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_RX_EV))
  {
    handle_rx_ev();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_NAK))
  {
    (void)USB->USB_NAKEV_REG;
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_FRAME))
  {
    if (_dcd.in_reset)
    {
      // Enable reset detection
      _dcd.in_reset = false;
      (void)USB->USB_ALTEV_REG;
    }
#if USE_SOF
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
#else
    // SOF was used to re-enable reset detection
    // No need to keep it enabled
    USB->USB_MAMSK_REG &= ~USB_USB_MAMSK_REG_USB_M_FRAME_Msk;
#endif
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_TX_EV))
  {
    handle_tx_ev();
  }

  if (GET_BIT(int_status, USB_USB_MAEV_REG_USB_ALT))
  {
    handle_alt_ev();
  }
}

#endif
