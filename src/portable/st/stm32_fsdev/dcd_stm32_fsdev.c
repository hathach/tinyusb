/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Portions:
 * Copyright (c) 2016 STMicroelectronics
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022 Simon Küppers (skuep)
 * Copyright (c) 2022 HiFiPhile
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

/**********************************************
 * This driver has been tested with the following MCUs:
 *  - F070, F072, L053, F042F6
 *
 * It also should work with minimal changes for any ST MCU with an "USB A"/"PCD"/"HCD" peripheral. This
 *  covers:
 *
 * F04x, F072, F078, 070x6/B      1024 byte buffer
 * F102, F103                      512 byte buffer; no internal D+ pull-up (maybe many more changes?)
 * F302xB/C, F303xB/C, F373        512 byte buffer; no internal D+ pull-up
 * F302x6/8, F302xD/E2, F303xD/E  1024 byte buffer; no internal D+ pull-up
 * L0x2, L0x3                     1024 byte buffer
 * L1                              512 byte buffer
 * L4x2, L4x3                     1024 byte buffer
 * G0                             2048 byte buffer
 *
 * To use this driver, you must:
 * - If you are using a device with crystal-less USB, set up the clock recovery system (CRS)
 * - Remap pins to be D+/D- on devices that they are shared (for example: F042Fx)
 *   - This is different to the normal "alternate function" GPIO interface, needs to go through SYSCFG->CFGRx register
 * - Enable USB clock; Perhaps use __HAL_RCC_USB_CLK_ENABLE();
 * - (Optionally configure GPIO HAL to tell it the USB driver is using the USB pins)
 * - call tusb_init();
 * - periodically call tusb_task();
 *
 * Assumptions of the driver:
 * - You are not using CAN (it must share the packet buffer)
 * - APB clock is >= 10 MHz
 * - On some boards, series resistors are required, but not on others.
 * - On some boards, D+ pull up resistor (1.5kohm) is required, but not on others.
 * - You don't have long-running interrupts; some USB packets must be quickly responded to.
 * - You have the ST CMSIS library linked into the project. HAL is not used.
 *
 * Current driver limitations (i.e., a list of features for you to add):
 * - STALL handled, but not tested.
 *   - Does it work? No clue.
 * - All EP BTABLE buffers are created based on max packet size of first EP opened with that address.
 * - Packet buffer memory is copied in the interrupt.
 *   - This is better for performance, but means interrupts are disabled for longer
 *   - DMA may be the best choice, but it could also be pushed to the USBD task.
 * - No double-buffering
 * - No DMA
 * - Minimal error handling
 *   - Perhaps error interrupts should be reported to the stack, or cause a device reset?
 * - Assumes a single USB peripheral; I think that no hardware has multiple so this is fine.
 * - Add a callback for enabling/disabling the D+ PU on devices without an internal PU.
 * - F3 models use three separate interrupts. I think we could only use the LP interrupt for
 *     everything?  However, the interrupts are configurable so the DisableInt and EnableInt
 *     below functions could be adjusting the wrong interrupts (if they had been reconfigured)
 * - LPM is not used correctly, or at all?
 *
 * USB documentation and Reference implementations
 * - STM32 Reference manuals
 * - STM32 USB Hardware Guidelines AN4879
 *
 * - STM32 HAL (much of this driver is based on this)
 * - libopencm3/lib/stm32/common/st_usbfs_core.c
 * - Keil USB Device http://www.keil.com/pack/doc/mw/USB/html/group__usbd.html
 *
 * - YouTube OpenTechLab 011; https://www.youtube.com/watch?v=4FOkJLp_PUw
 *
 * Advantages over HAL driver:
 * - Tiny (saves RAM, assumes a single USB peripheral)
 *
 * Notes:
 * - The buffer table is allocated as endpoints are opened. The allocation is only
 *   cleared when the device is reset. This may be bad if the USB device needs
 *   to be reconfigured.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_FSDEV)

#include "device/dcd.h"

#ifdef TUP_USBIP_FSDEV_STM32
// Undefine to reduce the dependence on HAL
#undef USE_HAL_DRIVER
#include "portable/st/stm32_fsdev/dcd_stm32_fsdev.h"
#endif

/*****************************************************
 * Configuration
 *****************************************************/

// HW supports max of 8 bidirectional endpoints, but this can be reduced to save RAM
// (8u here would mean 8 IN and 8 OUT)
#ifndef MAX_EP_COUNT
#define MAX_EP_COUNT 8U
#endif

// If sharing with CAN, one can set this to be non-zero to give CAN space where it wants it
// Both of these MUST be a multiple of 2, and are in byte units.
#ifndef DCD_STM32_BTABLE_BASE
#define DCD_STM32_BTABLE_BASE 0U
#endif

#ifndef DCD_STM32_BTABLE_SIZE
#define DCD_STM32_BTABLE_SIZE (FSDEV_PMA_SIZE - DCD_STM32_BTABLE_BASE)
#endif

/***************************************************
 * Checks, structs, defines, function definitions, etc.
 */

TU_VERIFY_STATIC((MAX_EP_COUNT) <= STFSDEV_EP_COUNT, "Only 8 endpoints supported on the hardware");
TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) + (DCD_STM32_BTABLE_SIZE)) <= (FSDEV_PMA_SIZE), "BTABLE does not fit in PMA RAM");
TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) % 8) == 0, "BTABLE base must be aligned to 8 bytes");

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// One of these for every EP IN & OUT, uses a bit of RAM....
typedef struct {
  uint8_t *buffer;
  tu_fifo_t *ff;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_packet_size;
  uint8_t ep_idx;   // index for USB_EPnR register
  bool iso_in_sending; // Workaround for ISO IN EP doesn't have interrupt mask
} xfer_ctl_t;

// EP allocator
typedef struct {
  uint8_t ep_num;
  uint8_t ep_type;
  bool allocated[2];
} ep_alloc_t;

static xfer_ctl_t xfer_status[MAX_EP_COUNT][2];

