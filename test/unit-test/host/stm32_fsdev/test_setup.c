// SPDX-License-Identifier: MIT
// EP0 SETUP handling of the fsdev device driver on a RAM register block and PMA.
// Models only what the SIE leaves behind for a received packet (PMA data, COUNT_RX, CTR_RX, SETUP),
// and drives the CTR_RX branch of dcd_int_handler() directly since ISTR is not modelled.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "portable/st/stm32_fsdev/fsdev_common.c"
#include "portable/st/stm32_fsdev/dcd_stm32_fsdev.c"

uint32_t test_usb_regs[0x60 / 4];
uint32_t test_pma[1024 / 4];

static unsigned setup_events, xfer_events;
static uint16_t max_hwfifo_read; // largest copy out of the PMA the driver may ask for

void dcd_event_handler(dcd_event_t const *event, bool in_isr) {
  (void) in_isr;
  if (event->event_id == DCD_EVENT_SETUP_RECEIVED) {
    setup_events++;
  } else if (event->event_id == DCD_EVENT_XFER_COMPLETE) {
    xfer_events++;
  }
}

void tu_hwfifo_read(const volatile void *hwfifo, uint8_t *dest, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void) access_mode;
  assert(len <= max_hwfifo_read);
  memcpy(dest, (const void *) (uintptr_t) hwfifo, len);
}

void tu_hwfifo_write(volatile void *hwfifo, const uint8_t *src, uint16_t len, const tu_hwfifo_access_t *access_mode) {
  (void) access_mode;
  memcpy((void *) (uintptr_t) hwfifo, src, len);
}

// Only endpoints transferring through a tu_fifo use these; EP0 never does
uint16_t tu_fifo_read_n_access_mode(tu_fifo_t *f, void *buffer, uint16_t n, const tu_hwfifo_access_t *access_mode) {
  (void) f;
  (void) buffer;
  (void) access_mode;
  assert(false);
  return n;
}

uint16_t tu_fifo_write_n_access_mode(tu_fifo_t *f, const void *data, uint16_t n,
                                     const tu_hwfifo_access_t *access_mode) {
  (void) f;
  (void) data;
  (void) access_mode;
  assert(false);
  return n;
}

// Bytes the EP0 RX buffer accepts, from BL_SIZE/NUM_BLOCK of COUNT0_RX
static uint16_t ep0_rx_capacity(void) {
  const uint16_t count     = FSDEV_BTABLE->ep16[0][BTABLE_BUF_RX].count;
  const uint16_t num_block = (count >> 10) & 0x1Fu;
  return (count & 0x8000u) ? (uint16_t) ((num_block + 1u) * 32u) : (uint16_t) (num_block * 2u);
}

// A packet the SIE stored in the EP0 RX buffer, as SETUP or OUT
static void ep0_receive(const uint8_t *data, uint16_t len, bool setup) {
  assert(len <= ep0_rx_capacity()); // the hardware does not store more than the buffer takes
  const uint16_t addr = (uint16_t) btable_get_addr(0, BTABLE_BUF_RX);
  if (len > 0) {
    memcpy((void *) (uintptr_t) PMA_BUF_AT(addr), data, len);
  }
  const uint16_t count                       = FSDEV_BTABLE->ep16[0][BTABLE_BUF_RX].count;
  FSDEV_BTABLE->ep16[0][BTABLE_BUF_RX].count = (uint16_t) ((count & ~0x3FFu) | len);
  FSDEV_REG->ep[0].reg = (fsdev_bus_t) (FSDEV_REG->ep[0].reg | U_EP_CTR_RX | (setup ? U_EP_SETUP : 0u));

  if (setup) {
    handle_ctr_setup(0);
  } else {
    ep_write_clear_ctr(0, TUSB_DIR_OUT);
    handle_ctr_rx(0);
  }
}

static const uint8_t get_descriptor[8] = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00};

int main(void) {
  max_hwfifo_read = CFG_TUD_ENDPOINT0_SIZE;
  dcd_init(0, NULL);
  assert(ep0_rx_capacity() == 8);

  // control read: SETUP, then the status OUT once the data IN is done
  max_hwfifo_read = 8;
  ep0_receive(get_descriptor, sizeof(get_descriptor), true);
  assert(setup_events == 1);

  max_hwfifo_read = CFG_TUD_ENDPOINT0_SIZE;
  assert(dcd_edpt_xfer(0, 0x00, NULL, 0, true));
  ep0_receive(NULL, 0, false);
  assert(xfer_events == 1);

  // EP0 RX now takes a full packet until the next SETUP: a SETUP data packet longer than 8 bytes is stored
  assert(ep0_rx_capacity() == CFG_TUD_ENDPOINT0_SIZE);
  uint8_t long_setup[CFG_TUD_ENDPOINT0_SIZE];
  memset(long_setup, 0x5a, sizeof(long_setup));
  memcpy(long_setup, get_descriptor, sizeof(get_descriptor));

  // at most the 8 bytes of a request may be copied to the driver's setup buffer, and the packet is dropped
  max_hwfifo_read = 8;
  ep0_receive(long_setup, sizeof(long_setup), true);
  assert(setup_events == 1);
  assert(ep0_rx_capacity() == 8);

  // the next well-formed SETUP is handled
  ep0_receive(get_descriptor, sizeof(get_descriptor), true);
  assert(setup_events == 2);

  printf("stm32_fsdev setup: ok\n");
  return 0;
}
