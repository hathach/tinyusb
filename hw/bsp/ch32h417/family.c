/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   manufacturer: WCH
*/

// TinyUSB runs entirely on the V3F core (core 0, the boot core, HCLK = 100 MHz at the vendor
// default clock tree): single image at flash 0x0, no core-1 (V5F) wake-up. The vendor's own
// USBSS demo supports this Run_Core_V3F configuration.

#include "ch32h417.h"

#include "bsp/board_api.h"
#include "board.h"

// Reflash backdoor: the USART1 magic-sequence hook, the UART flash loader and the flash-park
// window below. ON by default, on purpose: the chip's only SWD/SDI pins are the USB2 D+/D- pair,
// so once a USB image owns them this is the only way to get new firmware in (the test rig
// reflashes exclusively through it, test/hil/wch_uart_flash.py). A product build compiles it out
// with -DCFG_BOARD_CH32H417_UART_LOADER=0 - which also stops stray console-UART bytes from
// rebooting the board, at the price of needing physical access to recover it.
#ifndef CFG_BOARD_CH32H417_UART_LOADER
#define CFG_BOARD_CH32H417_UART_LOADER 1
#endif

// Crash watchdog: an ~8 s IWDG armed at the end of board_init, kicked from the 1 ms SysTick and,
// secondarily, from board_led_write(). OFF by default - it resets the chip whenever the core is
// halted longer than the period (any GDB/breakpoint session) and the board_led_write() kick is a
// contract no application signed up for. The HIL rig builds with -DCFG_BOARD_CH32H417_CRASH_WDT=1
// so a crashed board reboots itself into the flash-park window (CFG_BOARD_CH32H417_UART_LOADER)
// and stays reflashable with no hands on the hardware.
#ifndef CFG_BOARD_CH32H417_CRASH_WDT
#define CFG_BOARD_CH32H417_CRASH_WDT 0
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Prototypes for the (weak) vector-table handlers overridden here
void USBSS_IRQHandler(void);
void USBSS_LINK_IRQHandler(void);
void USBHS_IRQHandler(void);
#if CFG_BOARD_CH32H417_UART_LOADER
void USART1_IRQHandler(void);
#endif
void SysTick0_Handler(void);

#if CFG_BOARD_CH32H417_UART_LOADER

// Flash-park support: the HIL flasher writes {0x55,0xAA,'F','P'} to the WCH-LinkE virtual COM
// port; the RX interrupt below stamps this flag (in a NOLOAD region that survives the soft
// reset) and reboots. board_init then parks ~10 s before any USB init so the debug interface is
// guaranteed reachable for an unattended reflash - the chip's only SWD/SDI pins double as the
// USB2 D+/D- pair, and a running USB2(-fallback) image makes them undebuggable.
__attribute__((section(".noinit"))) static volatile uint32_t flash_park_flag;
__attribute__((section(".noinit"))) static volatile uint32_t flash_park_token;
#define FLASH_PARK_MAGIC 0x50414B31UL // 'P','A','K','1'
#define FLASH_LOAD_MAGIC 0x4C445231UL // 'L','D','R','1' - UART flash loader (see uart_flash_loader)
// The .noinit words are uninitialized SRAM on a cold boot: a request is honoured only when the
// companion token matches its magic, so random content cannot park the board (10 s stall) or drop
// it into the loader (looks bricked). board_init additionally ignores requests after a power-on
// reset, where SRAM content is undefined by definition.
#define FLASH_PARK_TOKEN(_magic) ((_magic) ^ 0xA5C33C5AUL)

static void flash_park_request(uint32_t magic) {
  flash_park_flag = magic;
  flash_park_token = FLASH_PARK_TOKEN(magic);
}

// Consume a pending request, clearing both words; returns 0 when there is none or the pair is not
// a valid request
static uint32_t flash_park_take(void) {
  const uint32_t magic = flash_park_flag;
  const bool valid = (flash_park_token == FLASH_PARK_TOKEN(magic)) &&
                     ((magic == FLASH_PARK_MAGIC) || (magic == FLASH_LOAD_MAGIC));
  flash_park_flag = 0;
  flash_park_token = 0;
  return valid ? magic : 0;
}

