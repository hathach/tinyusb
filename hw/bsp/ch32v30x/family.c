/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Greg Davill
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

/* metadata:
   manufacturer: WCH
*/

#include "stdio.h"
#include <string.h>

// https://github.com/openwch/ch32v307/pull/90
// https://github.com/openwch/ch32v20x/pull/12

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#include "debug_uart.h"
#include "ch32v30x.h"
#include "ch32v30x_it.h"
#include "ch32v30x_usb.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// TODO maybe having FS as port0, HS as port1

#if CFG_TUH_ENABLED && CFG_TUH_WCH_USBIP_USBHS && CFG_TUH_WCH_USBIP_USBFS
  #error "Select one WCH host controller"
#endif
// USBHS runs device and host on one controller (switched at run time, examples/dual/dynamic_switch);
// USBFS has no device deinit to switch with
#if CFG_TUD_ENABLED && CFG_TUH_ENABLED && CFG_TUD_WCH_USBIP_USBFS && CFG_TUH_WCH_USBIP_USBFS
  #error "WCH USBFS cannot run host and device on the same controller"
#endif
// The legacy tusb_init() (CFG_TUSB_RHPORTx_MODE) would start both roles on the one controller at once
#if defined(TUD_OPT_RHPORT) && defined(TUH_OPT_RHPORT) && (TUD_OPT_RHPORT == TUH_OPT_RHPORT)
  #error "Device and host on the same port need tusb_init(rhport, &init) per role, deinit to switch"
#endif

#define USBHS_HOST_ROLE   (CFG_TUH_ENABLED && CFG_TUH_WCH_USBIP_USBHS)
#define USBHS_DEVICE_ROLE (CFG_TUD_ENABLED && CFG_TUD_WCH_USBIP_USBHS)

// The startup enables QingKe hardware context stacking. Avoid duplicating its
// register saves in software: closely spaced ISO transactions need a short IRQ entry.
// Keep the assembly call or jump targets visible to LTO, as in the CH32V20x USB interrupt wrappers.
#if USBHS_HOST_ROLE
static void usbhs_hcd_int_handler(void) {
  tuh_int_handler(BOARD_TUH_RHPORT);
}
TU_ATTR_USED static void (*const usbhs_hcd_keep)(void) = usbhs_hcd_int_handler;
#endif
#if USBHS_DEVICE_ROLE
// The USBHS DCD's interrupt vector (dcd_wch_usbhs_v1.c, "Application integration"): early
// register snapshot, returns with mret. SystemView builds take it too: the DCD records the interrupt after
// the snapshot.
void dcd_int_handler_fast(void);
TU_ATTR_USED static void (*const usbhs_dcd_keep)(void) = dcd_int_handler_fast;
#endif

__attribute__((naked)) __attribute__((used)) void USBHS_IRQHandler(void) {
#if USBHS_HOST_ROLE && USBHS_DEVICE_ROLE
  // Both roles: the controller's host mode bit, which hcd_init() sets and dcd_init() clears, picks the
  // driver. CONTROL's bit 7 is its sign: the device path costs a load and a branch.
  TU_VERIFY_STATIC(USBHS_UC_HOST_MODE == 0x80, "host mode is CONTROL bit 7");
  __asm volatile(
    "li   t0, %[base]\n\t"
    "lb   t1, %[o_ctrl](t0)\n\t"
    "bgez t1, 1f\n\t"
    "call %[host]\n\t"
    "mret\n\t"
    "1:\n\t"
    "j    %[dev]\n\t"
    :
    : [base] "i"(USBHS_BASE), [o_ctrl] "i"(offsetof(USBHSD_TypeDef, CONTROL)), [host] "i"(usbhs_hcd_int_handler),
      [dev] "i"(dcd_int_handler_fast));
#elif USBHS_HOST_ROLE
  __asm volatile("call %[host]\n\t mret" : : [host] "i"(usbhs_hcd_int_handler));
#elif USBHS_DEVICE_ROLE
  __asm volatile("j %[dev]" : : [dev] "i"(dcd_int_handler_fast));
#else
  __asm volatile("mret");
#endif
}

