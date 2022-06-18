/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Portions Copyright (c) 2022 Travis Robinson (libusbdotnet@gmail.com)
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

#if (CFG_TUD_ENABLED && CFG_TUD_CH341)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "ch341_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//////////////////////////////////////////////////////////////////////////////
// CH341 defines directly from Linux host driver.  The CH341 works basically
// the same as a CH340G.  In-fact, they share the same driver on Windows.
// These are formatted and laid out much better than the older CH340.c so I'm
// using them even though the descriptors are from a CH340G.
// https://github.com/torvalds/linux/blob/master/drivers/usb/serial/ch341.c
//////////////////////////////////////////////////////////////////////////////

#define CH341_CLKRATE 48000000UL

/* flags for IO-Bits */
#define CH341_BIT_RTS (1 << 6)
#define CH341_BIT_DTR (1 << 5)

/******************************/
/* interrupt pipe definitions */
/******************************/
/* always 4 interrupt bytes */
/* first irq byte normally 0x08 */
/* second irq byte base 0x7d + below */
/* third irq byte base 0x94 + below */
/* fourth irq byte normally 0xee */

/* second interrupt byte */
#define CH341_MULT_STAT 0x04 /* multiple status since last interrupt event */

/* status returned in third interrupt answer byte, inverted in data
   from irq */
#define CH341_BIT_CTS 0x01
#define CH341_BIT_DSR 0x02
#define CH341_BIT_RI 0x04
#define CH341_BIT_DCD 0x08
#define CH341_BITS_MODEM_STAT 0x0f /* all bits */

#define CH341_REQ_READ_VERSION 0x5F
#define CH341_REQ_WRITE_REG 0x9A
#define CH341_REQ_READ_REG 0x95
#define CH341_REQ_SERIAL_INIT 0xA1
#define CH341_REQ_MODEM_CTRL 0xA4

#define CH341_REG_BREAK 0x05
#define CH341_REG_PRESCALER 0x12
#define CH341_REG_DIVISOR 0x13
#define CH341_REG_LCR 0x18
#define CH341_REG_LCR2 0x25

// undocumented register. init val:0
#define CH341_REG_0x0F 0x0F
// undocumented register.  init val:4
#define CH341_REG_0x2C 0x2C
// undocumented register. init val:0
#define CH341_REG_0x27 0x27

#define CH341_REG_MCR_MSR 0x06
#define CH341_REG_MCR_MSR2 0x07

#define CH341_NBREAK_BITS 0x01

#define CH341_LCR_ENABLE_RX 0x80
#define CH341_LCR_ENABLE_TX 0x40
#define CH341_LCR_MARK_SPACE 0x20
#define CH341_LCR_PAR_EVEN 0x10
#define CH341_LCR_ENABLE_PAR 0x08
#define CH341_LCR_STOP_BITS_2 0x04
#define CH341_LCR_CS8 0x03
#define CH341_LCR_CS7 0x02
#define CH341_LCR_CS6 0x01
#define CH341_LCR_CS5 0x00

// The CH340G stores it's data in 8 bit registers.
// REG_DIDX is used as our indicies to emulate these.
typedef enum
{
  REG_DIDX_BREAK,
  REG_DIDX_PRESCALER,
  REG_DIDX_DIVISOR,
  REG_DIDX_LCR,
  REG_DIDX_LCR2,
  REG_DIDX_MCR_MSR,
  REG_DIDX_MCR_MSR2,
  REG_DIDX_0x0F,
  REG_DIDX_0x2C,
  REG_DIDX_0x27,

  REG_DIDX_MAX,
}REG_DIDX;