static ep_alloc_t ep_alloc_status[STFSDEV_EP_COUNT];

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[6];

static uint8_t remoteWakeCountdown; // When wake is requested

//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+

// into the stack.
static void dcd_handle_bus_reset(void);
static void dcd_transmit_packet(xfer_ctl_t *xfer, uint16_t ep_ix);
static bool edpt_xfer(uint8_t rhport, uint8_t ep_addr);
static void dcd_ep_ctr_handler(void);

// PMA allocation/access
static uint16_t ep_buf_ptr; ///< Points to first free memory location
static uint32_t dcd_pma_alloc(uint16_t length, bool dbuf);
static uint8_t dcd_ep_alloc(uint8_t ep_addr, uint8_t ep_type);
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t wNBytes);
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t wNBytes);

static bool dcd_write_packet_memory_ff(tu_fifo_t *ff, uint16_t dst, uint16_t wNBytes);
static bool dcd_read_packet_memory_ff(tu_fifo_t *ff, uint16_t src, uint16_t wNBytes);

//--------------------------------------------------------------------+
// Inline helper
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline xfer_ctl_t *xfer_ctl_ptr(uint32_t ep_addr)
{
  uint8_t epnum = tu_edpt_number(ep_addr);
  uint8_t dir = tu_edpt_dir(ep_addr);
  // Fix -Werror=null-dereference
  TU_ASSERT(epnum < MAX_EP_COUNT, &xfer_status[0][0]);

  return &xfer_status[epnum][dir];
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

void dcd_init(uint8_t rhport)
{
  /* Clocks should already be enabled */
  /* Use __HAL_RCC_USB_CLK_ENABLE(); to enable the clocks before calling this function */

  /* The RM mentions to use a special ordering of PDWN and FRES, but this isn't done in HAL.
   * Here, the RM is followed. */

  for (uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }
  // Perform USB peripheral reset
  USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for (uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }

  USB->CNTR &= ~USB_CNTR_PDWN;

  // Wait startup time, for F042 and F070, this is <= 1 us.
  for (uint32_t i = 0; i < 200; i++) { // should be a few us
    asm("NOP");
  }
  USB->CNTR = 0; // Enable USB

#if !defined(STM32G0) && !defined(STM32H5) // BTABLE register does not exist any more on STM32G0, it is fixed to USB SRAM base address
  USB->BTABLE = DCD_STM32_BTABLE_BASE;
#endif
  USB->ISTR = 0; // Clear pending interrupts

  // Reset endpoints to disabled
  for (uint32_t i = 0; i < STFSDEV_EP_COUNT; i++) {
    // This doesn't clear all bits since some bits are "toggle", but does set the type to DISABLED.
    pcd_set_endpoint(USB, i, 0u);
  }

  USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_ESOFM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
  dcd_handle_bus_reset();

  // Enable pull-up if supported
  if (dcd_connect) {
    dcd_connect(rhport);
  }
}

// Define only on MCU with internal pull-up. BSP can define on MCU without internal PU.
#if defined(USB_BCDR_DPPU)

// Disable internal D+ PU
void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;
  USB->BCDR &= ~(USB_BCDR_DPPU);
}

// Enable internal D+ PU
void dcd_connect(uint8_t rhport)
{
  (void)rhport;
  USB->BCDR |= USB_BCDR_DPPU;
}

#elif defined(SYSCFG_PMC_USB_PU) // works e.g. on STM32L151
// Disable internal D+ PU
void dcd_disconnect(uint8_t rhport)
{
  (void)rhport;
  SYSCFG->PMC &= ~(SYSCFG_PMC_USB_PU);
}

// Enable internal D+ PU
void dcd_connect(uint8_t rhport)
{
  (void)rhport;
  SYSCFG->PMC |= SYSCFG_PMC_USB_PU;
}
#endif

void dcd_sof_enable(uint8_t rhport, bool en)
{
  (void)rhport;
  (void)en;

  if (en) {
    USB->CNTR |= USB_CNTR_SOFM;
  } else {
    USB->CNTR &= ~USB_CNTR_SOFM;
  }
}

