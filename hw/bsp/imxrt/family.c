/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
   manufacturer: NXP
*/

#include "bsp/board_api.h"
#include "board/clock_config.h"
#include "board/pin_mux.h"
#include "board.h"

// Suppress warning caused by mcu driver
#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "fsl_clock.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_lpuart.h"
#include "fsl_ocotp.h"

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif

/* --- Note about USB buffer RAM ---
  For M7 core it's recommended to put USB buffer in DTCM for better performance (flexspi_nor linker default)
  Otherwise you have to put the buffer in a non-cacheable section by configurate MPU manually or using BOARD_ConfigMPU():
  - Define CFG_TUSB_MEM_SECTION=__attribute__((section("NonCacheable")))
  - (IAR only) Change __NCACHE_REGION_SIZE in linker script to cover the size of non-cacheable section, multiple of 2^N

  For secondary M4 core, the USB controller doesn't support transfer from DTCM so OCRAM must be used:
  - __NCACHE_REGION_SIZE is defined by the linker script by default
  - Define CFG_TUSB_MEM_SECTION=__attribute__((section("NonCacheable")))
*/

// needed by fsl_flexspi_nor_boot
TU_ATTR_USED const uint8_t dcd_data[] = {0x00};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// unify naming convention
#if !defined(USBPHY1) && defined(USBPHY)
  #define USBPHY1 USBPHY
#endif

static void init_usb_phy(uint8_t usb_id) {
  USBPHY_Type *usb_phy;

  if (usb_id == 0) {
    usb_phy = USBPHY1;
    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
  }
#ifdef USBPHY2
  else if (usb_id == 1) {
    usb_phy = USBPHY2;
    CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
  }
#endif
  else {
    return;
  }

  // Enable PHY support for Low speed device + LS via FS Hub
  usb_phy->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  // TODO may not be needed since it is called within CLOCK_EnableUsbhs0PhyPllClock()
  usb_phy->PWD = 0;

  // TX Timing
  uint32_t phytx = usb_phy->TX;
  phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
  phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
  usb_phy->TX = phytx;
}

#ifdef TRACE_ETM
static void trace_etm_init(void) {
#if defined(CPU_MIMXRT1011DAE5A)
  // Metro M7 rev A "ETM Trace" rework: 4-bit TRACE + TRACE_CLK on the added
  // 2x10 header; the RT1011 has a single mux option per trace signal
  IOMUXC_SetPinMux(IOMUXC_GPIO_AD_00_ARM_TRACE0, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_13_ARM_TRACE1, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_12_ARM_TRACE2, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_11_ARM_TRACE3, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_AD_02_ARM_TRACE_CLK, 0U);
  // fast slew, max speed, high drive
  IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_00_ARM_TRACE0, 0x00F1U);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_13_ARM_TRACE1, 0x00F1U);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_12_ARM_TRACE2, 0x00F1U);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_11_ARM_TRACE3, 0x00F1U);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_02_ARM_TRACE_CLK, 0x00F1U);

  // TRACE_CLK_ROOT already runs 132 MHz (PLL2/4) from BOARD_BootClockRUN,
  // which leaves it gated - just ungate
  CLOCK_EnableClock(kCLOCK_Trace);
#elif defined(CPU_MIMXRT1176DVMAA_cm7)
  // JTAG_nTRST pad is shared with DMIC_DATA1 which drives it low by default and
  // breaks ETM trace - switch the pad to GPIO (MIMXRT1170-EVKB HUG 3.2)
  IOMUXC_SetPinMux(IOMUXC_GPIO_LPSR_10_GPIO12_IO10, 0U);

#ifdef TRACE_ETM_QUIET_ENET_PHY
  // Hold the 100M Ethernet PHY (RTL8201) in reset: its RMII lines are
  // hardwired to the trace pads and drive against the stream at speed. The
  // reset net is a BOARD property (mimxrt1170_evkb: ENET_RST_B =
  // GPIO_LPSR_04), hence the board.h gate.
  IOMUXC_SetPinMux(IOMUXC_GPIO_LPSR_04_GPIO12_IO04, 0U);
  GPIO12->GDIR |= (1U << 4);
  GPIO12->DR &= ~(1U << 4);
