/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

// Boot from the octal NOR flash on XSPI2 (XSPIM_P2) and run from AXI SRAM.
//
// The 64 KB internal flash holds only the vector table and this file's .boot_text. The
// rest of the image is linked to run in AXI SRAM and stored in the external flash
// (0x70000000). Reset_Handler maps the flash in 1-line SPI mode at <= 32 MHz (within the
// HSLV-off I/O limit, so the XSPI2_HSLV option byte is not needed), then:
//  - GCC/Clang: copies the image into RAM, zeroes .bss and calls main()
//  - IAR: hands over to __iar_program_start, whose "initialize by copy" does the copy
//    (what ILINK deems needed before it, e.g. the IRQ handlers, stays in internal flash).
//
// Everything reachable before the copy lives in .boot_text: no calls out of it, no
// constant tables, volatile copy loops so the compiler cannot turn them into memcpy().

#include "stm32h7rsxx.h"

// make compiles every family .c: only boards that link xspi linker scripts set XSPI_BOOT
#if defined(XSPI_BOOT) && XSPI_BOOT

#define BOOT_FUNC __attribute__((section(".boot_text")))

// Supply configuration, must match the HAL_PWREx_ConfigSupply() call in family.c
#ifndef XSPI_BOOT_PWR_SUPPLY
  #define XSPI_BOOT_PWR_SUPPLY PWR_CSR2_LDOEN // PWR_LDO_SUPPLY
#endif

// Flash size: 2^(DEVSIZE+1) bytes, default 32 MB (MX25UW25645G)
#ifndef XSPI_FLASH_DEVSIZE
  #define XSPI_FLASH_DEVSIZE 24u
#endif

// Flash commands: 1-line SPI, and the 2-byte (cmd, ~cmd) octal form
#define FLASH_CMD_RSTEN       0x66u
#define FLASH_CMD_RST         0x99u
#define FLASH_CMD_OPI_RSTEN   0x6699u
#define FLASH_CMD_OPI_RST     0x9966u
#define FLASH_CMD_RDSR        0x05u
#define FLASH_CMD_FAST_READ4B 0x0Cu
#define FLASH_FAST_READ_DUMMY 8u
#define FLASH_SR_WIP          0x01u

#define CCR_INSTR_1LINE   (1u << XSPI_CCR_IMODE_Pos)
#define CCR_INSTR_8LINE   (4u << XSPI_CCR_IMODE_Pos)
#define CCR_INSTR_DTR     (1u << XSPI_CCR_IDTR_Pos)
#define CCR_INSTR_16BIT   (1u << XSPI_CCR_ISIZE_Pos)
#define CCR_ADDR_1LINE_32 ((1u << XSPI_CCR_ADMODE_Pos) | (3u << XSPI_CCR_ADSIZE_Pos))
#define CCR_DATA_1LINE    (1u << XSPI_CCR_DMODE_Pos)

#define CR_FMODE_INDIRECT_WRITE (0u << XSPI_CR_FMODE_Pos)
#define CR_FMODE_INDIRECT_READ  (1u << XSPI_CR_FMODE_Pos)
#define CR_FMODE_MEMORY_MAPPED  (3u << XSPI_CR_FMODE_Pos)

void Reset_Handler(void);
void xspi_boot_deinit(void);

//--------------------------------------------------------------------+
// XSPI2 indirect commands
//--------------------------------------------------------------------+
BOOT_FUNC static void xspi_wait_idle(void) {
  while (XSPI2->SR & XSPI_SR_BUSY_Msk) {}
}

BOOT_FUNC static void xspi_wait_complete(void) {
  while (!(XSPI2->SR & XSPI_SR_TCF_Msk)) {}
  XSPI2->FCR = XSPI_FCR_CTCF_Msk;
}

BOOT_FUNC static void xspi_set_fmode(uint32_t fmode) {
  XSPI2->CR = (XSPI2->CR & ~XSPI_CR_FMODE_Msk) | fmode;
}

// Instruction-only command: starts on the IR write (no address, no data)
BOOT_FUNC static void xspi_command(uint32_t ccr, uint32_t instruction) {
  xspi_wait_idle();
  xspi_set_fmode(CR_FMODE_INDIRECT_WRITE);
  XSPI2->CCR = ccr;
  XSPI2->TCR = 0;
  XSPI2->IR = instruction;
  xspi_wait_complete();
}