// Enable device interrupt
void dcd_int_enable(uint8_t rhport)
{
  (void)rhport;
  // Member here forces write to RAM before allowing ISR to execute
  __DSB();
  __ISB();
#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0 || CFG_TUSB_MCU == OPT_MCU_STM32L4
  NVIC_EnableIRQ(USB_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  NVIC_EnableIRQ(USB_LP_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
// Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
// shared USB/CAN IRQs to separate CAN and USB IRQs.
// This dynamically checks if this remap is active to enable the right IRQs.
#ifdef SYSCFG_CFGR1_USB_IT_RMP
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_EnableIRQ(USB_HP_IRQn);
    NVIC_EnableIRQ(USB_LP_IRQn);
    NVIC_EnableIRQ(USBWakeUp_RMP_IRQn);
  } else
#endif
  {
    NVIC_EnableIRQ(USB_HP_CAN_TX_IRQn);
    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
    NVIC_EnableIRQ(USBWakeUp_IRQn);
  }
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32G4
  NVIC_EnableIRQ(USB_HP_IRQn);
  NVIC_EnableIRQ(USB_LP_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32G0
#ifdef STM32G0B0xx
  NVIC_EnableIRQ(USB_IRQn);
#else
  NVIC_EnableIRQ(USB_UCPD1_2_IRQn);
#endif

#elif CFG_TUSB_MCU == OPT_MCU_STM32H5
  NVIC_EnableIRQ(USB_DRD_FS_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32WB
  NVIC_EnableIRQ(USB_HP_IRQn);
  NVIC_EnableIRQ(USB_LP_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32L5
  NVIC_EnableIRQ(USB_FS_IRQn);

#else
#error Unknown arch in USB driver
#endif
}

// Disable device interrupt
void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;

#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0 || CFG_TUSB_MCU == OPT_MCU_STM32L4
  NVIC_DisableIRQ(USB_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  NVIC_DisableIRQ(USB_LP_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
// Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
// shared USB/CAN IRQs to separate CAN and USB IRQs.
// This dynamically checks if this remap is active to disable the right IRQs.
#ifdef SYSCFG_CFGR1_USB_IT_RMP
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_DisableIRQ(USB_HP_IRQn);
    NVIC_DisableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USBWakeUp_RMP_IRQn);
  } else
#endif
  {
    NVIC_DisableIRQ(USB_HP_CAN_TX_IRQn);
    NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);
    NVIC_DisableIRQ(USBWakeUp_IRQn);
  }
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32G4
  NVIC_DisableIRQ(USB_HP_IRQn);
  NVIC_DisableIRQ(USB_LP_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32G0
#ifdef STM32G0B0xx
  NVIC_DisableIRQ(USB_IRQn);
#else
  NVIC_DisableIRQ(USB_UCPD1_2_IRQn);
#endif

#elif CFG_TUSB_MCU == OPT_MCU_STM32H5
  NVIC_DisableIRQ(USB_DRD_FS_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32WB
  NVIC_DisableIRQ(USB_HP_IRQn);
  NVIC_DisableIRQ(USB_LP_IRQn);

#elif CFG_TUSB_MCU == OPT_MCU_STM32L5
  NVIC_DisableIRQ(USB_FS_IRQn);

#else
#error Unknown arch in USB driver
#endif

  // CMSIS has a membar after disabling interrupts
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void)rhport;
  (void)dev_addr;

  // Respond with status
  dcd_edpt_xfer(rhport, TUSB_DIR_IN_MASK | 0x00, NULL, 0);

  // DCD can only set address after status for this request is complete.
  // do it at dcd_edpt0_status_complete()
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void)rhport;

  USB->CNTR |= USB_CNTR_RESUME;
  remoteWakeCountdown = 4u; // required to be 1 to 15 ms, ESOF should trigger every 1ms.
}

static const tusb_desc_endpoint_t ep0OUT_desc = {
    .bLength = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x00,
    .bmAttributes = {.xfer = TUSB_XFER_CONTROL},
    .wMaxPacketSize = CFG_TUD_ENDPOINT0_SIZE,
    .bInterval = 0
};

static const tusb_desc_endpoint_t ep0IN_desc = {
    .bLength = sizeof(tusb_desc_endpoint_t),
    .bDescriptorType = TUSB_DESC_ENDPOINT,
    .bEndpointAddress = 0x80,
    .bmAttributes = {.xfer = TUSB_XFER_CONTROL},
    .wMaxPacketSize = CFG_TUD_ENDPOINT0_SIZE,
    .bInterval = 0
};

static void dcd_handle_bus_reset(void)
{
  USB->DADDR = 0u; // disable USB peripheral by clearing the EF flag

  for (uint32_t i = 0; i < STFSDEV_EP_COUNT; i++) {
    // Clear EP allocation status
    ep_alloc_status[i].ep_num = 0xFF;
    ep_alloc_status[i].ep_type = 0xFF;
    ep_alloc_status[i].allocated[0] = false;
    ep_alloc_status[i].allocated[1] = false;
  }

  // Reset PMA allocation
  ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8 * MAX_EP_COUNT;

  dcd_edpt_open(0, &ep0OUT_desc);
  dcd_edpt_open(0, &ep0IN_desc);

  USB->DADDR = USB_DADDR_EF; // Set enable flag, and leaving the device address as zero.
}

// Handle CTR interrupt for the TX/IN direction
//
// Upon call, (wIstr & USB_ISTR_DIR) == 0U
static void dcd_ep_ctr_tx_handler(uint32_t wIstr)
{
  uint32_t EPindex = wIstr & USB_ISTR_EP_ID;
  uint32_t wEPRegVal = pcd_get_endpoint(USB, EPindex);
  uint8_t ep_addr = (wEPRegVal & USB_EPADDR_FIELD) | TUSB_DIR_IN_MASK;

  // Verify the CTR_TX bit is set. This was in the ST Micro code,
  // but I'm not sure it's actually necessary?
  if ((wEPRegVal & USB_EP_CTR_TX) == 0U) {
    return;
  }

  /* clear int flag */
  pcd_clear_tx_ep_ctr(USB, EPindex);

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);

  if ((wEPRegVal & USB_EP_TYPE_MASK) == USB_EP_ISOCHRONOUS) {
    // Ignore spurious interrupts that we don't schedule
    // host can send IN token while there is no data to send, since ISO does not have NAK
    // this will result to zero length packet --> trigger interrupt (which cannot be masked)
    if (!xfer->iso_in_sending) {
      return;
    }
    xfer->iso_in_sending = false;

    if (wEPRegVal & USB_EP_DTOG_TX) {
      pcd_set_ep_tx_dbuf0_cnt(USB, EPindex, 0);
    } else {
      pcd_set_ep_tx_dbuf1_cnt(USB, EPindex, 0);
    }
  }

  if ((xfer->total_len != xfer->queued_len)) {
    dcd_transmit_packet(xfer, EPindex);
  } else {
    dcd_event_xfer_complete(0, ep_addr, xfer->total_len, XFER_RESULT_SUCCESS, true);
  }
}

// Handle CTR interrupt for the RX/OUT direction
// Upon call, (wIstr & USB_ISTR_DIR) == 0U
static void dcd_ep_ctr_rx_handler(uint32_t wIstr)
{
#ifdef FSDEV_BUS_32BIT
  /* https://www.st.com/resource/en/errata_sheet/es0561-stm32h503cbebkbrb-device-errata-stmicroelectronics.pdf
   * From STM32H503 errata 2.15.1: Buffer description table update completes after CTR interrupt triggers
   * Description:
   * - During OUT transfers, the correct transfer interrupt (CTR) is triggered a little before the last USB SRAM accesses
   * have completed. If the software responds quickly to the interrupt, the full buffer contents may not be correct.
   * Workaround:
   * - Software should ensure that a small delay is included before accessing the SRAM contents. This delay
   * should be 800 ns in Full Speed mode and 6.4 μs in Low Speed mode
   * - Since H5 can run up to 250Mhz -> 1 cycle = 4ns. Per errata, we need to wait 200 cycles. Though executing code
   * also takes time, so we'll wait 60 cycles (count = 20).
   * - Since Low Speed mode is not supported/popular, we will ignore it for now.
   *
   * Note: this errata also seems to apply to G0, U5, H5 etc.
   */
  volatile uint32_t cycle_count = 20; // defined as PCD_RX_PMA_CNT in stm32 hal_driver
  while (cycle_count > 0U) {
    cycle_count--; // each count take 3 cycles (1 for sub, jump, and compare)
  }
#endif

  uint32_t EPindex = wIstr & USB_ISTR_EP_ID;
  uint32_t wEPRegVal = pcd_get_endpoint(USB, EPindex);
  uint8_t ep_addr = wEPRegVal & USB_EPADDR_FIELD;

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);

  // Verify the CTR_RX bit is set. This was in the ST Micro code,
  // but I'm not sure it's actually necessary?
  if ((wEPRegVal & USB_EP_CTR_RX) == 0U) {
    return;
  }

  if ((ep_addr == 0U) && ((wEPRegVal & USB_EP_SETUP) != 0U)) {
    /* Setup packet */
    uint32_t count = pcd_get_ep_rx_cnt(USB, EPindex);
    // Setup packet should always be 8 bytes. If not, ignore it, and try again.
    if (count == 8) {
      // Must reset EP to NAK (in case it had been stalling) (though, maybe too late here)
      pcd_set_ep_rx_status(USB, 0u, USB_EP_RX_NAK);
      pcd_set_ep_tx_status(USB, 0u, USB_EP_TX_NAK);
#ifdef FSDEV_BUS_32BIT
      dcd_event_setup_received(0, (uint8_t *)(USB_PMAADDR + pcd_get_ep_rx_address(USB, EPindex)), true);
#else
      // The setup_received function uses memcpy, so this must first copy the setup data into
      // user memory, to allow for the 32-bit access that memcpy performs.
      uint8_t userMemBuf[8];
      dcd_read_packet_memory(userMemBuf, pcd_get_ep_rx_address(USB, EPindex), 8);
      dcd_event_setup_received(0, (uint8_t *)userMemBuf, true);
#endif
    }
  } else {
    // Clear RX CTR interrupt flag
    if (ep_addr != 0u) {
      pcd_clear_rx_ep_ctr(USB, EPindex);
    }

    uint32_t count;
    uint16_t addr;
    /* Read from correct register when ISOCHRONOUS (double buffered) */
    if ((wEPRegVal & USB_EP_TYPE_MASK) == USB_EP_ISOCHRONOUS) {
      if (wEPRegVal & USB_EP_DTOG_RX) {
        count = pcd_get_ep_dbuf0_cnt(USB, EPindex);
        addr = pcd_get_ep_dbuf0_address(USB, EPindex);
      } else {
        count = pcd_get_ep_dbuf1_cnt(USB, EPindex);
        addr = pcd_get_ep_dbuf1_address(USB, EPindex);
      }
    } else {
      count = pcd_get_ep_rx_cnt(USB, EPindex);
      addr = pcd_get_ep_rx_address(USB, EPindex);
    }

    TU_ASSERT(count <= xfer->max_packet_size, /**/);

    if (count != 0U) {
      if (xfer->ff) {
        dcd_read_packet_memory_ff(xfer->ff, addr, count);
      } else {
        dcd_read_packet_memory(&(xfer->buffer[xfer->queued_len]), addr, count);
      }

      xfer->queued_len = (uint16_t)(xfer->queued_len + count);
    }

    if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len)) {
      // all bytes received or short packet
      dcd_event_xfer_complete(0, ep_addr, xfer->queued_len, XFER_RESULT_SUCCESS, true);
    } else {
      /* Set endpoint active again for receiving more data.
       * Note that isochronous endpoints stay active always */
      if ((wEPRegVal & USB_EP_TYPE_MASK) != USB_EP_ISOCHRONOUS) {
        uint16_t remaining = xfer->total_len - xfer->queued_len;
        uint16_t cnt = tu_min16(remaining, xfer->max_packet_size);
        pcd_set_ep_rx_cnt(USB, EPindex, cnt);
      }
      pcd_set_ep_rx_status(USB, EPindex, USB_EP_RX_VALID);
    }
  }

  // For EP0, prepare to receive another SETUP packet.
  // Clear CTR last so that a new packet does not overwrite the packing being read.
  // (Based on the docs, it seems SETUP will always be accepted after CTR is cleared)
  if (ep_addr == 0u) {
    // Always be prepared for a status packet...
    pcd_set_ep_rx_cnt(USB, EPindex, CFG_TUD_ENDPOINT0_SIZE);
    pcd_clear_rx_ep_ctr(USB, EPindex);
  }
}