typedef struct
{
  uint8_t itf_num;
  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  // There is no way to sense if the serial port is closed without using DTR/RTS.
  // This driver does not make any assumptions as to how the user might want to
  // use signals hence we do not do this.
  bool connected;
  bool line_coding_changed;
  bool line_state_changed;
  bool break_state_changed;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  char    wanted_char;
  ch341_line_coding_t line_coding;
  ch341_line_state_t line_state;

  // FIFO
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;
  tu_fifo_t txnotify_ff;

  uint8_t rx_ff_buf[CFG_TUD_CH341_FIFO_SIZE];
  uint8_t tx_ff_buf[CFG_TUD_CH341_FIFO_SIZE];
  uint8_t txnotify_ff_buf[CFG_TUD_CH341_EP_TXNOTIFY_MAX_PACKET];

#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex;
  osal_mutex_def_t tx_ff_mutex;
  osal_mutex_def_t txnotify_ff_mutex;
#endif

  // Endpoint 0 Transfer buffer
  // Note that we are NOT using CFG_TUD_ENDPOINT0_SIZE here!  This is because
  // this is merely a temporary buffer we use to send data to the host and
  // we never send more than 2 bytes.
  CFG_TUSB_MEM_ALIGN uint8_t ep0_in_buf[4];

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_CH341_EP_RX_MAX_PACKET];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_CH341_EP_TX_MAX_PACKET];

  uint8_t register_data[REG_DIDX_MAX];

}ch341d_interface_t;


#define ITF_MEM_RESET_SIZE   offsetof(ch341d_interface_t, wanted_char)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

// The CH341 cannot coexist with other interfaces so there can be only 1.
// This is because it sends vendor class control messages directly to the
// device asnd uses wIndex of the control packet to transfer vendor
// specific data.
CFG_TUSB_MEM_SECTION static ch341d_interface_t _ch341d_itf[1];


static const uint32_t ch341_known_baud_rates[] = {  50, 75, 100, 110, 150, 300, 600, 900, 1200, 1800, 2400, 3600, 4800, 9600, 14400, 19200, 28800, 33600, 38400, 56000, 57600, 76800, 115200, 128000, 153600, 230400, 460800, 921600, 1500000, 2000000};

static inline void ch341_decode_bit_rate(ch341d_interface_t *p_ch341)
{
  uint16_t baud_div = 0x100 - p_ch341->register_data[REG_DIDX_DIVISOR];
  uint8_t baud_fact = (p_ch341->register_data[REG_DIDX_PRESCALER] >> 2) & 0x01;
  uint8_t baud_ps = p_ch341->register_data[REG_DIDX_PRESCALER] & 0x03;
  uint32_t calc_bit_rate = (uint32_t)(CH341_CLKRATE / ((1 << (12 - 3 * baud_ps - baud_fact)) * baud_div));
  int index;

  for (index = 0; index < (int)((sizeof(ch341_known_baud_rates) / sizeof(uint32_t))); index++)
  {
    int max_diff = (int)((ch341_known_baud_rates[index] * 2) / 1000);
    if (calc_bit_rate >= ch341_known_baud_rates[index] - max_diff && calc_bit_rate <= ch341_known_baud_rates[index] + max_diff)
    {
      p_ch341->line_coding.bit_rate = ch341_known_baud_rates[index];
      return;
    }
    if (calc_bit_rate < ch341_known_baud_rates[index])
      break;
  }
  p_ch341->line_coding.bit_rate = calc_bit_rate;
}

static inline void ch341_decode_lcr(ch341d_interface_t *p_ch341)
{
  // decode rx/tx enable
  uint8_t lcr = p_ch341->register_data[REG_DIDX_LCR];
  p_ch341->line_coding.tx_en = (lcr & CH341_LCR_ENABLE_TX) ? true : false;
  p_ch341->line_coding.rx_en = (lcr & CH341_LCR_ENABLE_RX) ? true : false;

  // decode parity (0=None, 1=Odd, 2=Even, 3=Mark, 4=Space)
  if (lcr & CH341_LCR_ENABLE_PAR)
  {
    if ((lcr & (CH341_LCR_PAR_EVEN)) == 0)
      p_ch341->line_coding.parity = 1; // Odd
    else
      p_ch341->line_coding.parity = 2; // Even

    if ((lcr & (CH341_LCR_MARK_SPACE)))
      p_ch341->line_coding.parity+=2;
  }
  else
  {
    p_ch341->line_coding.parity = 0; // None
  }

  // decode stop bits (0=1 stop bit, 2=2 stop bits)
  p_ch341->line_coding.stop_bits = (lcr & CH341_LCR_STOP_BITS_2) ? 2 : 0;

  // decode data bits
  if ((lcr & 0x3) == CH341_LCR_CS5)
    p_ch341->line_coding.data_bits = 5;
  else if ((lcr & 0x3) == CH341_LCR_CS6)
    p_ch341->line_coding.data_bits = 6;
  else if ((lcr & 0x3) == CH341_LCR_CS7)
    p_ch341->line_coding.data_bits = 7;
  else
    p_ch341->line_coding.data_bits = 8;
}