BOOT_FUNC static uint8_t xspi_read_status(void) {
  xspi_wait_idle();
  xspi_set_fmode(CR_FMODE_INDIRECT_READ);
  XSPI2->DLR = 0; // 1 byte
  XSPI2->CCR = CCR_INSTR_1LINE | CCR_DATA_1LINE;
  XSPI2->TCR = 0;
  XSPI2->IR = FLASH_CMD_RDSR; // no address: indirect read starts here
  xspi_wait_complete();
  return *((volatile uint8_t const *) &XSPI2->DR);
}

//--------------------------------------------------------------------+
// Bring up XSPI2 and map the flash at 0x70000000
//--------------------------------------------------------------------+
BOOT_FUNC static void xspi_boot_init(void) {
  // Run SYSCLK from HSI so hclk5 (XSPI2 kernel clock) is at most 64 MHz, also when entered
  // without a reset (debugger "go" after an external loader left the PLL running)
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) {}
  RCC->CFGR &= ~RCC_CFGR_SW;
  while (RCC->CFGR & RCC_CFGR_SWS) {}
  RCC->CCIPR1 &= ~RCC_CCIPR1_XSPI2SEL; // hclk5

  RCC->AHB4ENR |= RCC_AHB4ENR_GPIONEN;
  RCC->AHB5ENR |= RCC_AHB5ENR_XSPI2EN | RCC_AHB5ENR_XSPIMEN;
  RCC->AHB5RSTR |= RCC_AHB5RSTR_XSPI2RST | RCC_AHB5RSTR_XSPIMRST;
  RCC->AHB5RSTR &= ~(RCC_AHB5RSTR_XSPI2RST | RCC_AHB5RSTR_XSPIMRST);
  (void) RCC->AHB5RSTR;

  // XSPIM_P2 I/O supply; XSPIM stays in its reset (direct) mode: XSPI2 -> port 2.
  // PWR_CSR2's supply byte is write-once per power-on: any write of CSR2 locks it, and writing
  // back the startup value (SDEN | LDOEN) is an illegal combination that leaves ACTVOSRDY low
  // until the next power cycle (RM0477 Table 44). So the first CSR2 write programs the supply
  // too, and the image is copied only once the voltage levels are valid (no RAM writes in Run*).
  uint32_t const supply_mask = PWR_CSR2_SDHILEVEL | PWR_CSR2_SMPSEXTHP | PWR_CSR2_SDEN |
                               PWR_CSR2_LDOEN | PWR_CSR2_BYPASS;
  PWR->CSR2 = (PWR->CSR2 & ~supply_mask) | XSPI_BOOT_PWR_SUPPLY | PWR_CSR2_EN_XSPIM2;
  for (uint32_t i = 0; i < 1000000u && !(PWR->SR1 & PWR_SR1_ACTVOSRDY); i++) {}

  // PN0 DQS, PN1 NCS1, PN2-PN5 IO0-3, PN6 CLK, PN8-PN11 IO4-7: AF9, very high speed, no pull
  for (uint32_t pin = 0; pin < 12; pin++) {
    if (pin == 7) {
      continue;
    }
    uint32_t const afr_shift = 4u * (pin & 7u);
    GPION->AFR[pin >> 3] = (GPION->AFR[pin >> 3] & ~(0xFu << afr_shift)) | (9u << afr_shift);
    GPION->OSPEEDR |= 3u << (2u * pin);
    GPION->PUPDR &= ~(3u << (2u * pin));
    GPION->OTYPER &= ~(1u << pin);
    GPION->MODER = (GPION->MODER & ~(3u << (2u * pin))) | (2u << (2u * pin));
  }

  // Macronix memory type, 4 cycles NCS high time, hclk5 / 2
  XSPI2->DCR1 = (1u << XSPI_DCR1_MTYP_Pos) | (XSPI_FLASH_DEVSIZE << XSPI_DCR1_DEVSIZE_Pos) |
                (3u << XSPI_DCR1_CSHT_Pos);
  XSPI2->DCR2 = 1u << XSPI_DCR2_PRESCALER_Pos;
  XSPI2->CR = XSPI_CR_EN_Msk;

  // Flash RESET# is not wired: software reset from whichever mode the last user (e.g. a
  // programmer's external loader) left it in: octal DTR, octal STR, then SPI. A command in
  // the wrong mode is too short for the flash to decode, so it is ignored.
  xspi_command(CCR_INSTR_8LINE | CCR_INSTR_DTR | CCR_INSTR_16BIT, FLASH_CMD_OPI_RSTEN);
  xspi_command(CCR_INSTR_8LINE | CCR_INSTR_DTR | CCR_INSTR_16BIT, FLASH_CMD_OPI_RST);
  xspi_command(CCR_INSTR_8LINE | CCR_INSTR_16BIT, FLASH_CMD_OPI_RSTEN);
  xspi_command(CCR_INSTR_8LINE | CCR_INSTR_16BIT, FLASH_CMD_OPI_RST);
  xspi_command(CCR_INSTR_1LINE, FLASH_CMD_RSTEN);
  xspi_command(CCR_INSTR_1LINE, FLASH_CMD_RST);

  // reset recovery (~1 ms at 64 MHz), then wait for any interrupted operation to finish
  for (volatile uint32_t i = 0; i < 16000u; i++) {}
  while (xspi_read_status() & FLASH_SR_WIP) {}

  // Memory-mapped 4-byte fast read, 1-line SPI
  xspi_wait_idle();
  XSPI2->CCR = CCR_INSTR_1LINE | CCR_ADDR_1LINE_32 | CCR_DATA_1LINE;
  XSPI2->TCR = FLASH_FAST_READ_DUMMY << XSPI_TCR_DCYC_Pos;
  XSPI2->IR = FLASH_CMD_FAST_READ4B;
  xspi_set_fmode(CR_FMODE_MEMORY_MAPPED);
  __DSB();
  __ISB();
}