__attribute__((interrupt)) void USART1_IRQHandler(void) {
  // {0x55,0xAA,'F','P'} = park for SWD flashing; {0x55,0xAA,'F','L'} = enter the UART loader
  static const uint8_t seq[3] = {0x55, 0xAA, 'F'};
  static uint8_t idx = 0;
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    const uint8_t b = (uint8_t)USART_ReceiveData(USART1); // reading DATAR clears RXNE
    if (idx < 3) {
      idx = (b == seq[idx]) ? (uint8_t)(idx + 1) : ((b == seq[0]) ? 1 : 0);
    } else {
      if (b == 'P' || b == 'L') {
        flash_park_request((b == 'P') ? FLASH_PARK_MAGIC : FLASH_LOAD_MAGIC);
        NVIC_SystemReset();
      }
      idx = (b == seq[0]) ? 1 : 0;
    }
  }
}

//--------------------------------------------------------------------+
// UART flash loader: reflash over the WCH-LinkE VCP with no debug-pin access. The chip's only
// SWD/SDI pins (PB8/PB9) are the USB2 D+/D- pair; with an untaped cable plugged into a host,
// the host PHY's line termination corrupts SDI beyond use, so wlink cannot flash. This loader
// needs only USART1 (dedicated pins), and the whole image executes from RAM_CODE, so rewriting
// flash under it is safe. Host side: test/hil/wch_uart_flash.py.
//
// Protocol (little-endian, image padded to a 1024-byte multiple by the host):
//   dev : 'L' banner (~2 Hz) while waiting for a session
//   host: 'H' len32 crc32
//   dev : erase [0x08000000, len8k) -> 'A' ok / 'E' error
//   host: 'C' n16 <n bytes>   (n == 1024)
//   dev : program             -> 'K' ok / 'E' error
//   dev : after len bytes: CRC32 readback over flash -> 'D' + reset, or 'X' (host may retry)
//--------------------------------------------------------------------+

#define UART_LOAD_BASE 0x08000000UL

static uint32_t loader_crc32(uint32_t crc, const uint8_t *p, uint32_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

static int loader_getc(uint32_t spins) { // -1 on timeout; ~10M spins/s at 100 MHz HCLK
  while (spins--) {
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
      return (int)(USART_ReceiveData(USART1) & 0xFF);
    }
  }
  return -1;
}

static void loader_putc(uint8_t c) {
  while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
  USART_SendData(USART1, c);
}

static bool loader_get_u32(uint32_t *out) {
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) {
    const int c = loader_getc(20000000u); // ~2 s
    if (c < 0) {
      return false;
    }
    v |= (uint32_t)c << (8 * i);
  }
  *out = v;
  return true;
}

__attribute__((noreturn)) static void uart_flash_loader(void) {
  USART_ITConfig(USART1, USART_IT_RXNE, DISABLE); // polled from here on
  NVIC_DisableIRQ(USART1_IRQn);
  static uint8_t chunk[1024] __attribute__((aligned(4)));

  for (;;) { // one iteration = one flash session attempt
    // Banner until the host opens a session. Bounded: an interrupted flash (Ctrl-C, a harness
    // timeout, a VCP hiccup) must not strand the board here with USB down and the RXNE vector
    // disabled -- the park bytes the rig uses to reset a board only take effect in the
    // application's boot window, so nothing external can recover it. After ~30 s with no
    // session, reset back into the application.
    int c;
    uint32_t blink = 0;
    uint32_t idle = 0;
    do {
      loader_putc('L');
      board_led_write((++blink) & 1);
      c = loader_getc(5000000u); // ~0.5 s
      if (c == -1 && ++idle >= 60u) {
        NVIC_SystemReset();
      }
    } while (c != 'H');

    uint32_t len, crc;
    if (!loader_get_u32(&len) || !loader_get_u32(&crc)) {
      loader_putc('E');
      continue;
    }
    if (len == 0 || len > 448u * 1024u || (len & 1023u)) {
      loader_putc('E');
      continue;
    }

    const uint32_t erase_len = (len + 8191u) & ~8191u; // FLASH_ROM_ERASE needs 8 KB granularity
    if (FLASH_ROM_ERASE(UART_LOAD_BASE, erase_len) != FLASH_COMPLETE) {
      loader_putc('E');
      continue;
    }
    loader_putc('A');

    uint32_t off = 0;
    bool fail = false;
    while (off < len) {
      c = loader_getc(20000000u); // ~2 s for the next chunk header
      if (c != 'C') {
        fail = true;
        break;
      }
      uint32_t n = 0;
      for (int i = 0; i < 2 && !fail; i++) {
        c = loader_getc(20000000u);
        if (c < 0) {
          fail = true;
        } else {
          n |= (uint32_t)c << (8 * i);
        }
      }
      if (fail || n != sizeof(chunk)) {
        fail = true;
        break;
      }
      for (uint32_t i = 0; i < n; i++) {
        c = loader_getc(20000000u);
        if (c < 0) {
          fail = true;
          break;
        }
        chunk[i] = (uint8_t)c;
      }
      if (fail || FLASH_ROM_WRITE(UART_LOAD_BASE + off, (uint32_t *)(uintptr_t)chunk, n) != FLASH_COMPLETE) {
        fail = true;
        break;
      }
      off += n;
      loader_putc('K');
    }
    if (fail) {
      loader_putc('E');
      continue;
    }

    if (loader_crc32(0, (const uint8_t *)UART_LOAD_BASE, len) == crc) {
      loader_putc('D');
      for (volatile uint32_t i = 0; i < 2000000; i++) {} // drain the last byte
      NVIC_SystemReset();
    }
    loader_putc('X');
  }
}