static inline ch341_line_state_t ch341_decode_mcr(ch341d_interface_t *p_ch341, uint16_t mcr)
{
  p_ch341->register_data[REG_DIDX_MCR_MSR] = (p_ch341->register_data[REG_DIDX_MCR_MSR] & (~(CH341_BIT_DTR | CH341_BIT_RTS))) | (mcr & (CH341_BIT_DTR | CH341_BIT_RTS));

  ch341_line_state_t line_state = 0;
  if (mcr & CH341_BIT_DTR)
    line_state &= ~CH341_LINE_STATE_DTR_ACTIVE;
  else
    line_state |= CH341_LINE_STATE_DTR_ACTIVE;

  if (mcr & CH341_BIT_RTS)
    line_state &= ~CH341_LINE_STATE_RTS_ACTIVE;
  else
    line_state |= CH341_LINE_STATE_RTS_ACTIVE;

  return line_state;
}

static void ch341_write_regs(ch341d_interface_t *p_ch341, uint16_t wValue, uint16_t wIndex)
{
  int i;

  for (i = 0; i < 2; i++)
  {
    uint8_t reg = wValue & 0xFF;
    uint8_t regval = wIndex & 0xFF;
    switch (reg)
    {
      case CH341_REG_BREAK:
        p_ch341->register_data[REG_DIDX_BREAK] = regval;
        p_ch341->break_state_changed = true;
        break;
      case CH341_REG_DIVISOR:
        p_ch341->register_data[REG_DIDX_DIVISOR] = regval;
        p_ch341->line_coding_changed = true;
        break;
      case CH341_REG_LCR:
        p_ch341->register_data[REG_DIDX_LCR] = regval;
        p_ch341->line_coding_changed = true;
        break;
      case CH341_REG_LCR2:
        p_ch341->register_data[REG_DIDX_LCR2] = regval;
        break;
      case CH341_REG_PRESCALER:
        regval &= 0x7;
        p_ch341->register_data[REG_DIDX_PRESCALER] = regval;
        p_ch341->line_coding_changed = true;
        break;
      case CH341_REG_0x0F:
        p_ch341->register_data[REG_DIDX_0x0F] = regval;
        break;
      case CH341_REG_0x27:
        p_ch341->register_data[REG_DIDX_0x27] = regval;
        break;
      case CH341_REG_0x2C:
        p_ch341->register_data[REG_DIDX_0x2C] = regval;
        break;
      default:
        break;
      }

      wValue >>= 8;
      wIndex >>= 8;
  }
}

static void ch341_read_regs(ch341d_interface_t *p_ch341, uint8_t* data, uint16_t wValue)
{
  int i;

  for (i = 0; i < 2; i++)
  {
    uint8_t reg = wValue & 0xFF;
    switch (reg)
    {
    case CH341_REG_BREAK:
      data[i] = p_ch341->register_data[REG_DIDX_BREAK];
      break;
    case CH341_REG_DIVISOR:
      data[i] = p_ch341->register_data[REG_DIDX_DIVISOR];
      break;
    case CH341_REG_LCR:
      data[i] = p_ch341->register_data[REG_DIDX_LCR];
      break;
    case CH341_REG_LCR2:
      data[i] = p_ch341->register_data[REG_DIDX_LCR2];
      break;
    case CH341_REG_PRESCALER:
      data[i] = p_ch341->register_data[REG_DIDX_PRESCALER];
      break;
    case CH341_REG_0x0F:
      data[i] = p_ch341->register_data[REG_DIDX_0x0F];
      break;
    case CH341_REG_0x27:
      data[i] = p_ch341->register_data[REG_DIDX_0x27];
      break;
    case CH341_REG_0x2C:
      data[i] = p_ch341->register_data[REG_DIDX_0x2C];
      break;
    case CH341_REG_MCR_MSR:
      data[i] = p_ch341->register_data[REG_DIDX_MCR_MSR];
      break;
    case CH341_REG_MCR_MSR2:
      data[i] = p_ch341->register_data[REG_DIDX_MCR_MSR2];
      break;
    default:
      break;
    }
    wValue >>= 8;
  }
}