static void dcd_ep_ctr_handler(void)
{
  uint32_t wIstr;

  /* stay in loop while pending interrupts */
  while (((wIstr = USB->ISTR) & USB_ISTR_CTR) != 0U) {
    if ((wIstr & USB_ISTR_DIR) == 0U) {
      /* TX/IN */
      dcd_ep_ctr_tx_handler(wIstr);
    } else {
      /* RX/OUT*/
      dcd_ep_ctr_rx_handler(wIstr);
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{

  (void)rhport;

  uint32_t int_status = USB->ISTR;
  // const uint32_t handled_ints = USB_ISTR_CTR | USB_ISTR_RESET | USB_ISTR_WKUP
  //     | USB_ISTR_SUSP | USB_ISTR_SOF | USB_ISTR_ESOF;
  //  unused IRQs: (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_L1REQ )

  // The ST driver loops here on the CTR bit, but that loop has been moved into the
  // dcd_ep_ctr_handler(), so less need to loop here. The other interrupts shouldn't
  // be triggered repeatedly.

  /* Put SOF flag at the beginning of ISR in case to get least amount of jitter if it is used for timing purposes */
  if (int_status & USB_ISTR_SOF) {
    USB->ISTR = (fsdev_bus_t)~USB_ISTR_SOF;
    dcd_event_sof(0, USB->FNR & USB_FNR_FN, true);
  }

  if (int_status & USB_ISTR_RESET) {
    // USBRST is start of reset.
    USB->ISTR = (fsdev_bus_t)~USB_ISTR_RESET;
    dcd_handle_bus_reset();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
    return; // Don't do the rest of the things here; perhaps they've been cleared?
  }

  if (int_status & USB_ISTR_CTR) {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    dcd_ep_ctr_handler();
  }

  if (int_status & USB_ISTR_WKUP) {
    USB->CNTR &= ~USB_CNTR_LPMODE;
    USB->CNTR &= ~USB_CNTR_FSUSP;

    USB->ISTR = (fsdev_bus_t)~USB_ISTR_WKUP;
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  if (int_status & USB_ISTR_SUSP) {
    /* Suspend is asserted for both suspend and unplug events. without Vbus monitoring,
     * these events cannot be differentiated, so we only trigger suspend. */

    /* Force low-power mode in the macrocell */
    USB->CNTR |= USB_CNTR_FSUSP;
    USB->CNTR |= USB_CNTR_LPMODE;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    USB->ISTR = (fsdev_bus_t)~USB_ISTR_SUSP;
    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  if (int_status & USB_ISTR_ESOF) {
    if (remoteWakeCountdown == 1u) {
      USB->CNTR &= ~USB_CNTR_RESUME;
    }
    if (remoteWakeCountdown > 0u) {
      remoteWakeCountdown--;
    }
    USB->ISTR = (fsdev_bus_t)~USB_ISTR_ESOF;
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const *request)
{
  (void)rhport;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS) {
    uint8_t const dev_addr = (uint8_t)request->wValue;

    // Setting new address after the whole request is complete
    USB->DADDR &= ~USB_DADDR_ADD;
    USB->DADDR |= dev_addr; // leave the enable bit set
  }
}

/***
 * Allocate a section of PMA
 * In case of double buffering, high 16bit is the address of 2nd buffer
 * During failure, TU_ASSERT is used. If this happens, rework/reallocate memory manually.
 */
static uint32_t dcd_pma_alloc(uint16_t length, bool dbuf)
{
  // Ensure allocated buffer is aligned
#ifdef FSDEV_BUS_32BIT
  length = (length + 3) & ~0x03;
#else
  length = (length + 1) & ~0x01;
#endif

  uint32_t addr = ep_buf_ptr;
  ep_buf_ptr = (uint16_t)(ep_buf_ptr + length); // increment buffer pointer

  if (dbuf) {
    addr |= ((uint32_t)ep_buf_ptr) << 16;
    ep_buf_ptr = (uint16_t)(ep_buf_ptr + length); // increment buffer pointer
  }

  // Verify packet buffer is not overflowed
  TU_ASSERT(ep_buf_ptr <= FSDEV_PMA_SIZE, 0xFFFF);

  return addr;
}

/***
 * Allocate hardware endpoint
 */
static uint8_t dcd_ep_alloc(uint8_t ep_addr, uint8_t ep_type)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir = tu_edpt_dir(ep_addr);

  for (uint8_t i = 0; i < STFSDEV_EP_COUNT; i++) {
    // Check if already allocated
    if (ep_alloc_status[i].allocated[dir] &&
        ep_alloc_status[i].ep_type == ep_type &&
        ep_alloc_status[i].ep_num == epnum) {
      return i;
    }

    // If EP of current direction is not allocated
    // Except for ISO endpoint, both direction should be free
    if (!ep_alloc_status[i].allocated[dir] &&
        (ep_type != TUSB_XFER_ISOCHRONOUS || !ep_alloc_status[i].allocated[dir ^ 1])) {
      // Check if EP number is the same
      if (ep_alloc_status[i].ep_num == 0xFF || ep_alloc_status[i].ep_num == epnum) {
        // One EP pair has to be the same type
        if (ep_alloc_status[i].ep_type == 0xFF || ep_alloc_status[i].ep_type == ep_type) {
          ep_alloc_status[i].ep_num = epnum;
          ep_alloc_status[i].ep_type = ep_type;
          ep_alloc_status[i].allocated[dir] = true;

          return i;
        }
      }
    }
  }

  // Allocation failed
  TU_ASSERT(0);
}

// The STM32F0 doesn't seem to like |= or &= to manipulate the EP#R registers,
// so I'm using the #define from HAL here, instead.

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *p_endpoint_desc)
{
  (void)rhport;
  uint8_t const ep_addr = p_endpoint_desc->bEndpointAddress;
  uint8_t const ep_idx = dcd_ep_alloc(ep_addr, p_endpoint_desc->bmAttributes.xfer);
  uint8_t const dir = tu_edpt_dir(ep_addr);
  const uint16_t packet_size = tu_edpt_packet_size(p_endpoint_desc);
  const uint16_t buffer_size = pcd_aligned_buffer_size(packet_size);
  uint16_t pma_addr;
  uint32_t wType;

  TU_ASSERT(ep_idx < STFSDEV_EP_COUNT);
  TU_ASSERT(buffer_size <= 64);

  // Set type
  switch (p_endpoint_desc->bmAttributes.xfer) {
    case TUSB_XFER_CONTROL:
      wType = USB_EP_CONTROL;
      break;
    case TUSB_XFER_BULK:
      wType = USB_EP_CONTROL;
      break;

    case TUSB_XFER_INTERRUPT:
      wType = USB_EP_INTERRUPT;
      break;

    default:
      // Note: ISO endpoint should use alloc / active functions
      TU_ASSERT(false);
  }

  pcd_set_eptype(USB, ep_idx, wType);
  pcd_set_ep_address(USB, ep_idx, tu_edpt_number(ep_addr));

  /* Create a packet memory buffer area. */
  pma_addr = dcd_pma_alloc(buffer_size, false);

  if (dir == TUSB_DIR_IN) {
    pcd_set_ep_tx_address(USB, ep_idx, pma_addr);
    pcd_set_ep_tx_status(USB, ep_idx, USB_EP_TX_NAK);
    pcd_clear_tx_dtog(USB, ep_idx);
  } else {
    pcd_set_ep_rx_address(USB, ep_idx, pma_addr);
    pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_NAK);
    pcd_clear_rx_dtog(USB, ep_idx);
  }

  xfer_ctl_ptr(ep_addr)->max_packet_size = packet_size;
  xfer_ctl_ptr(ep_addr)->ep_idx = ep_idx;

  return true;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  (void)rhport;

  for (uint32_t i = 1; i < STFSDEV_EP_COUNT; i++) {
    // Reset endpoint
    pcd_set_endpoint(USB, i, 0);
    // Clear EP allocation status
    ep_alloc_status[i].ep_num = 0xFF;
    ep_alloc_status[i].ep_type = 0xFF;
    ep_alloc_status[i].allocated[0] = false;
    ep_alloc_status[i].allocated[1] = false;
  }

  // Reset PMA allocation
  ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8 * MAX_EP_COUNT + 2 * CFG_TUD_ENDPOINT0_SIZE;
}

/**
 * Close an endpoint.
 *
 * This function may be called with interrupts enabled or disabled.
 *
 * This also clears transfers in progress, should there be any.
 */
void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);
  uint8_t const ep_idx = xfer->ep_idx;
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_IN) {
    pcd_set_ep_tx_status(USB, ep_idx, USB_EP_TX_DIS);
  } else {
    pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_DIS);
  }
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size)
{
  (void)rhport;

  uint8_t const ep_idx = dcd_ep_alloc(ep_addr, TUSB_XFER_ISOCHRONOUS);
  const uint16_t buffer_size = pcd_aligned_buffer_size(largest_packet_size);

  /* Create a packet memory buffer area. Enable double buffering for devices with 2048 bytes PMA,
     for smaller devices double buffering occupy too much space. */
#if FSDEV_PMA_SIZE > 1024u
  uint32_t pma_addr = dcd_pma_alloc(buffer_size, true);
  uint16_t pma_addr2 = pma_addr >> 16;
#else
  uint32_t pma_addr = dcd_pma_alloc(buffer_size, true);
  uint16_t pma_addr2 = pma_addr;
#endif
  pcd_set_ep_tx_address(USB, ep_idx, pma_addr);
  pcd_set_ep_rx_address(USB, ep_idx, pma_addr2);

  pcd_set_eptype(USB, ep_idx, USB_EP_ISOCHRONOUS);

  xfer_ctl_ptr(ep_addr)->ep_idx = ep_idx;

  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const *p_endpoint_desc)
{
  (void)rhport;
  uint8_t const ep_addr = p_endpoint_desc->bEndpointAddress;
  uint8_t const ep_idx = xfer_ctl_ptr(ep_addr)->ep_idx;
  uint8_t const dir = tu_edpt_dir(ep_addr);
  const uint16_t packet_size = tu_edpt_packet_size(p_endpoint_desc);

  pcd_set_ep_tx_status(USB, ep_idx, USB_EP_TX_DIS);
  pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_DIS);

  pcd_set_ep_address(USB, ep_idx, tu_edpt_number(ep_addr));

  pcd_clear_tx_dtog(USB, ep_idx);
  pcd_clear_rx_dtog(USB, ep_idx);

  if (dir == TUSB_DIR_IN) {
    pcd_rx_dtog(USB, ep_idx);
  } else {
    pcd_tx_dtog(USB, ep_idx);
  }

  xfer_ctl_ptr(ep_addr)->max_packet_size = packet_size;

  return true;
}