#endif // CFG_BOARD_CH32H417_UART_LOADER

// Single rhport 0: SPEED=super compiles the USB3 dcd (USBSS + USBSS_LINK vectors); SPEED=high
// the USB2 dcd (USBHS vector); the active dcd's dcd_int_handler dispatches on its own flag
// registers, so all vectors share ONE forwarder. Aliases (not identical function bodies) are
// required: gcc's identical-code-folding would otherwise rewrite one interrupt handler as a
// plain call into another, whose mret then skips the caller's epilogue and leaks its stack
// frame on every interrupt. Use the standard riscv interrupt attribute - WCH's
// "WCH-Interrupt-fast" is only understood by WCH's own gcc fork.
__attribute__((interrupt)) void USBSS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USBSS_LINK_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
void USBHS_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));

// the USB3 dcd uses TIM12 for the SuperSpeed retrain backoff and, with
// CFG_TUD_WCH_USB30_FALLBACK, as the SuperSpeed-training timeout for the USB2 fallback
// (unused but harmless with SPEED=high: nothing enables the TIM12 interrupt there)
void TIM12_IRQHandler(void);
void TIM12_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));

//--------------------------------------------------------------------+
// SysTick (V3F core uses SysTick0; its ISR register holds the flags of both SysTicks)
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick0_Handler(void) {
  SysTick0->ISR &= ~(1u << 0); // clear count-reached flag
  system_ticks++;
#if CFG_BOARD_CH32H417_CRASH_WDT
  // Primary watchdog kick: a live tick means the core is running. A halted core (debug halt,
  // ebreak on a silent assert) stops the tick and the IWDG reset + auto-park below recovers the
  // board. Applications are NOT required to blink the LED (see board_init).
  IWDG_ReloadCounter();
#endif
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+

void board_init(void) {
  SystemInit(); // clock tree: SYSCLK 400 MHz, V3F core (HCLK) 100 MHz from HSE
  SystemAndCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1 ms tick on SysTick0 (counts HCLK, auto-reload)
  SysTick0->ISR &= ~(1u << 0);
  SysTick0->CMP = SystemCoreClock / 1000 - 1;
  SysTick0->CNT = 0;
  SysTick0->CTLR = 0xF;
  NVIC_SetPriority(SysTick0_IRQn, 0xF0);
  NVIC_EnableIRQ(SysTick0_IRQn);
#endif

  // USART1 TX on PA9, RX on PA10 (AF7) - wired to the on-board WCH-LinkE virtual COM port
#ifdef CFG_BOARD_UART_BAUDRATE
  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF7);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF7);
  GPIO_InitTypeDef gpio_init = {
    .GPIO_Pin = GPIO_Pin_9,
    .GPIO_Speed = GPIO_Speed_Very_High,
    .GPIO_Mode = GPIO_Mode_AF_PP,
  };
  GPIO_Init(GPIOA, &gpio_init);
  gpio_init.GPIO_Pin = GPIO_Pin_10;
  gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &gpio_init);

  USART_InitTypeDef usart_init = {
    .USART_BaudRate = CFG_BOARD_UART_BAUDRATE,
    .USART_WordLength = USART_WordLength_8b,
    .USART_StopBits = USART_StopBits_1,
    .USART_Parity = USART_Parity_No,
    .USART_Mode = USART_Mode_Tx | USART_Mode_Rx,
    .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
  };
  USART_Init(USART1, &usart_init);
#if CFG_BOARD_CH32H417_UART_LOADER
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // flash-park magic matcher (see USART1_IRQHandler)
  NVIC_EnableIRQ(USART1_IRQn);
#endif
  USART_Cmd(USART1, ENABLE);
#endif

  // LED
  RCC_HB2PeriphClockCmd(LED_CLOCK, ENABLE);
  GPIO_InitTypeDef led_init = {
    .GPIO_Pin = LED_PIN,
    .GPIO_Speed = GPIO_Speed_Very_High,
    .GPIO_Mode = GPIO_Mode_Out_PP,
  };
  GPIO_Init(LED_PORT, &led_init);
  board_led_write(false);