static void _prep_out_transaction (ch341d_interface_t* p_ch341)
{
  uint8_t const rhport = BOARD_TUD_RHPORT;
  uint16_t available = tu_fifo_remaining(&p_ch341->rx_ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  TU_VERIFY(available >= sizeof(p_ch341->epout_buf), );

  // claim endpoint
  TU_VERIFY(usbd_edpt_claim(rhport, p_ch341->ep_out), );

  // fifo can be changed before endpoint is claimed
  available = tu_fifo_remaining(&p_ch341->rx_ff);

  if ( available >= sizeof(p_ch341->epout_buf) )
  {
    usbd_edpt_xfer(rhport, p_ch341->ep_out, p_ch341->epout_buf, sizeof(p_ch341->epout_buf));
  }
	else
  {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(rhport, p_ch341->ep_out);
  }
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_ch341_connected(void)
{
  return tud_ready() && _ch341d_itf[0].connected;
}

ch341_line_state_t tud_ch341_get_line_state (void)
{
  return _ch341d_itf[0].line_state;
}

uint32_t tud_ch341_set_modem_state(ch341_modem_state_t modem_states)
{
  ch341d_interface_t *p_ch341 = &_ch341d_itf[0];
  uint8_t buffer[4];

  modem_states &= CH341_MODEM_STATE_ALL;
	
  // FIXME?  I beleive these signals are all active=low.  I can only test the CTS line with
  // my CH340G breakout board and I know that with nothing connected or the CTS line shorted
  // to positive, the value transferred is 0xF (all 1's). With the CTS line shorted to ground,
  // the value is 0xE (all 1's except CTS)
  modem_states = (~modem_states) & CH341_MODEM_STATE_ALL;
	
  p_ch341->register_data[REG_DIDX_MCR_MSR] = (p_ch341->register_data[REG_DIDX_MCR_MSR] & 0xF0) | (modem_states);
  return 1;

  buffer[0] = 0x08;
	
	// This is sometimes 0x3F and other times 0xBF but I beleieve that's because when I short
  // CTS to ground there is no de-bounce so it flickers.
  buffer[1] = 0x3F;
  
	buffer[2] = 0x90 | modem_states;
	
	// Register MCR_MSR2 is presumably for future use as it's always 0xEE but I beleive it to be
  // for additional mcr/msr status.
  buffer[3] = p_ch341->register_data[REG_DIDX_MCR_MSR2];

  // write fifo
  uint16_t ret = tu_fifo_write_n(&p_ch341->txnotify_ff, buffer, 4);

  if (ret == 4)
  {
    // start transfer
    ret = tud_ch341_notify_flush();
  }
  return ret;
}

ch341_modem_state_t tud_ch341_get_modem_state(void)
{
  ch341d_interface_t *p_ch341 = &_ch341d_itf[0];
  ch341_modem_state_t modem_states = p_ch341->register_data[REG_DIDX_MCR_MSR];
  modem_states = (~modem_states) & CH341_MODEM_STATE_ALL;

  return modem_states;
}

void tud_ch341_get_line_coding (ch341_line_coding_t* coding)
{
  (*coding) = _ch341d_itf[0].line_coding;
}

void tud_ch341_set_wanted_char (char wanted)
{
  _ch341d_itf[0].wanted_char = wanted;
}


//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_ch341_available(void)
{
  return tu_fifo_count(&_ch341d_itf[0].rx_ff);
}

uint32_t tud_ch341_read(void* buffer, uint32_t bufsize)
{
  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];
  uint32_t num_read = tu_fifo_read_n(&p_ch341->rx_ff, buffer, bufsize);
  _prep_out_transaction(p_ch341);
  return num_read;
}

bool tud_ch341_peek(uint8_t* chr)
{
  return tu_fifo_peek(&_ch341d_itf[0].rx_ff, chr);
}

void tud_ch341_read_flush (void)
{
  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];
  tu_fifo_clear(&p_ch341->rx_ff);
  _prep_out_transaction(p_ch341);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_ch341_write(void const* buffer, uint32_t bufsize)
{
  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];
  uint16_t ret = tu_fifo_write_n(&p_ch341->tx_ff, buffer, bufsize);

  // flush if queue more than packet size
  if (tu_fifo_count(&p_ch341->tx_ff) >= sizeof(p_ch341->epin_buf))
  {
    tud_ch341_write_flush();
  }

  return ret;
}