// Currently, single-buffered, and only 64 bytes at a time (max)

static void dcd_transmit_packet(xfer_ctl_t *xfer, uint16_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);
  if (len > xfer->max_packet_size) {
    len = xfer->max_packet_size;
  }

  uint16_t ep_reg = pcd_get_endpoint(USB, ep_ix);
  bool const is_iso = (ep_reg & USB_EP_TYPE_MASK) == USB_EP_ISOCHRONOUS;
  uint16_t addr_ptr;

  if (is_iso) {
    if (ep_reg & USB_EP_DTOG_TX) {
      addr_ptr = pcd_get_ep_dbuf1_address(USB, ep_ix);
      pcd_set_ep_tx_dbuf1_cnt(USB, ep_ix, len);
    } else {
      addr_ptr = pcd_get_ep_dbuf0_address(USB, ep_ix);
      pcd_set_ep_tx_dbuf0_cnt(USB, ep_ix, len);
    }
  } else {
    addr_ptr = pcd_get_ep_tx_address(USB, ep_ix);
    pcd_set_ep_tx_cnt(USB, ep_ix, len);
  }

  if (xfer->ff) {
    dcd_write_packet_memory_ff(xfer->ff, addr_ptr, len);
  } else {
    dcd_write_packet_memory(addr_ptr, &(xfer->buffer[xfer->queued_len]), len);
  }
  xfer->queued_len = (uint16_t)(xfer->queued_len + len);

  dcd_int_disable(0);
  pcd_set_ep_tx_status(USB, ep_ix, USB_EP_TX_VALID);
  if (is_iso) {
    xfer->iso_in_sending = true;
  }
  dcd_int_enable(0);
}