#endif

  // TRACE0-3 + TRACE_CLK on GPIO_DISP_B2_02..06, fast slew + high drive
  IOMUXC_SetPinMux(IOMUXC_GPIO_DISP_B2_02_ARM_TRACE00, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_DISP_B2_03_ARM_TRACE01, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_DISP_B2_04_ARM_TRACE02, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_DISP_B2_05_ARM_TRACE03, 0U);
  IOMUXC_SetPinMux(IOMUXC_GPIO_DISP_B2_06_ARM_TRACE_CLK, 0U);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_DISP_B2_02_ARM_TRACE00, IOMUXC_SW_PAD_CTL_PAD_DSE_MASK);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_DISP_B2_03_ARM_TRACE01, IOMUXC_SW_PAD_CTL_PAD_DSE_MASK);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_DISP_B2_04_ARM_TRACE02, IOMUXC_SW_PAD_CTL_PAD_DSE_MASK);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_DISP_B2_05_ARM_TRACE03, IOMUXC_SW_PAD_CTL_PAD_DSE_MASK);
  IOMUXC_SetPinConfig(IOMUXC_GPIO_DISP_B2_06_ARM_TRACE_CLK, IOMUXC_SW_PAD_CTL_PAD_DSE_MASK);

  // 100 MHz CSTRACE root (OscRc400M/4) -> 50 MHz TRACE_CLK pin (= root/2).
  // With the PHY held in reset the CLK line is clean here; 133 MHz root
  // (66 MHz pin) is marginal on this board, stock 132 MHz corrupts.
  CLOCK_SetRootClockMux(kCLOCK_Root_Cstrace, kCLOCK_CSTRACE_ClockRoot_MuxOscRc400M);
  CLOCK_SetRootClockDiv(kCLOCK_Root_Cstrace, 4);
  CLOCK_EnableClock(kCLOCK_Cstrace);

  // Enable the CM7 slave port on the platform trace funnel (E004_3000): the
  // debugger only programs the CSSYS funnel/TPIU it was told about, and this
  // in-between funnel resets with all ports disabled, silently eating the ETM
  // stream. Core-side CoreSight accesses honor the lock, hence the LAR unlock.
  *(volatile uint32_t *) 0xE0043FB0 = 0xC5ACCE55;
  *(volatile uint32_t *) 0xE0043000 |= 1U;
#else
  #error "TRACE_ETM: no trace pin setup for this MCU variant"
#endif
}
#else
  #define trace_etm_init()
#endif

void board_init(void) {
// make sure the dcache is on.
#if defined(__DCACHE_PRESENT) && __DCACHE_PRESENT
  if (SCB_CCR_DC_Msk != (SCB_CCR_DC_Msk & SCB->CCR)) {
    SCB_EnableDCache();
  }
#endif

  BOARD_InitBootPins();
  BOARD_BootClockRUN();
  SystemCoreClockUpdate();

  BOARD_ConfigMPU(); // defined in board.h
  trace_etm_init();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_OTG1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  #ifdef USBPHY2
  NVIC_SetPriority(USB_OTG2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  #endif
#endif

  board_led_write(true);

  // UART
  lpuart_config_t uart_config;
  LPUART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx = true;
  uart_config.enableRx = true;

  if (kStatus_Success != LPUART_Init(UART_PORT, &uart_config, UART_CLK_ROOT)) {
    // failed to init uart, probably baudrate is not supported
    // TU_BREAKPOINT();
  }

  //------------- USB -------------//
  // Note: RT105x RT106x and later have dual USB controllers.
  init_usb_phy(0);// USB0
#ifdef USBPHY2
  init_usb_phy(1);// USB1
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_OTG1_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USB_OTG2_IRQHandler(void) {
  tusb_int_handler(1, true);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinWrite(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;

#if FSL_FEATURE_OCOTP_HAS_TIMING_CTRL
  OCOTP_Init(OCOTP, CLOCK_GetFreq(kCLOCK_IpgClk));
#else
  OCOTP_Init(OCOTP, 0u);
#endif

  // Reads shadow registers 0x01 - 0x04 (Configuration and Manufacturing Info)
  // into 8 bit wide destination, avoiding punning.
  for (int i = 0; i < 4; ++i) {
    uint32_t wr = OCOTP_ReadFuseShadowRegister(OCOTP, i + 1);
    for (int j = 0; j < 4; j++) {
      id[i * 4 + j] = wr & 0xff;
      wr >>= 8;
    }
  }
  OCOTP_Deinit(OCOTP);

  return 16;
}

int board_uart_read(uint8_t *buf, int len) {
  int count = 0;

  while (count < len) {
    uint8_t const rx_count = LPUART_GetRxFifoCount(UART_PORT);
    if (!rx_count) {
      // clear all error flag if any
      uint32_t status_flags = LPUART_GetStatusFlags(UART_PORT);
      status_flags &= (kLPUART_RxOverrunFlag | kLPUART_ParityErrorFlag | kLPUART_FramingErrorFlag |
                       kLPUART_NoiseErrorFlag);
      LPUART_ClearStatusFlags(UART_PORT, status_flags);
      break;
    }

    for (int i = 0; i < rx_count; i++) {
      buf[count] = LPUART_ReadByte(UART_PORT);
      count++;
    }
  }

  return count;
}

int board_uart_write(void const *buf, int len) {
  const uint8_t *p     = (const uint8_t *)buf;
  int            count = 0;
  while (count < len) {
    if (LPUART_GetStatusFlags(UART_PORT) & kLPUART_TxDataRegEmptyFlag) {
      LPUART_WriteByte(UART_PORT, p[count]);
      count++;
    } else {
      break;
    }
  }
  return count;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

#ifdef __clang__
void _exit(int __status) {
  (void) __status;
  while (1) {}
}
#endif