uint32_t tud_ch341_write_flush (void)
{
  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];

  // Skip if usb is not ready yet
  TU_VERIFY( tud_ready(), 0 );

  // No data to send
  if ( !tu_fifo_count(&p_ch341->tx_ff) ) return 0;

  uint8_t const rhport = BOARD_TUD_RHPORT;

  // Claim the endpoint
  TU_VERIFY( usbd_edpt_claim(rhport, p_ch341->ep_in), 0 );

  // Pull data from FIFO
  uint16_t const count = tu_fifo_read_n(&p_ch341->tx_ff, p_ch341->epin_buf, sizeof(p_ch341->epin_buf));

  if ( count )
  {
    TU_ASSERT( usbd_edpt_xfer(rhport, p_ch341->ep_in, p_ch341->epin_buf, count), 0 );
    return count;
  }else
  {
    // Release endpoint since we don't make any transfer
    // Note: data is dropped if terminal is not connected
    usbd_edpt_release(rhport, p_ch341->ep_in);
    return 0;
  }
}
uint32_t tud_ch341_notify_flush(void)
{
  ch341d_interface_t *p_ch341 = &_ch341d_itf[0];

  // Skip if usb is not ready yet
  TU_VERIFY(tud_ready(), 0);

  // No data to send
  if (!tu_fifo_count(&p_ch341->txnotify_ff))
    return 0;

  uint8_t const rhport = BOARD_TUD_RHPORT;

  // Claim the endpoint
  TU_VERIFY(usbd_edpt_claim(rhport, p_ch341->ep_notif), 0);

  // Pull data from FIFO
  uint16_t const count = tu_fifo_read_n(&p_ch341->txnotify_ff, p_ch341->txnotify_ff_buf, sizeof(p_ch341->txnotify_ff_buf));

  if (count)
  {
    TU_ASSERT(usbd_edpt_xfer(rhport, p_ch341->ep_notif, p_ch341->txnotify_ff_buf, count), 0);
    return count;
  }
  else
  {
    // Release endpoint since we don't make any transfer
    // Note: data is dropped if terminal is not connected
    usbd_edpt_release(rhport, p_ch341->ep_notif);
    return 0;
  }
}

uint32_t tud_ch341_write_available (void)
{
  return tu_fifo_remaining(&_ch341d_itf[0].tx_ff);
}