static bool edpt_xfer(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);
  uint8_t const ep_idx = xfer->ep_idx;
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_IN) {
    dcd_transmit_packet(xfer, ep_idx);
  } else {
    // A setup token can occur immediately after an OUT STATUS packet so make sure we have a valid
    // buffer for the control endpoint.
    if (ep_idx == 0 && xfer->buffer == NULL) {
      xfer->buffer = (uint8_t *)_setup_packet;
    }

    uint32_t cnt = (uint32_t ) tu_min16(xfer->total_len, xfer->max_packet_size);
    uint16_t ep_reg = pcd_get_endpoint(USB, ep_idx);

    if ((ep_reg & USB_EP_TYPE_MASK) == USB_EP_ISOCHRONOUS) {
      pcd_set_ep_rx_dbuf0_cnt(USB, ep_idx, cnt);
      pcd_set_ep_rx_dbuf1_cnt(USB, ep_idx, cnt);
    } else {
      pcd_set_ep_rx_cnt(USB, ep_idx, cnt);
    }

    pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_VALID);
  }

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes)
{
  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);

  xfer->buffer = buffer;
  xfer->ff = NULL;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  return edpt_xfer(rhport, ep_addr);
}

bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *ff, uint16_t total_bytes)
{
  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);
  xfer->buffer = NULL;
  xfer->ff = ff;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  return edpt_xfer(rhport, ep_addr);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);
  uint8_t const ep_idx = xfer->ep_idx;
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_IN) {
    pcd_set_ep_tx_status(USB, ep_idx, USB_EP_TX_STALL);
  } else {
    pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_STALL);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  xfer_ctl_t *xfer = xfer_ctl_ptr(ep_addr);
  uint8_t const ep_idx = xfer->ep_idx;
  uint8_t const dir = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_IN) { // IN
    if (pcd_get_eptype(USB, ep_idx) != USB_EP_ISOCHRONOUS) {
      pcd_set_ep_tx_status(USB, ep_idx, USB_EP_TX_NAK);
    }

    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_tx_dtog(USB, ep_idx);
  } else { // OUT
    if (pcd_get_eptype(USB, ep_idx) != USB_EP_ISOCHRONOUS) {
      pcd_set_ep_rx_status(USB, ep_idx, USB_EP_RX_NAK);
    }
    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_rx_dtog(USB, ep_idx);
  }
}