#if CFG_BOARD_CH32H417_UART_LOADER
  // Flash-park window: hold here ~10 s with USB never initialized - PB8/PB9 stay in SWD/SDI mode
  // and no interrupt load exists, so wlink can erase/flash unattended even when the normal image
  // would own the USB2 pins (HS fallback) or storm the core. Fast LED blink signals the window.
  // Entered when:
  //  - the previous run requested a park (magic over the WCH-LinkE VCP, see USART1_IRQHandler), or
  //  - the independent watchdog reset the chip (the previous image crashed and stopped kicking,
  //    CFG_BOARD_CH32H417_CRASH_WDT builds only): a crash-looping image thus re-opens this window
  //    every cycle, keeping the board recoverable with no hands and no reset line.
  const bool wdg_reset = (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET);
  const bool por_reset = (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET);
  RCC_ClearFlag(); // reset flags are sticky: clear on every boot, else a cold boot's POR flag
                   // would shadow every later software reset
  uint32_t park_req = flash_park_take();
  if (por_reset) {
    park_req = 0; // SRAM (and with it the request words) is undefined after a power-on reset
  }
  if (wdg_reset || park_req == FLASH_PARK_MAGIC) {
    for (uint32_t t = 0; t < 80; t++) { // 80 x ~125 ms = ~10 s at 100 MHz HCLK
      board_led_write(t & 1);
      for (volatile uint32_t i = 0; i < 2500000; i++) {}
    }
    board_led_write(false);
  }

  // UART flash loader (requested via {0x55,0xAA,'F','L'} over the VCP): flash over USART1,
  // for when the SWD/SDI pins are unusable (untaped USB2 cable plugged into a host). Runs
  // before the watchdog is armed; never returns (resets after a successful flash).
  if (park_req == FLASH_LOAD_MAGIC) {
    uart_flash_loader();
  }
#endif // CFG_BOARD_CH32H417_UART_LOADER

  // Crash recovery (opt-in, see CFG_BOARD_CH32H417_CRASH_WDT): independent watchdog, ~8 s at
  // LSI/256, kicked from the 1 ms SysTick (and, as a secondary, from board_led_write for OS
  // builds that reconfigure the tick). Armed AFTER the park window above so the window itself
  // cannot be cut short. Recovers a halted core (debug halt, ebreak); a firmware that merely
  // stops calling tud_task still ticks and is NOT reset. NOTE: halting the core for more than
  // the period watchdog-resets it (DBGMCU_Config(DBGMCU_IWDG_STOP) hangs when no debugger
  // session is active - its csrw 0x7c0 faults - so the freeze-on-halt is deliberately not
  // used); request a park first for long debug sessions, the window runs with the watchdog
  // unarmed. Bare-metal only: the 1 ms SysTick kick above does not exist under an RTOS.
#if CFG_BOARD_CH32H417_CRASH_WDT && (CFG_TUSB_OS == OPT_OS_NONE)
  RCC_LSICmd(ENABLE);
  { // bounded: a dead LSI must not hang boot (the IWDG simply stays unarmed)
    uint32_t timeout = 1000000;
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && --timeout) {}
    if (timeout == 0) { return; }
  }
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
  IWDG_SetPrescaler(IWDG_Prescaler_256);
  IWDG_SetReload(1000); // ~8 s at the ~32 kHz LSI
  IWDG_ReloadCounter();
  IWDG_Enable();
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
#if CFG_BOARD_CH32H417_CRASH_WDT
  IWDG_ReloadCounter(); // secondary watchdog kick (primary is the SysTick handler, see board_init)
#endif
  GPIO_WriteBit(LED_PORT, LED_PIN, (state ^ (LED_STATE_ON == 0)) ? Bit_SET : Bit_RESET);
}

uint32_t board_button_read(void) {
  return 0; // no user button on this board
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  volatile uint32_t* ch32_uuid = ((volatile uint32_t*) 0x1FFFF7E8UL); // ESIG unique ID (96 bit)
  uint32_t uid[3] = {ch32_uuid[0], ch32_uuid[1], ch32_uuid[2]};
  const size_t len = max_len < sizeof(uid) ? max_len : sizeof(uid);
  memcpy(id, uid, len); // byte copy: id[] need not be 4-byte aligned
  return len;
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
  int txsize = len;
  const char* bufc = (const char*) buf;
  while (txsize--) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
    USART_SendData(USART1, (uint16_t) (uint8_t) *bufc++);
  }
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}
  return len;
}
