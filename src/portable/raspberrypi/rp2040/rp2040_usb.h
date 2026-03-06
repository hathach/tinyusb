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
  bool    is_xfer_fifo; // transfer using fifo

#if TUD_OPT_RP2040_USB_DEVICE_UFRAME_FIX
  bool    e15_bulk_in; // Errata15 device bulk in
  uint8_t pending;     // Transfer scheduled but not active
#endif

#if CFG_TUH_ENABLED
  bool    configured;    // Is this a valid struct
  uint8_t dev_addr;
  uint8_t interrupt_num; // for host interrupt endpoints
#endif

  uint16_t wMaxPacketSize;
  uint8_t *hw_data_buf; // Buffer pointer in usb dpram

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

void hw_endpoint_xfer_start(struct hw_endpoint *ep, uint8_t *buffer, tu_fifo_t *ff, uint16_t total_len);
bool hw_endpoint_xfer_continue(struct hw_endpoint *ep);
void hw_endpoint_reset_transfer(struct hw_endpoint *ep);
void hw_endpoint_start_next_buffer(struct hw_endpoint *ep);

TU_ATTR_ALWAYS_INLINE static inline void hw_endpoint_lock_update(__unused struct hw_endpoint * ep, __unused int delta) {
  // todo add critsec as necessary to prevent issues between worker and IRQ...
  //  note that this is perhaps as simple as disabling IRQs because it would make
  //  sense to have worker and IRQ on same core, however I think using critsec is about equivalent.
}

// #if CFG_TUD_ENABLED
TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *hwep_ctrl_reg_device(struct hw_endpoint *ep) {
  uint8_t const epnum = tu_edpt_number(ep->ep_addr);
  const uint8_t dir   = (uint8_t)tu_edpt_dir(ep->ep_addr);
  if (epnum == 0) {
    // EP0 has no endpoint control register because the buffer offsets are fixed and always enabled
    return NULL;
  }
  return (dir == TUSB_DIR_IN) ? &usb_dpram->ep_ctrl[epnum - 1].in : &usb_dpram->ep_ctrl[epnum - 1].out;
}

TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *hwbuf_ctrl_reg_device(struct hw_endpoint *ep) {
  const uint8_t epnum = tu_edpt_number(ep->ep_addr);
  const uint8_t dir   = (uint8_t)tu_edpt_dir(ep->ep_addr);
  return (dir == TUSB_DIR_IN) ? &usb_dpram->ep_buf_ctrl[epnum].in : &usb_dpram->ep_buf_ctrl[epnum].out;
}
// #endif

#if CFG_TUH_ENABLED
TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *hwep_ctrl_reg_host(struct hw_endpoint *ep) {
  if (tu_edpt_number(ep->ep_addr) == 0) {
    return &usbh_dpram->epx_ctrl;
  }
  return &usbh_dpram->int_ep_ctrl[ep->interrupt_num].ctrl;
}

TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *hwbuf_ctrl_reg_host(struct hw_endpoint *ep) {
  if (tu_edpt_number(ep->ep_addr) == 0) {
    return &usbh_dpram->epx_buf_ctrl;
  }
  return &usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num].ctrl;
}
#endif

//--------------------------------------------------------------------+
//
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
