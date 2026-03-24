#ifndef RP2040_COMMON_H_
#define RP2040_COMMON_H_

#include "pico.h"
#include "hardware/structs/usb.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/timer.h"

#include "common/tusb_common.h"
#include "osal/osal.h"
#include "common/tusb_fifo.h"

#if defined(RP2040_USB_HOST_MODE) && defined(RP2040_USB_DEVICE_MODE)
  #error TinyUSB device and host mode not supported at the same time
#endif

// E5 and E15 only apply to RP2040
#if defined(PICO_RP2040) && PICO_RP2040 == 1
  // RP2040 E5: USB device fails to exit RESET state on busy USB bus.
  #if defined(PICO_RP2040_USB_DEVICE_ENUMERATION_FIX) && !defined(TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX)
    #define TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX PICO_RP2040_USB_DEVICE_ENUMERATION_FIX
  #endif

  // RP2040 E15: USB Device controller will hang if certain bus errors occur during an IN transfer.
  #if defined(PICO_RP2040_USB_DEVICE_UFRAME_FIX) && !defined(TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX)
    #define TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX PICO_RP2040_USB_DEVICE_UFRAME_FIX
  #endif

  #define CFG_TUSB_RP2040_ERRATA_E4_FIX 1
#endif

#ifndef TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
  #define TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX 0
#endif

#ifndef TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
  #define TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX 0
#endif

#if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
  #undef PICO_RP2040_USB_FAST_IRQ
  #define PICO_RP2040_USB_FAST_IRQ 1
#endif

#ifndef PICO_RP2040_USB_FAST_IRQ
#define PICO_RP2040_USB_FAST_IRQ 0
#endif

#ifndef CFG_TUSB_RP2040_ERRATA_E4_FIX
#define CFG_TUSB_RP2040_ERRATA_E4_FIX 0
#endif

#if PICO_RP2040_USB_FAST_IRQ
#define __tusb_irq_path_func(x) __no_inline_not_in_flash_func(x)
#else
#define __tusb_irq_path_func(x) x
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
#define usb_hw_set    ((usb_hw_t *) hw_set_alias_untyped(usb_hw))
#define usb_hw_clear  ((usb_hw_t *) hw_clear_alias_untyped(usb_hw))

#define pico_info(...)  TU_LOG(2, __VA_ARGS__)
#define pico_trace(...) TU_LOG(3, __VA_ARGS__)

// Hardware information per endpoint
typedef struct hw_endpoint {
  uint8_t ep_addr;
  uint8_t next_pid;
  bool    active;       // transferring data
  uint8_t pending;      // Transfer scheduled but not active
  bool    is_xfer_fifo; // transfer using fifo

#if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
  bool    e15_bulk_in; // Errata15 device bulk in
#endif

#if CFG_TUH_ENABLED
  uint8_t dev_addr;
  uint8_t interrupt_num; // 1-15 for interrupt endpoints
  uint8_t transfer_type;
  bool    need_pre;      // need preamble for low speed device behind full speed hub
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

#if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
extern volatile uint32_t e15_last_sof;
#endif

void rp2usb_init(void);

// if usb hardware is in host mode
TU_ATTR_ALWAYS_INLINE static inline bool rp2usb_is_host_mode(void) {
  return (usb_hw->main_ctrl & USB_MAIN_CTRL_HOST_NDEVICE_BITS) ? true : false;
}

//--------------------------------------------------------------------+
// Hardware Endpoint
//--------------------------------------------------------------------+
void hw_endpoint_xfer_start(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t *buffer, tu_fifo_t *ff,
                            uint16_t total_len);
bool hw_endpoint_xfer_continue(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg, uint8_t buf_id);
void hw_endpoint_buffer_xact(struct hw_endpoint *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg);
void hw_endpoint_reset_transfer(struct hw_endpoint *ep);

TU_ATTR_ALWAYS_INLINE static inline void hw_endpoint_lock_update(__unused struct hw_endpoint * ep, __unused int delta) {
  // todo add critsec as necessary to prevent issues between worker and IRQ...
  //  note that this is perhaps as simple as disabling IRQs because it would make
  //  sense to have worker and IRQ on same core, however I think using critsec is about equivalent.
}

//--------------------------------------------------------------------+
// Hardware Buffer
//--------------------------------------------------------------------+
void hwbuf_ctrl_update(io_rw_32 *buf_ctrl_reg, uint32_t and_mask, uint32_t or_mask);

TU_ATTR_ALWAYS_INLINE static inline void hwbuf_ctrl_set(io_rw_32 *buf_ctrl_reg, uint32_t value) {
  hwbuf_ctrl_update(buf_ctrl_reg, 0, value);
}

TU_ATTR_ALWAYS_INLINE static inline void hwbuf_ctrl_set_mask(io_rw_32 *buf_ctrl_reg, uint32_t value) {
  hwbuf_ctrl_update(buf_ctrl_reg, ~value, value);
}

TU_ATTR_ALWAYS_INLINE static inline void hwbuf_ctrl_clear_mask(io_rw_32 *buf_ctrl_reg, uint32_t value) {
  hwbuf_ctrl_update(buf_ctrl_reg, ~value, 0);
}

static inline uintptr_t hw_data_offset(uint8_t *buf) {
  // Remove usb base from buffer pointer
  return (uintptr_t)buf ^ (uintptr_t)usb_dpram;
}

#endif