#ifdef FSDEV_BUS_32BIT
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t wNBytes)
{
  const uint8_t *srcVal = src;
  volatile uint32_t *dst32 = (volatile uint32_t *)(USB_PMAADDR + dst);

  for (uint32_t n = wNBytes / 4; n > 0; --n) {
    *dst32++ = tu_unaligned_read32(srcVal);
    srcVal += 4;
  }

  wNBytes = wNBytes & 0x03;
  if (wNBytes) {
    uint32_t wrVal = *srcVal;
    wNBytes--;

    if (wNBytes) {
      wrVal |= *++srcVal << 8;
      wNBytes--;

      if (wNBytes) {
        wrVal |= *++srcVal << 16;
      }
    }

    *dst32 = wrVal;
  }

  return true;
}
#else
// Packet buffer access can only be 8- or 16-bit.
/**
 * @brief Copy a buffer from user memory area to packet memory area (PMA).
 *        This uses byte-access for user memory (so support non-aligned buffers)
 *        and 16-bit access for packet memory.
 * @param   dst, byte address in PMA; must be 16-bit aligned
 * @param   src pointer to user memory area.
 * @param   wPMABufAddr address into PMA.
 * @param   wNBytes no. of bytes to be copied.
 * @retval None
 */
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, uint16_t wNBytes)
{
  uint32_t n = (uint32_t)wNBytes >> 1U;
  uint16_t temp1, temp2;
  const uint8_t *srcVal;

  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO uint16_t *pdwVal;

  srcVal = src;
  pdwVal = &pma[FSDEV_PMA_STRIDE * (dst >> 1)];

  while (n--) {
    temp1 = (uint16_t)*srcVal;
    srcVal++;
    temp2 = temp1 | ((uint16_t)(((uint16_t)(*srcVal)) << 8U));
    *pdwVal = temp2;
    pdwVal += FSDEV_PMA_STRIDE;
    srcVal++;
  }

  if (wNBytes) {
    temp1 = *srcVal;
    *pdwVal = temp1;
  }

  return true;
}
#endif

/**
 * @brief Copy from FIFO to packet memory area (PMA).
 *        Uses byte-access of system memory and 16-bit access of packet memory
 * @param   wNBytes no. of bytes to be copied.
 * @retval None
 */
static bool dcd_write_packet_memory_ff(tu_fifo_t *ff, uint16_t dst, uint16_t wNBytes)
{
  // Since we copy from a ring buffer FIFO, a wrap might occur making it necessary to conduct two copies
  tu_fifo_buffer_info_t info;
  tu_fifo_get_read_info(ff, &info);

  uint16_t cnt_lin = TU_MIN(wNBytes, info.len_lin);
  uint16_t cnt_wrap = TU_MIN(wNBytes - cnt_lin, info.len_wrap);

  // We want to read from the FIFO and write it into the PMA, if LIN part is ODD and has WRAPPED part,
  // last lin byte will be combined with wrapped part
  // To ensure PMA is always access aligned (dst aligned to 16 or 32 bit)
#ifdef FSDEV_BUS_32BIT
  if ((cnt_lin & 0x03) && cnt_wrap) {
    // Copy first linear part
    dcd_write_packet_memory(dst, info.ptr_lin, cnt_lin & ~0x03);
    dst += cnt_lin & ~0x03;

    // Copy last linear bytes & first wrapped bytes to buffer
    uint32_t i;
    uint8_t tmp[4];
    for (i = 0; i < (cnt_lin & 0x03); i++) {
      tmp[i] = ((uint8_t *)info.ptr_lin)[(cnt_lin & ~0x03) + i];
    }
    uint32_t wCnt = cnt_wrap;
    for (; i < 4 && wCnt > 0; i++, wCnt--) {
      tmp[i] = *(uint8_t *)info.ptr_wrap;
      info.ptr_wrap = (uint8_t *)info.ptr_wrap + 1;
    }

    // Write unaligned buffer
    dcd_write_packet_memory(dst, &tmp, 4);
    dst += 4;

    // Copy rest of wrapped byte
    if (wCnt)
      dcd_write_packet_memory(dst, info.ptr_wrap, wCnt);
  }
#else
  if ((cnt_lin & 0x01) && cnt_wrap) {
    // Copy first linear part
    dcd_write_packet_memory(dst, info.ptr_lin, cnt_lin & ~0x01);
    dst += cnt_lin & ~0x01;

    // Copy last linear byte & first wrapped byte
    uint16_t tmp = ((uint8_t *)info.ptr_lin)[cnt_lin - 1] | ((uint16_t)(((uint8_t *)info.ptr_wrap)[0]) << 8U);
    dcd_write_packet_memory(dst, &tmp, 2);
    dst += 2;

    // Copy rest of wrapped byte
    dcd_write_packet_memory(dst, ((uint8_t *)info.ptr_wrap) + 1, cnt_wrap - 1);
  }
#endif
  else {
    // Copy linear part
    dcd_write_packet_memory(dst, info.ptr_lin, cnt_lin);
    dst += info.len_lin;

    if (info.len_wrap) {
      // Copy wrapped byte
      dcd_write_packet_memory(dst, info.ptr_wrap, cnt_wrap);
    }
  }

  tu_fifo_advance_read_pointer(ff, cnt_lin + cnt_wrap);

  return true;
}