bool tud_ch341_write_clear (void)
{
  return tu_fifo_clear(&_ch341d_itf[0].tx_ff);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void ch341d_init(void)
{
		tu_memclr(_ch341d_itf, sizeof(_ch341d_itf));

    ch341d_interface_t* p_ch341 = &_ch341d_itf[0];

    p_ch341->wanted_char = -1;

    // default line coding is : stop bit = 1, parity = none, data bits = 8
    p_ch341->line_coding.bit_rate  = 115200;
    p_ch341->line_coding.stop_bits = 0;
    p_ch341->line_coding.parity    = 0;
    p_ch341->line_coding.data_bits = 8;
    p_ch341->line_coding.rx_en = false;
    p_ch341->line_coding.tx_en = false;

    p_ch341->line_state = 0;

    p_ch341->register_data[REG_DIDX_0x0F] = 0xD2;
    p_ch341->register_data[REG_DIDX_0x27] = 0x00;
    p_ch341->register_data[REG_DIDX_0x2C] = 0x0B;
    p_ch341->register_data[REG_DIDX_BREAK] = 0xBF;
    p_ch341->register_data[REG_DIDX_DIVISOR] = 0xCC;
    p_ch341->register_data[REG_DIDX_LCR] = 0xC3;
    p_ch341->register_data[REG_DIDX_LCR2] = 0x00;
    p_ch341->register_data[REG_DIDX_PRESCALER] = 0x03;
    p_ch341->register_data[REG_DIDX_MCR_MSR] = 0xFF;
    p_ch341->register_data[REG_DIDX_MCR_MSR2] = 0xEE;

    // Config RX fifo
    tu_fifo_config(&p_ch341->rx_ff, p_ch341->rx_ff_buf, TU_ARRAY_SIZE(p_ch341->rx_ff_buf), 1, false);
    // Config TX fifo
    tu_fifo_config(&p_ch341->tx_ff, p_ch341->tx_ff_buf, TU_ARRAY_SIZE(p_ch341->tx_ff_buf), 1, false);
    // Config TX NOTIFY fifo
    tu_fifo_config(&p_ch341->txnotify_ff, p_ch341->txnotify_ff_buf, TU_ARRAY_SIZE(p_ch341->txnotify_ff_buf), 1, false);

#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&p_ch341->rx_ff, NULL, osal_mutex_create(&p_ch341->rx_ff_mutex));
    tu_fifo_config_mutex(&p_ch341->tx_ff, osal_mutex_create(&p_ch341->tx_ff_mutex), NULL);
    tu_fifo_config_mutex(&p_ch341->txnotify_ff, osal_mutex_create(&p_ch341->txnotify_ff_mutex), NULL);
#endif
}

void ch341d_reset(uint8_t rhport)
{
  (void) rhport;

	ch341d_interface_t* p_ch341 = &_ch341d_itf[0];

	tu_memclr(p_ch341, ITF_MEM_RESET_SIZE);
	tu_fifo_clear(&p_ch341->rx_ff);
	tu_fifo_clear(&p_ch341->tx_ff);
	tu_fifo_clear(&p_ch341->txnotify_ff);
	tu_fifo_set_overwritable(&p_ch341->tx_ff, false);
	tu_fifo_set_overwritable(&p_ch341->txnotify_ff, false);
}

uint16_t ch341d_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  // CH340G interface class, subclass, and protocol
  TU_VERIFY(0xFF == itf_desc->bInterfaceClass &&
              0x01 == itf_desc->bInterfaceSubClass &&
              0x02 == itf_desc->bInterfaceProtocol, 0);

  ch341d_interface_t * p_ch341 = &_ch341d_itf[0];

  //------------- CH341 Interface -------------//
  p_ch341->itf_num = itf_desc->bInterfaceNumber;

  uint16_t drv_len = sizeof(tusb_desc_interface_t);
  uint8_t const * p_desc = tu_desc_next( itf_desc );
  while (p_desc && drv_len < max_len)
  {
    if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc))
    {
      tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
      if (desc_ep->bmAttributes.xfer == TUSB_XFER_BULK)
      {
        if (p_ch341->ep_in == 0)
        {
          // Open endpoint pair
          TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &p_ch341->ep_out, &p_ch341->ep_in), 0);
          drv_len += tu_desc_len(p_desc);
          p_desc = tu_desc_next(p_desc);
        }
      }
      else if (desc_ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT)
      {
        if (p_ch341->ep_notif == 0)
        {
          // Open notification endpoint
          TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
          p_ch341->ep_notif = desc_ep->bEndpointAddress;
        }
      }
    }
    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  // Prepare for incoming data
  _prep_out_transaction(p_ch341);

  return drv_len;
}

