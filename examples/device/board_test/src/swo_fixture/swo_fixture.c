/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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
 */

// Deterministic workload for SWO trace checks: timer interrupts at known rates, a nested interrupt per TIM7 tick,
// one variable with a known access pattern, ITM stimulus with known bytes, and busy/idle phases.
// A probe-side tool enables ITM/DWT; without it the fixture runs the same and counts the stimulus it skipped.

#include "bsp/board_api.h"
#include "swo_fixture.h"

#if CFG_TUSB_MCU == OPT_MCU_STM32F4
  #include "stm32f4xx_hal.h"
#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  #include "stm32h7xx_hal.h"
#else
  #error "SWO fixture supports stm32f4 and stm32h7 only"
#endif

#define SWO_FIXTURE_TIM6_HZ   1000u
#define SWO_FIXTURE_TIM7_HZ   100u
#define SWO_FIXTURE_BUSY_MS   100u
#define SWO_FIXTURE_IDLE_MS   100u
#define SWO_FIXTURE_ITM_SPIN  10000u  // FIFOREADY polls before a stimulus write is skipped

// Within each 1 ms SysTick period, every trace burst drains before the next one starts, even at a 1.5 MHz SWO
// with local timestamps (about 6.7 us per byte): SysTick at 0, TIM7's episode at 250-550 us every 10th period,
// TIM6 at 700 us, the ITM marker at 150 us of a period without TIM7. A packet arriving while the ITM is still
// sending is dropped as an overflow.
#define SWO_FIXTURE_TIM7_AT_US   250u
#define SWO_FIXTURE_GAP_US       150u  // around the nested interrupt in TIM7's episode
#define SWO_FIXTURE_TIM6_AT_US   700u
#define SWO_FIXTURE_MARKER_AT_US 150u

// nested interrupt: TIM8_TRG_COM_TIM14 is pended from the TIM7 handler only, its peripherals stay off
#define NESTED_IRQn       TIM8_TRG_COM_TIM14_IRQn
#define NESTED_IRQHandler TIM8_TRG_COM_TIM14_IRQHandler

enum {
  ITM_PORT_TEXT = 1, // "swo-fixture <phase>\n" in 32-bit writes, its tail in 8-bit ones
  ITM_PORT_WORDS = 2 // per phase: 32-bit phase, 16-bit 0xA55A, 8-bit 0x7E
};

volatile uint32_t swo_fixture_var;
volatile swo_fixture_stats_t swo_fixture_cur;
volatile swo_fixture_stats_t swo_fixture_last;
volatile uint32_t swo_fixture_timclk_hz;

//--------------------------------------------------------------------+
// Interrupts: NESTED preempts TIM6 preempts TIM7
//--------------------------------------------------------------------+
void TIM6_DAC_IRQHandler(void);
void TIM7_IRQHandler(void);
void NESTED_IRQHandler(void);

void TIM6_DAC_IRQHandler(void) {
  TIM6->SR = ~TIM_SR_UIF;
  swo_fixture_cur.tim6_count++;
  swo_fixture_var = swo_fixture_cur.tim6_count;
  swo_fixture_cur.var_writes++;
}

static void wait_tim7_us(uint32_t us) {
  uint32_t const start = TIM7->CNT;
  while (TIM7->CNT - start < us) {}
}

void TIM7_IRQHandler(void) {
  TIM7->SR = ~TIM_SR_UIF;
  swo_fixture_cur.tim7_count++;
  (void) swo_fixture_var;
  swo_fixture_cur.var_reads++;
  wait_tim7_us(SWO_FIXTURE_GAP_US);
  NVIC_SetPendingIRQ(NESTED_IRQn);
  __DSB();
  __ISB();
  wait_tim7_us(SWO_FIXTURE_GAP_US);
}

void NESTED_IRQHandler(void) {
  swo_fixture_cur.nested_count++;
}

//--------------------------------------------------------------------+
// ITM stimulus, never blocking
//--------------------------------------------------------------------+
static bool itm_put(uint32_t port, uint32_t value, uint32_t size) {
  bool const enabled = (CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) &&
                       (ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1UL << port));
  if (enabled) {
    for (uint32_t i = 0; i < SWO_FIXTURE_ITM_SPIN; i++) {
      if (ITM->PORT[port].u32 & 1u) { // FIFOREADY
        if (size == 1) {
          ITM->PORT[port].u8 = (uint8_t) value;
        } else if (size == 2) {
          ITM->PORT[port].u16 = (uint16_t) value;
        } else {
          ITM->PORT[port].u32 = value;
        }
        swo_fixture_cur.itm_sent++;
        return true;
      }
    }
  }
  swo_fixture_cur.itm_skipped++;
  return false;
}