// USBFS (OTG_FS): the startup vector table names it USBFS_IRQHandler
__attribute__((interrupt)) void USBFS_IRQHandler(void) {
  #if CFG_TUD_ENABLED && CFG_TUD_WCH_USBIP_USBFS
  tud_int_handler(0);
  #endif

  #if CFG_TUH_ENABLED && CFG_TUH_WCH_USBIP_USBFS
  tuh_int_handler(BOARD_TUH_RHPORT);
  #endif
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

static uint32_t SysTick_Config(uint32_t ticks) {
  // A lower preemption level than USBHS (left at 0), so the USB interrupt preempts the tick: a USBHS
  // completion waiting out the tick handler can have its status overwritten by the next isochronous
  // transaction, losing it (docs/reference/device_issues.rst, WCH item 1). The startup sets INTSYSCR
  // PMTCFG to 2 preemption bits (IPRIOR bits 7:6).
  NVIC_SetPriority(SysTicK_IRQn, 0x80);
  NVIC_EnableIRQ(SysTicK_IRQn);
  SysTick->CTLR = 0;
  SysTick->SR = 0;
  SysTick->CNT = 0;
  SysTick->CMP = ticks - 1;
  SysTick->CTLR = 0xF;
  return 0;
}

void board_init(void) {

  /* Disable interrupts during init */
  __disable_irq();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#endif

  usart_printf_init(CFG_BOARD_UART_BAUDRATE);

#ifdef CH32V30x_D8C
  // v305/v307: Highspeed USB
  RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
  RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
  RCC_USBHSConfig(RCC_USBPLL_Div2);
  RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
  RCC_USBHSPHYPLLALIVEcmd(ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
#endif

  // Fullspeed USB
  uint8_t otg_div;
  switch (SystemCoreClock) {
    case 48000000:  otg_div = RCC_OTGFSCLKSource_PLLCLK_Div1; break;
    case 96000000:  otg_div = RCC_OTGFSCLKSource_PLLCLK_Div2; break;
    case 144000000: otg_div = RCC_OTGFSCLKSource_PLLCLK_Div3; break;
    default: TU_ASSERT(0,); break;
  }
  RCC_OTGFSCLKConfig(otg_div);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure = {0};

  // LED
  LED_CLOCK_EN();
  GPIO_InitStructure.GPIO_Pin = LED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(LED_PORT, &GPIO_InitStructure);

  // Button
#ifdef BUTTON_PORT
  BUTTON_CLOCK_EN();
  GPIO_InitStructure.GPIO_Pin = BUTTON_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(BUTTON_PORT, &GPIO_InitStructure);
#endif
#if USBHS_HOST_ROLE || USBHS_DEVICE_ROLE
  // The USBHS vector returns with mret and leaves register saving to QingKe HPE: check that the
  // startup enabled it (INTSYSCR bit 0) rather than corrupt registers on the first USB interrupt
  uint32_t intsyscr;
  __asm volatile("csrr %0, 0x804" : "=r"(intsyscr));
  TU_ASSERT(intsyscr & 1u, );
#endif
#if USBHS_DEVICE_ROLE && defined(CFG_BOARD_USBHS_VTF_SLOT) && CFG_BOARD_USBHS_VTF_SLOT >= 0
  // PFIC fast vector (VTF): USBHS skips the vector table lookup, straight into the DCD's entry unless the
  // host role shares the controller
  TU_VERIFY_STATIC(CFG_BOARD_USBHS_VTF_SLOT < 4, "USBHS VTF slot must be 0..3, or -1 to disable");
  #if USBHS_HOST_ROLE
  SetVTFIRQ((uint32_t) USBHS_IRQHandler, USBHS_IRQn, CFG_BOARD_USBHS_VTF_SLOT, ENABLE);
  #else
  SetVTFIRQ((uint32_t) dcd_int_handler_fast, USBHS_IRQn, CFG_BOARD_USBHS_VTF_SLOT, ENABLE);
  #endif
#endif

  /* Enable interrupts globally */
  __enable_irq();

  board_delay(2);
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick_Handler(void) {
  // USBHS preempts this handler (SysTick_Config): mask it across the flag clear and the increment, or a
  // SystemView timestamp taken in between reads the old tick with no wrap pending, 1 ms behind. gintenr
  // (CSR 0x800) save/mask/restore, as SEGGER_RTT_LOCK() does in sysview_rtt_conf.h.
  uint32_t gintenr;
  __asm volatile("csrr %0, 0x800" : "=r"(gintenr));
  __asm volatile("csrc 0x800, %0\n fence.i" ::"r"(0x88) : "memory");
  SysTick->SR = 0;
  system_ticks++;
  __asm volatile("csrw 0x800, %0" ::"r"(gintenr) : "memory");
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

#if CFG_TUSB_SYSVIEW_ENABLED && CFG_TUSB_OS == OPT_OS_NONE
// SystemView timestamp in microseconds (src/tusb.c): RISC-V has no DWT, so it composes the 1 ms
// system_ticks with SysTick CNT, which hardware reloads to 0 every millisecond.
uint32_t SEGGER_SYSVIEW_X_GetTimestamp(void) {
  // SystemCoreClock is the SysTick clock; the clock is fixed after board_init()
  static uint32_t cycles_per_us;
  if (cycles_per_us == 0) {
    cycles_per_us = SystemCoreClock / 1000000u;
  }
  uint32_t ms, cnt, pend;
  do { // re-read the tick: CNT wraps to 0 exactly when system_ticks increments
    ms = system_ticks;
    // Callers hold SEGGER_RTT_LOCK(), so a wrap's tick IRQ can be pending: SR bit 0 (CNTIF) latches
    // the wrap. SR before CNT: a wrap between the two reads must not pair a pre-wrap CNT with a
    // post-wrap pending flag (that adds a phantom ~1 ms)
    pend = SysTick->SR & 1u;
    cnt = (uint32_t) SysTick->CNT;
  } while (ms != system_ticks || pend != (SysTick->SR & 1u));
  uint32_t t = (ms + (pend ? 1u : 0u)) * 1000u + (cycles_per_us ? cnt / cycles_per_us : 0);
  static uint32_t last; // monotonic clamp; callers are serialized by the RTT lock
  if ((int32_t) (t - last) < 0) { t = last; }
  last = t;
  return t;
}
#elif CFG_TUSB_SYSVIEW_ENABLED
#error "SYSVIEW on ch32v30x needs the bare-metal SysTick tick; not ported for this RTOS yet"
#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_WriteBit(LED_PORT, LED_PIN, state);
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PORT
  return BUTTON_STATE_ACTIVE == GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN);
#else
  return false;
#endif
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  volatile uint32_t* ch32_uuid = ((volatile uint32_t*) 0x1FFFF7E8UL); // ESIG unique ID
  uint32_t uid[3] = { ch32_uuid[0], ch32_uuid[1], ch32_uuid[2] };
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
  uint8_t const *p = (uint8_t const *) buf;
  for (int i = 0; i < len; i++) {
    uart_write(p[i]);
  }
  return len;
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(char* file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)
   */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
