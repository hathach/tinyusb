#ifndef RP2040_COMMON_H_
#define RP2040_COMMON_H_

#include "pico.h"
#include "hardware/structs/usb.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/timer.h"

#include "pico/critical_section.h"

#include "common/tusb_common.h"
#include "osal/osal.h"
#include "common/tusb_fifo.h"

#if defined(RP2040_USB_HOST_MODE) && defined(RP2040_USB_DEVICE_MODE)
  #error TinyUSB device and host mode not supported at the same time
#endif

#if defined(PICO_RP2040) && PICO_RP2040 == 1
  // RP2040-E2 USB device endpoint abort is not cleared.
  #define CFG_TUSB_RP2_ERRATA_E2 1

  // RP2040-E4: USB host writes to upper half of buffer status in single buffered mode.
  #define CFG_TUSB_RP2_ERRATA_E4 1

  // RP2040-E5: USB device fails to exit RESET state on busy USB bus.
  #if defined(PICO_RP2040_USB_DEVICE_ENUMERATION_FIX) && !defined(TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX)
    #define TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX PICO_RP2040_USB_DEVICE_ENUMERATION_FIX
  #endif

  // RP2040-E15: USB Device controller will hang if certain bus errors occur during an IN transfer.
  #ifndef CFG_TUSB_RP2_ERRATA_E15
    #if defined(PICO_RP2040_USB_DEVICE_UFRAME_FIX)
      #define CFG_TUSB_RP2_ERRATA_E15 (CFG_TUD_ENABLED && PICO_RP2040_USB_DEVICE_UFRAME_FIX)
    #elif defined(TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX)
      #define CFG_TUSB_RP2_ERRATA_E15 (CFG_TUD_ENABLED && TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX)
    #endif
  #endif
#endif

#ifndef CFG_TUSB_RP2_ERRATA_E2
  #define CFG_TUSB_RP2_ERRATA_E2 0
#endif

#ifndef CFG_TUSB_RP2_ERRATA_E4
  #define CFG_TUSB_RP2_ERRATA_E4 0
#endif

#ifndef TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
  #define TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX 0
#endif

#ifndef CFG_TUSB_RP2_ERRATA_E15
  #define CFG_TUSB_RP2_ERRATA_E15 0
#endif

#if CFG_TUSB_RP2_ERRATA_E15
  #undef PICO_RP2040_USB_FAST_IRQ
  #define PICO_RP2040_USB_FAST_IRQ 1
#endif

#ifndef PICO_RP2040_USB_FAST_IRQ
  #define PICO_RP2040_USB_FAST_IRQ 0
#endif

#if PICO_RP2040_USB_FAST_IRQ
  #define __tusb_irq_path_func(x) __no_inline_not_in_flash_func(x)
#else
  #define __tusb_irq_path_func(x) x
#endif

// Flags we set by default in sie_ctrl (we add other bits on top)
enum {
  SIE_CTRL_BASE      = USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS,
  SIE_CTRL_BASE_MASK = USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS | USB_SIE_CTRL_SOF_EN_BITS |
                       USB_SIE_CTRL_KEEP_ALIVE_EN_BITS
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
#define usb_hw_set    ((usb_hw_t *) hw_set_alias_untyped(usb_hw))
#define usb_hw_clear  ((usb_hw_t *) hw_clear_alias_untyped(usb_hw))

#define pico_info(...)  TU_LOG(2, __VA_ARGS__)
#define pico_trace(...) TU_LOG(3, __VA_ARGS__)

enum {
  EPSTATE_IDLE = 0,
  EPSTATE_ACTIVE,
  EPSTATE_PENDING,
  EPSTATE_PENDING_SETUP
};

// Hardware information per endpoint
typedef struct hw_endpoint {
  uint8_t ep_addr;
  uint8_t next_pid;
  uint8_t state;

#if CFG_TUD_EDPT_DEDICATED_HWFIFO
  bool is_xfer_fifo; // transfer using fifo
#endif

#if CFG_TUD_ENABLED
  uint8_t future_bufid; // which buffer holds next data
  uint8_t future_len;   // next data len
#endif

#if CFG_TUSB_RP2_ERRATA_E15
  bool e15_bulk_in; // Errata15 device bulk in
#endif

#if CFG_TUH_ENABLED
  uint8_t dev_addr;
  uint8_t interrupt_num; // 1-15 for interrupt endpoints
  struct TU_ATTR_PACKED {
    uint8_t transfer_type : 2;
    uint8_t need_pre      : 1; // preamble for low-speed device behind full speed hub
  };
#endif

  uint16_t max_packet_size; // max packet size also indicates configured
  uint8_t *dpram_buf;       // Buffer pointer in usb dpram

  // transfer info
  union {
    uint8_t   *user_buf; // User buffer in main memory
    tu_fifo_t *user_fifo;
  };
  uint16_t remaining_len;
  uint16_t xferred_len;

} hw_endpoint_t;

#if CFG_TUSB_RP2_ERRATA_E15
extern volatile uint32_t e15_last_sof;
#endif

void rp2usb_init(void);

// if usb hardware is in host mode
TU_ATTR_ALWAYS_INLINE static inline bool rp2usb_is_host_mode(void) {
  return (usb_hw->main_ctrl & USB_MAIN_CTRL_HOST_NDEVICE_BITS) ? true : false;
}

extern critical_section_t rp2usb_lock;

TU_ATTR_ALWAYS_INLINE static inline void rp2usb_critical_enter(void) {
  critical_section_enter_blocking(&rp2usb_lock);
}
TU_ATTR_ALWAYS_INLINE static inline void rp2usb_critical_exit(void) {
  critical_section_exit(&rp2usb_lock);
}

//--------------------------------------------------------------------+
// Hardware Endpoint
//--------------------------------------------------------------------+
void rp2usb_xfer_start(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t *buffer, tu_fifo_t *ff,
                       uint16_t total_len);
bool rp2usb_xfer_continue(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t buf_id, bool is_rx);
void rp2usb_buffer_start(hw_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, bool is_rx);
void rp2usb_reset_transfer(hw_endpoint_t *ep);


TU_ATTR_ALWAYS_INLINE static inline void hw_endpoint_lock_update(__unused struct hw_endpoint *ep, __unused int delta) {
  // todo add critsec as necessary to prevent issues between worker and IRQ...
  //  note that this is perhaps as simple as disabling IRQs because it would make
  //  sense to have worker and IRQ on same core, however I think using critsec is about equivalent.
}

//--------------------------------------------------------------------+
// Hardware Buffer
//--------------------------------------------------------------------+
void bufctrl_write32(io_rw_32 *buf_reg, uint32_t value);
void bufctrl_write16(io_rw_16 *buf_reg16, uint16_t value);
uint16_t bufctrl_prepare16(hw_endpoint_t *ep, uint8_t *dpram_buf, bool is_rx);

TU_ATTR_ALWAYS_INLINE static inline uintptr_t hw_data_offset(uint8_t *buf) {
  // Remove usb base from buffer pointer
  return (uintptr_t)buf ^ (uintptr_t)usb_dpram;
}

#endif