static void itm_phase_marker(uint32_t phase) {
  char text[32];
  int const len = snprintf(text, sizeof(text), "swo-fixture %lu\n", (unsigned long) phase);
  int i = 0;
  for (; i + 4 <= len; i += 4) {
    uint32_t word;
    memcpy(&word, text + i, 4); // little-endian: the first character goes out first
    itm_put(ITM_PORT_TEXT, word, 4);
  }
  for (; i < len; i++) {
    itm_put(ITM_PORT_TEXT, (uint8_t) text[i], 1);
  }
  itm_put(ITM_PORT_WORDS, phase, 4);
  itm_put(ITM_PORT_WORDS, 0xA55Au, 2);
  itm_put(ITM_PORT_WORDS, 0x7Eu, 1);
}

//--------------------------------------------------------------------+
// Phases
//--------------------------------------------------------------------+
TU_ATTR_USED __attribute__((noinline)) static void swo_fixture_busy(uint32_t until_ms) {
  while ((int32_t) (tusb_time_millis_api() - until_ms) < 0) {
    __NOP();
  }
}

TU_ATTR_USED __attribute__((noinline)) static void swo_fixture_idle(uint32_t until_ms) {
  while ((int32_t) (tusb_time_millis_api() - until_ms) < 0) {
    __WFI();
  }
}

// Timer kernel clock: PCLK1 when the APB1 prescaler is 1, else 2 x PCLK1 (RM0090 RCC clocks; RM0433 Table 57, TIMPRE=0)
static uint32_t timer_clock_hz(void) {
  uint32_t const pclk1 = HAL_RCC_GetPCLK1Freq();
#if CFG_TUSB_MCU == OPT_MCU_STM32F4
  bool const apb1_div1 = (RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1;
#else
  bool const apb1_div1 = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) == RCC_D2CFGR_D2PPRE1_DIV1;
#endif
  return apb1_div1 ? pclk1 : 2 * pclk1;
}

// first update first_us after the start, then one every 1/hz
static void timer_start(TIM_TypeDef* tim, IRQn_Type irq, uint32_t hz, uint32_t first_us, uint32_t priority) {
  tim->PSC = swo_fixture_timclk_hz / 1000000u - 1; // 1 MHz count
  tim->ARR = 1000000u / hz - 1;
  tim->EGR = TIM_EGR_UG;
  tim->CNT = 1000000u / hz - first_us;
  tim->SR = 0;
  tim->DIER = TIM_DIER_UIE;
  NVIC_SetPriority(irq, priority);
  NVIC_EnableIRQ(irq);
  tim->CR1 = TIM_CR1_CEN;
}

void swo_fixture_init(void) {
  __HAL_RCC_TIM6_CLK_ENABLE();
  __HAL_RCC_TIM7_CLK_ENABLE();
  swo_fixture_timclk_hz = timer_clock_hz();

  // start right after a SysTick tick, so the timers keep their place in its period (both from the same PLL)
  (void) SysTick->CTRL;
  while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {}

  NVIC_SetPriority(NESTED_IRQn, 1);
  NVIC_EnableIRQ(NESTED_IRQn);
  timer_start(TIM6, TIM6_DAC_IRQn, SWO_FIXTURE_TIM6_HZ, SWO_FIXTURE_TIM6_AT_US, 2);
  timer_start(TIM7, TIM7_IRQn, SWO_FIXTURE_TIM7_HZ, SWO_FIXTURE_TIM7_AT_US, 3);
}

// SWO_FIXTURE_MARKER_AT_US into a SysTick period whose TIM7 episode is at least a period away
static bool marker_slot(void) {
  uint32_t const tim6_cnt = TIM6->CNT; // SysTick period position + 1000 - SWO_FIXTURE_TIM6_AT_US
  uint32_t const at = (SWO_FIXTURE_MARKER_AT_US + 1000u - SWO_FIXTURE_TIM6_AT_US) % 1000u;
  uint32_t const tim7_cnt = TIM7->CNT; // us since TIM7's last update
  return tim6_cnt >= at && tim6_cnt < at + 50u && tim7_cnt >= 1000u && tim7_cnt < 9000u;
}

// One phase: busy half with the ITM marker in its middle, then idle half; swo_fixture_last holds the counts of the
// last complete phase. The LED toggles at each phase start, a reference for an external probe.
void swo_fixture_task(void) {
  uint32_t const start = tusb_time_millis_api();
  board_led_write(swo_fixture_cur.phase & 1u);
  swo_fixture_busy(start + SWO_FIXTURE_BUSY_MS / 2);
  while (!marker_slot()) {}
  itm_phase_marker(swo_fixture_cur.phase);
  swo_fixture_busy(start + SWO_FIXTURE_BUSY_MS);
  swo_fixture_idle(start + SWO_FIXTURE_BUSY_MS + SWO_FIXTURE_IDLE_MS);

  __disable_irq();
  swo_fixture_last = swo_fixture_cur;
  swo_fixture_cur = (swo_fixture_stats_t) {.phase = swo_fixture_cur.phase + 1};
  __enable_irq();
}