// The CH341 driver sends vendor requests only.  we will pipe these into the ch341d_control_xfer_cb so it can handle everything
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  return ch341d_control_xfer_cb(rhport, stage, request);
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool ch341d_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{

  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];

  if ((request->bmRequestType_bit.type) == TUSB_REQ_TYPE_VENDOR && ((request->bmRequestType_bit.recipient) == TUSB_REQ_RCPT_DEVICE))
  {
    switch(request->bRequest)
    {
    case CH341_REQ_READ_VERSION:  // (0x5F)
      if (stage == CONTROL_STAGE_SETUP)
      {
        p_ch341->ep0_in_buf[0] = 0x31;
        p_ch341->ep0_in_buf[1] = 0;
        tud_control_xfer(rhport, request, p_ch341->ep0_in_buf, 2);
      }
      break;

    case CH341_REQ_READ_REG: // (0x95)
      if (stage == CONTROL_STAGE_SETUP)
      {
        ch341_read_regs(p_ch341, p_ch341->ep0_in_buf, request->wValue);
        tud_control_xfer(rhport, request, p_ch341->ep0_in_buf, 2);
      }
      break;

      case CH341_REQ_WRITE_REG:   // (0x9A)
      if (stage == CONTROL_STAGE_SETUP)
      {
        tud_control_status(rhport, request);
      }
      else if (stage == CONTROL_STAGE_ACK)
      {
        ch341_write_regs(p_ch341, request->wValue, request->wIndex);
      }
      break;


    case CH341_REQ_SERIAL_INIT: // (0xA1)
      if (stage == CONTROL_STAGE_SETUP)
      {
        tud_control_status(rhport, request);
      }
      else if (stage == CONTROL_STAGE_ACK)
      {
        // wValue = LCR/LCR2
        // wIndex = BAUDDIV/PRESCALAR
        if (request->wValue && request->wIndex)
        {
          ch341_write_regs(p_ch341, CH341_REG_LCR << 8 | CH341_REG_LCR2, request->wValue);
          ch341_write_regs(p_ch341, CH341_REG_DIVISOR << 8 | CH341_REG_PRESCALER, request->wIndex);
          p_ch341->line_state_changed = true;
          p_ch341->connected = true;
        }
      }
      break;

    case CH341_REQ_MODEM_CTRL:  // (0xA4)
      if (stage == CONTROL_STAGE_SETUP)
      {
        tud_control_status(rhport, request);
      }
      else if (stage == CONTROL_STAGE_ACK)
      {
          p_ch341->line_state = ch341_decode_mcr(p_ch341, request->wValue);
          p_ch341->line_state_changed = true;
      }
      break;

    default:
      return false;
    }

    if (p_ch341->line_state_changed)
    {
      p_ch341->line_state_changed = false;
      if (tud_ch341_line_state_cb)
        tud_ch341_line_state_cb(p_ch341->line_state);
    }
    if (p_ch341->line_coding_changed)
    {
      p_ch341->line_coding_changed = false;
      ch341_decode_bit_rate(p_ch341);
      ch341_decode_lcr(p_ch341);
      if (tud_ch341_line_coding_cb)
        tud_ch341_line_coding_cb(&p_ch341->line_coding);
    }
    if (p_ch341->break_state_changed)
    {
      p_ch341->break_state_changed = false;
      if (tud_ch341_send_break_cb)
        tud_ch341_send_break_cb(p_ch341->register_data[REG_DIDX_BREAK]);
    }

    return true;
  }
   return false;
}

bool ch341d_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  ch341d_interface_t* p_ch341 = &_ch341d_itf[0];

  // Received new data
  if ( ep_addr == p_ch341->ep_out )
  {
    tu_fifo_write_n(&p_ch341->rx_ff, &p_ch341->epout_buf, xferred_bytes);

    // Check for wanted char and invoke callback if needed
    if ( tud_ch341_rx_wanted_cb && (((signed char) p_ch341->wanted_char) != -1) )
    {
      for ( uint32_t i = 0; i < xferred_bytes; i++ )
      {
        if ( (p_ch341->wanted_char == p_ch341->epout_buf[i]) && !tu_fifo_empty(&p_ch341->rx_ff) )
        {
          tud_ch341_rx_wanted_cb(p_ch341->wanted_char);
        }
      }
    }

    // invoke receive callback (if there is still data)
    if (tud_ch341_rx_cb && !tu_fifo_empty(&p_ch341->rx_ff) ) tud_ch341_rx_cb();

    // prepare for OUT transaction
    _prep_out_transaction(p_ch341);
  }

  // Data sent to host, we continue to fetch from tx fifo to send.
  // Note: This will cause incorrect baudrate set in line coding.
  //       Though maybe the baudrate is not really important !!!
  if ( ep_addr == p_ch341->ep_in )
  {
    // invoke transmit callback to possibly refill tx fifo
    if ( tud_ch341_tx_complete_cb ) tud_ch341_tx_complete_cb();

    if ( 0 == tud_ch341_write_flush() )
    {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP Packet size and not zero
      if (!tu_fifo_count(&p_ch341->tx_ff) && xferred_bytes && (0 == (xferred_bytes & (CFG_TUD_CH341_EP_TX_MAX_PACKET-1))) )
      {
        if ( usbd_edpt_claim(rhport, p_ch341->ep_in) )
        {
          usbd_edpt_xfer(rhport, p_ch341->ep_in, NULL, 0);
        }
      }
    }
  }

  return true;
}

#endif
