/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   manufacturer: WCH
*/

#include "debug_uart.h"
#include "CH56x_common.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Prototypes for the (weak) vector-table handlers overridden here
void USBSS_IRQHandler(void);
void LINK_IRQHandler(void);
void USBHS_IRQHandler(void);
void SysTick_Handler(void);

// Single rhport 0: SPEED=super compiles the USB3 dcd (USBSS + LINK vectors); SPEED=high the
// USB2 dcd (USBHS vector); the active dcd's dcd_int_handler dispatches on its own flag
// registers, so all three vectors share ONE forwarder. Aliases (not three identical
// functions) are required: gcc's identical-code-folding would otherwise rewrite one interrupt
// handler as a plain call into another, whose mret then skips the caller's epilogue and leaks
// its stack frame on every interrupt. Use the standard riscv interrupt attribute - WCH's
// "WCH-Interrupt-fast" is only understood by WCH's own gcc fork.
__attribute__((interrupt)) void USBSS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void LINK_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
void USBHS_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));

#if CFG_TUD_WCH_USB30_FALLBACK
// the USB3 dcd uses TMR0 as the SuperSpeed-training timeout for the USB2 fallback
void TMR0_IRQHandler(void);
void TMR0_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
#endif

//--------------------------------------------------------------------+
// SysTick
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick_Handler(void) {
  SysTick->CNTFG = 0; // clear count-reached flag
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+

void board_init(void) {
  // NOTE: the NOLOAD .dmadata (RAMX) section is zeroed before main by the EVT startup code
  // (hw/mcu/wch/ch569 EVT/EXAM/SRC/Startup/startup_CH56x.S, "Clear dmadata section"), so no
  // clear is needed here - repeating it would also wipe any .noinit-style RAMX state.

  // 120 MHz system clock: required for USB3 operation (160 MHz does not work with USB3).
  // SystemInit() takes the frequency in Hz (FREQ_SYS is set by the build)
  SystemInit(FREQ_SYS);

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(GetSysClock() / 1000);
#endif

  // UART1 (TXD1 = PA8, RXD1 = PA7) for debug output
#ifdef CFG_BOARD_UART_BAUDRATE
  usart_printf_init(CFG_BOARD_UART_BAUDRATE);
#endif

  // LED
  GPIOB_ResetBits(LED_PIN);
  GPIOB_ModeCfg(LED_PIN, GPIO_Highspeed_PP_8mA);

  // Button: internal pull to the inactive level for a defined idle state
#ifdef BUTTON_PIN
  GPIOB_ModeCfg(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_ModeIN_PD_NSMT : GPIO_ModeIN_PU_NSMT);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  if (state ^ LED_STATE_ON) {
    GPIOB_ResetBits(LED_PIN);
  } else {
    GPIOB_SetBits(LED_PIN);
  }
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == (GPIOB_ReadPortPin(BUTTON_PIN) ? 1 : 0);
#else
  return 0;
#endif
}

// SPI-ROM controller byte/word ports: the EVT SFR header only names the 16/32-bit views
#define SPI_ROM_CR8    (*(volatile uint8_t*) 0x4000101A)
#define SPI_ROM_CTRL8  (*(volatile uint8_t*) 0x40001018)
#define SPI_ROM_DATA8  (*(volatile uint8_t*) 0x40001018)
#define SPI_ROM_DATA32 (*(volatile uint32_t*) 0x40001014)

// Read 32-bit words from the code flash / info region through the SPI-ROM command
// interface (the memory-mapped path does not cover the info region); rom_addr = flash_addr
// + 0x8000. Each SPI_ROM_DATA8 access strobes the next byte out of the SPI NOR: fast-read
// needs 2 dummy strobes after the address, then 4 strobes per 32-bit word.
// Bounded wait for the SPI-ROM controller to accept/produce a byte. Every wait here is bounded:
// this runs inside GET_DESCRIPTOR(STRING) (board_usb_get_serial), not once at boot, and this BSP
// arms no watchdog - so a stuck status bit would hang the core mid-control-transfer until someone
// power-cycles the board. The iteration budget is far above any real transfer.
static bool flash_rom_wait_ready(void) {
  uint32_t timeout = 100000;
  while (((int8_t)SPI_ROM_CR8 < 0) && --timeout) {}
  return timeout != 0;
}

static bool flash_rom_read_words(uint32_t addr, uint32_t* buf, uint32_t nwords) {
  const uint32_t rom_addr = addr + 0x8000u;
  SPI_ROM_CR8 = 0;
  SPI_ROM_CR8 = 0x07;
  if (!flash_rom_wait_ready()) { // guard the first command byte too, not only the ones after it
    return false;
  }
  SPI_ROM_CTRL8 = 0x0B; // SPI NOR fast read
  const uint8_t addr_bytes[3] = {(uint8_t)(rom_addr >> 16), (uint8_t)(rom_addr >> 8), (uint8_t)rom_addr};
  for (uint32_t i = 0; i < 3; i++) {
    if (!flash_rom_wait_ready()) { return false; }
    SPI_ROM_DATA8 = addr_bytes[i];
  }
  for (uint32_t i = 0; i < 2; i++) {
    if (!flash_rom_wait_ready()) { return false; }
    (void)SPI_ROM_DATA8;
  }
  for (uint32_t w = 0; w < nwords; w++) {
    for (uint32_t s = 0; s < 4; s++) {
      if (!flash_rom_wait_ready()) { return false; }
      (void)SPI_ROM_DATA8;
    }
    buf[w] = SPI_ROM_DATA32;
  }
  if (!flash_rom_wait_ready()) { return false; }
  SPI_ROM_CR8 = 0;
  return true;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  // 64-bit factory unique ID (+ checksum) in the read-only info flash (CH569 datasheet 2.2.3)
  uint32_t uid[2];
  if (!flash_rom_read_words(0x77FE4, uid, 2)) {
    return 0; // controller did not respond: no serial rather than a hung control transfer
  }
  const size_t len = TU_MIN(max_len, (size_t)8);
  memcpy(id, uid, len);
  return len;
}

int board_uart_read(uint8_t* buf, int len) {
  // UART1 RX (RXD1 = PA7): drain what the FIFO already holds, non-blocking like every other BSP.
  int count = 0;
  while ((count < len) && (R8_UART1_RFC > 0)) {
    buf[count++] = R8_UART1_RBR;
  }
  return count;
}

int board_uart_write(void const* buf, int len) {
  int txsize = len;
  const char* bufc = (const char*) buf;
  while (txsize--) {
    uart_write(*bufc++);
  }
  uart_sync();
  return len;
}