//--------------------------------------------------------------------+
// Reset
//--------------------------------------------------------------------+
#if defined(__ICCARM__)

// ILINK leaves out of "initialize by copy" whatever its entry (__iar_program_start) can
// reach, as it may run before the copy. The library's __iar_program_start (cstartup_M.s)
// does "REQUIRE __vector_table", which drags the vector table, every IRQ handler and what
// they call into that set. This replacement does the same startup without that reference,
// so the copy takes everything but the boot code; the .icf keeps .intvec at FLASH_BASE.
extern void __cmain(void);
extern void __iar_init_vfp(void);
void __iar_program_start(void);

BOOT_FUNC void __iar_program_start(void) {
  __iar_init_vfp();
  __cmain(); // data init: copies the image from XSPI2 to RAM, then main()
}

BOOT_FUNC void Reset_Handler(void) {
  // SystemInit() equivalent: it is in the RAM image. VTOR by address, not by __vector_table,
  // to keep the vector table out of the startup's reach (see above)
  SCB->CPACR |= (3UL << 20U) | (3UL << 22U);
  SCB->VTOR = FLASH_BASE;

  xspi_boot_init();
  __iar_program_start();
}

#else

extern void *g_pfnVectors; // as declared by system_stm32h7rsxx.c
extern uint32_t __ram_image_lma[], __ram_image_start[], __ram_image_end[];
extern uint32_t _sbss[], _ebss[];
extern void __libc_init_array(void);
extern int main(void);

BOOT_FUNC void Reset_Handler(void) {
  // SystemInit() equivalent: it is in the RAM image
  SCB->CPACR |= (3UL << 20U) | (3UL << 22U);
  SCB->VTOR = (uint32_t) &g_pfnVectors;

  xspi_boot_init();

  // caches are still off: no maintenance needed after the copy
  volatile uint32_t const *src = __ram_image_lma;
  for (volatile uint32_t *dst = __ram_image_start; dst < __ram_image_end;) {
    *dst++ = *src++;
  }
  for (volatile uint32_t *dst = _sbss; dst < _ebss;) {
    *dst++ = 0;
  }
  __DSB();
  __ISB();

  __libc_init_array();
  (void) main();
  while (1) {}
}

#endif

// Called from board_init() once the image is in RAM: stop the memory-mapped prefetch and
// release NCS before the clocks are raised (hclk5 / 2 would overclock the flash)
void xspi_boot_deinit(void) {
  XSPI2->CR |= XSPI_CR_ABORT_Msk; // reads as 0: BUSY falls when the abort is done
  xspi_wait_idle();
  XSPI2->CR = 0;
}

#endif