#ifdef FSDEV_BUS_32BIT
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t wNBytes)
{
  uint8_t *dstVal = dst;
  volatile uint32_t *src32 = (volatile uint32_t *)(USB_PMAADDR + src);

  for (uint32_t n = wNBytes / 4; n > 0; --n) {
    tu_unaligned_write32(dstVal, *src32++);
    dstVal += 4;
  }

  wNBytes = wNBytes & 0x03;
  if (wNBytes) {
    uint32_t rdVal = *src32;

    *dstVal = tu_u32_byte0(rdVal);
    wNBytes--;

    if (wNBytes) {
      *++dstVal = tu_u32_byte1(rdVal);
      wNBytes--;

      if (wNBytes) {
        *++dstVal = tu_u32_byte2(rdVal);
      }
    }
  }

  return true;
}
#else
/**
 * @brief Copy a buffer from packet memory area (PMA) to user memory area.
 *        Uses byte-access of system memory and 16-bit access of packet memory
 * @param   wNBytes no. of bytes to be copied.
 * @retval None
 */
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, uint16_t wNBytes)
{
  uint32_t n = (uint32_t)wNBytes >> 1U;
  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO const uint16_t *pdwVal;
  uint32_t temp;

  pdwVal = &pma[FSDEV_PMA_STRIDE * (src >> 1)];
  uint8_t *dstVal = (uint8_t *)dst;

  while (n--) {
    temp = *pdwVal;
    pdwVal += FSDEV_PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
    *dstVal++ = ((temp >> 8) & 0xFF);
  }

  if (wNBytes & 0x01) {
    temp = *pdwVal;
    pdwVal += FSDEV_PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
  }
  return true;
}
#endif

/**
 * @brief Copy a buffer from user packet memory area (PMA) to FIFO.
 *        Uses byte-access of system memory and 16-bit access of packet memory
 * @param   wNBytes no. of bytes to be copied.
 * @retval None
 */
static bool dcd_read_packet_memory_ff(tu_fifo_t *ff, uint16_t src, uint16_t wNBytes)
{
  // Since we copy into a ring buffer FIFO, a wrap might occur making it necessary to conduct two copies
  // Check for first linear part
  tu_fifo_buffer_info_t info;
  tu_fifo_get_write_info(ff, &info); // We want to read from the FIFO

  uint16_t cnt_lin = TU_MIN(wNBytes, info.len_lin);
  uint16_t cnt_wrap = TU_MIN(wNBytes - cnt_lin, info.len_wrap);

  // We want to read from PMA and write it into the FIFO, if LIN part is ODD and has WRAPPED part,
  // last lin byte will be combined with wrapped part
  // To ensure PMA is always access aligned (src aligned to 16 or 32 bit)
#ifdef FSDEV_BUS_32BIT
  if ((cnt_lin & 0x03) && cnt_wrap) {
    // Copy first linear part
    dcd_read_packet_memory(info.ptr_lin, src, cnt_lin & ~0x03);
    src += cnt_lin & ~0x03;

    // Copy last linear bytes & first wrapped bytes
    uint8_t tmp[4];
    dcd_read_packet_memory(tmp, src, 4);
    src += 4;

    uint32_t i;
    for (i = 0; i < (cnt_lin & 0x03); i++) {
      ((uint8_t *)info.ptr_lin)[(cnt_lin & ~0x03) + i] = tmp[i];
    }
    uint32_t wCnt = cnt_wrap;
    for (; i < 4 && wCnt > 0; i++, wCnt--) {
      *(uint8_t *)info.ptr_wrap = tmp[i];
      info.ptr_wrap = (uint8_t *)info.ptr_wrap + 1;
    }

    // Copy rest of wrapped byte
    if (wCnt)
      dcd_read_packet_memory(info.ptr_wrap, src, wCnt);
  }
#else
  if ((cnt_lin & 0x01) && cnt_wrap) {
    // Copy first linear part
    dcd_read_packet_memory(info.ptr_lin, src, cnt_lin & ~0x01);
    src += cnt_lin & ~0x01;

    // Copy last linear byte & first wrapped byte
    uint8_t tmp[2];
    dcd_read_packet_memory(tmp, src, 2);
    src += 2;

    ((uint8_t *)info.ptr_lin)[cnt_lin - 1] = tmp[0];
    ((uint8_t *)info.ptr_wrap)[0] = tmp[1];

    // Copy rest of wrapped byte
    dcd_read_packet_memory(((uint8_t *)info.ptr_wrap) + 1, src, cnt_wrap - 1);
  }
#endif
  else {
    // Copy linear part
    dcd_read_packet_memory(info.ptr_lin, src, cnt_lin);
    src += cnt_lin;

    if (info.len_wrap) {
      // Copy wrapped byte
      dcd_read_packet_memory(info.ptr_wrap, src, cnt_wrap);
    }
  }

  tu_fifo_advance_write_pointer(ff, cnt_lin + cnt_wrap);

  return true;
}

#endif
