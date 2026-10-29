/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED || CFG_TUD_ENABLED

#include "tusb.h"
#include "common/tusb_private.h"

#if CFG_TUD_ENABLED
#include "device/usbd_pvt.h"
#endif

#if CFG_TUH_ENABLED
#include "host/usbh_pvt.h"
#endif

// Suppress IAR warning
// Warning[Pe111]: statement is unreachable
#if defined(__ICCARM__)
#pragma diag_suppress = Pe111
#endif

tusb_role_t _tusb_rhport_role[TUP_USBIP_CONTROLLER_NUM] = { TUSB_ROLE_INVALID };

//--------------------------------------------------------------------
// Weak/Default API, can be overwritten by Application
//--------------------------------------------------------------------

#if CFG_TUSB_OS != OPT_OS_NONE
TU_ATTR_WEAK uint32_t tusb_time_millis_api(void) {
  return osal_time_millis();
}

TU_ATTR_WEAK void tusb_time_delay_ms_api(uint32_t ms) {
  osal_task_delay(ms);
}

#else
// tusb_time_millis_api() must be implemented by user application.

TU_ATTR_WEAK void tusb_time_delay_ms_api(uint32_t ms) {
  // delay using millis()
  const uint32_t time_ms = tusb_time_millis_api();
  while ((tusb_time_millis_api() - time_ms) < ms) {}
}
#endif

TU_ATTR_WEAK void *tusb_app_virt_to_phys(void *virt_addr) {
  return virt_addr;
}

TU_ATTR_WEAK void* tusb_app_phys_to_virt(void *phys_addr) {
  return phys_addr;
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+
bool tusb_rhport_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  //  backward compatible called with tusb_init(void)
  #if defined(TUD_OPT_RHPORT) || defined(TUH_OPT_RHPORT)
  if (rh_init == NULL) {
    #if CFG_TUD_ENABLED && defined(TUD_OPT_RHPORT)
    // init device stack CFG_TUSB_RHPORTx_MODE must be defined
    const tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
    };
    TU_ASSERT ( tud_rhport_init(TUD_OPT_RHPORT, &dev_init) );
    _tusb_rhport_role[TUD_OPT_RHPORT] = TUSB_ROLE_DEVICE;
    #endif

    #if CFG_TUH_ENABLED && defined(TUH_OPT_RHPORT)
    // init host stack CFG_TUSB_RHPORTx_MODE must be defined
    const tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUH_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
    };
    TU_ASSERT( tuh_rhport_init(TUH_OPT_RHPORT, &host_init) );
    _tusb_rhport_role[TUH_OPT_RHPORT] = TUSB_ROLE_HOST;
    #endif

    return true;
  }
  #endif

  // new API with explicit rhport and role
  TU_ASSERT(rhport < TUP_USBIP_CONTROLLER_NUM && rh_init->role != TUSB_ROLE_INVALID);
  _tusb_rhport_role[rhport] = rh_init->role;

  #if CFG_TUD_ENABLED
  if (rh_init->role == TUSB_ROLE_DEVICE) {
    TU_ASSERT(tud_rhport_init(rhport, rh_init));
  }
  #endif

  #if CFG_TUH_ENABLED
  if (rh_init->role == TUSB_ROLE_HOST) {
    TU_ASSERT(tuh_rhport_init(rhport, rh_init));
  }
  #endif

  return true;
}

bool tusb_inited(void) {
  bool ret = false;

  #if CFG_TUD_ENABLED
  if (tud_inited()) {
    ret = true;
  }
  #endif

  #if CFG_TUH_ENABLED
  if (tuh_inited()) {
    ret = true;
  }
  #endif

  return ret;
}

void tusb_int_handler(uint8_t rhport, bool in_isr) {
  TU_VERIFY(rhport < TUP_USBIP_CONTROLLER_NUM,);
  // ports may call this from task context (in_isr false): no span here then (a port's ISR body may record its own)
  bool sv_span = in_isr;
#if CFG_TUD_ENABLED && CFG_TUH_ENABLED && (TUD_SYSVIEW_ISR_TRACED != TUH_SYSVIEW_ISR_TRACED)
  // dual-role with one role traced: the other role's interrupt records no span
  sv_span = sv_span && (_tusb_rhport_role[rhport] == (TUD_SYSVIEW_ISR_TRACED ? TUSB_ROLE_DEVICE : TUSB_ROLE_HOST));
#endif
  if (sv_span) { TU_SYSVIEW_ISR_ENTER(); }

  #if CFG_TUD_ENABLED
  if (_tusb_rhport_role[rhport] == TUSB_ROLE_DEVICE) {
    (void) in_isr;
    dcd_int_handler(rhport);
  }
  #endif

  #if CFG_TUH_ENABLED
  if (_tusb_rhport_role[rhport] == TUSB_ROLE_HOST) {
    hcd_int_handler(rhport, in_isr);
  }
  #endif

  if (sv_span) { TU_SYSVIEW_ISR_EXIT(); }
}

bool tusb_deinit(uint8_t rhport) {
  TU_VERIFY(rhport < TUP_USBIP_CONTROLLER_NUM);
  bool ret = false;

  #if CFG_TUD_ENABLED
  if (_tusb_rhport_role[rhport] == TUSB_ROLE_DEVICE) {
    TU_ASSERT(tud_deinit(rhport));
    _tusb_rhport_role[rhport] = TUSB_ROLE_INVALID;
    ret = true;
  }
  #endif

  #if CFG_TUH_ENABLED
  if (_tusb_rhport_role[rhport] == TUSB_ROLE_HOST) {
    TU_ASSERT(tuh_deinit(rhport));
    _tusb_rhport_role[rhport] = TUSB_ROLE_INVALID;
    ret = true;
  }
  #endif

  return ret;
}

//--------------------------------------------------------------------+
// Descriptor helper
//--------------------------------------------------------------------+

uint8_t const* tu_desc_find(uint8_t const* desc, uint8_t const* end, uint8_t byte1) {
  while (desc + 1 < end) {
    if (desc[1] == byte1) {
      return desc;
    }
    desc += desc[DESC_OFFSET_LEN];
  }
  return NULL;
}

uint8_t const* tu_desc_find2(uint8_t const* desc, uint8_t const* end, uint8_t byte1, uint8_t byte2) {
  while (desc + 2 < end) {
    if (desc[1] == byte1 && desc[2] == byte2) {
      return desc;
    }
    desc += desc[DESC_OFFSET_LEN];
  }
  return NULL;
}

uint8_t const* tu_desc_find3(uint8_t const* desc, uint8_t const* end, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
  while (desc + 3 < end) {
    if (desc[1] == byte1 && desc[2] == byte2 && desc[3] == byte3) {
      return desc;
    }
    desc += desc[DESC_OFFSET_LEN];
  }
  return NULL;
}

//--------------------------------------------------------------------+
// Endpoint Helper for both Host and Device stack
//--------------------------------------------------------------------+

bool tu_edpt_claim(volatile uint8_t* ep_state, osal_mutex_t mutex) {
  (void) mutex;

  // pre-check to help reducing mutex lock
  TU_VERIFY((*ep_state & (TU_EDPT_STATE_BUSY | TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_RX_PENDING)) == 0);
  (void) osal_mutex_lock(mutex, OSAL_TIMEOUT_WAIT_FOREVER);

  // can only claim the endpoint if it is not busy, not claimed yet and its received data is consumed.
  bool const available = (*ep_state & (TU_EDPT_STATE_BUSY | TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_RX_PENDING)) == 0;
  if (available) {
    *ep_state |= TU_EDPT_STATE_CLAIMED;
  }

  (void) osal_mutex_unlock(mutex);
  return available;
}

bool tu_edpt_release(volatile uint8_t* ep_state, osal_mutex_t mutex) {
  (void) mutex;
  (void) osal_mutex_lock(mutex, OSAL_TIMEOUT_WAIT_FOREVER);

  // can only release the endpoint if it is claimed and not busy
  bool const ret = (*ep_state & (TU_EDPT_STATE_CLAIMED | TU_EDPT_STATE_BUSY)) == TU_EDPT_STATE_CLAIMED;
  if (ret) {
    *ep_state &= (uint8_t) ~TU_EDPT_STATE_CLAIMED;
  }

  (void) osal_mutex_unlock(mutex);
  return ret;
}

#if CFG_TUSB_DEBUG
bool tu_edpt_validate(const tusb_desc_endpoint_t *desc_ep, tusb_speed_t speed) {
  const uint16_t max_packet_size = tu_edpt_packet_size(desc_ep);
  TU_LOG2("  Open EP %02X with Size = %u\r\n", desc_ep->bEndpointAddress, max_packet_size);
  TU_ASSERT(max_packet_size > 0);

  switch (desc_ep->bmAttributes.xfer) {
    case TUSB_XFER_ISOCHRONOUS: {
      const uint16_t spec_size = (speed == TUSB_SPEED_HIGH ? 1024 : 1023);
      TU_ASSERT(max_packet_size <= spec_size);
      break;
    }

    case TUSB_XFER_BULK:
      if (speed == TUSB_SPEED_HIGH) {
        // Bulk highspeed must be EXACTLY 512
        TU_ASSERT(max_packet_size == 512);
      } else {
        // Bulk fullspeed can only be 8, 16, 32, 64
        TU_ASSERT(max_packet_size == 8 || max_packet_size == 16 || max_packet_size == 32 || max_packet_size == 64);
      }
      break;

    case TUSB_XFER_INTERRUPT: {
      const uint16_t spec_size = (speed == TUSB_SPEED_HIGH ? 1024 : 64);
      TU_ASSERT(max_packet_size <= spec_size);
      break;
    }

    default:
      return false;
  }

  return true;
}
#endif

bool tu_bind_driver_to_ep_itf(uint8_t driver_id, uint8_t ep2drv[][2], uint8_t itf2drv[], uint8_t itf_max,
                              uint8_t ep_max, const uint8_t *p_desc, uint16_t desc_len) {
  const uint8_t *desc_end = p_desc + desc_len;
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    const uint8_t desc_type = tu_desc_type(p_desc);

    if (desc_type == TUSB_DESC_ENDPOINT) {
      const uint8_t ep_addr  = ((const tusb_desc_endpoint_t *)p_desc)->bEndpointAddress;
      const uint8_t ep_num   = tu_edpt_number(ep_addr);
      const uint8_t ep_dir   = tu_edpt_dir(ep_addr);
      TU_ASSERT(ep_num < ep_max);
      ep2drv[ep_num][ep_dir] = driver_id;
    } else if (desc_type == TUSB_DESC_INTERFACE) {
      const tusb_desc_interface_t *desc_itf = (const tusb_desc_interface_t *)p_desc;
      if (desc_itf->bAlternateSetting == 0) {
        TU_ASSERT(desc_itf->bInterfaceNumber < itf_max);
        itf2drv[desc_itf->bInterfaceNumber] = driver_id;
      }
    }

    p_desc = tu_desc_next(p_desc);
  }
  return true;
}

//--------------------------------------------------------------------+
// Endpoint Stream Helper for both Host and Device stack
//--------------------------------------------------------------------+

bool tu_edpt_stream_init(tu_edpt_stream_t *s, bool is_host, bool is_tx, bool overwritable, void *ff_buf,
                         uint16_t ff_bufsize, uint8_t *ep_buf) {
  (void) is_tx;

  if (ff_buf == NULL || ff_bufsize == 0) {
    return false;
  }

  s->is_host = is_host;
  tu_fifo_config(&s->ff, ff_buf, ff_bufsize, overwritable);

  #if OSAL_MUTEX_REQUIRED
  if (ff_buf != NULL && ff_bufsize > 0) {
    osal_mutex_t new_mutex = osal_mutex_create(&s->ff_mutexdef);
    tu_fifo_config_mutex(&s->ff, is_tx ? new_mutex : NULL, is_tx ? NULL : new_mutex);
  }
  #endif

  s->ep_buf = ep_buf;

  return true;
}

static bool stream_claim(tu_edpt_stream_t *s) {
  TU_VERIFY(s->ep_addr != 0); // must be opened
  if (s->is_host) {
    #if CFG_TUH_ENABLED
    return usbh_edpt_claim(s->hwid, s->ep_addr);
  #endif
  } else {
    #if CFG_TUD_ENABLED
    return usbd_edpt_claim(s->hwid, s->ep_addr);
  #endif
  }
  return false;
}

static bool stream_xfer(tu_edpt_stream_t *s, uint16_t count) {
  if (s->is_host) {
    #if CFG_TUH_ENABLED
    return usbh_edpt_xfer(s->hwid, s->ep_addr, count ? s->ep_buf : NULL, count);
  #endif
  } else {
    #if CFG_TUD_ENABLED
    if (s->ep_buf == NULL) {
      return usbd_edpt_xfer_fifo(s->hwid, s->ep_addr, &s->ff, count, false);
    } else {
      return usbd_edpt_xfer(s->hwid, s->ep_addr, count ? s->ep_buf : NULL, count, false);
    }
  #endif
  }
  return false;
}

static bool stream_release(tu_edpt_stream_t *s) {
  if (s->is_host) {
    #if CFG_TUH_ENABLED
    return usbh_edpt_release(s->hwid, s->ep_addr);
  #endif
  } else {
    #if CFG_TUD_ENABLED
    return usbd_edpt_release(s->hwid, s->ep_addr);
  #endif
  }
  return false;
}

//--------------------------------------------------------------------+
// Stream Write
//--------------------------------------------------------------------+
bool tu_edpt_stream_write_zlp_if_needed(tu_edpt_stream_t *s, uint32_t last_xferred_bytes) {
  // ZLP condition: no pending data, last transferred bytes is multiple of packet size
  TU_VERIFY(tu_fifo_empty(&s->ff) && last_xferred_bytes > 0 && (0 == (last_xferred_bytes & (s->mps - 1))));
  TU_VERIFY(stream_claim(s));
  TU_ASSERT(stream_xfer(s, 0));
  return true;
}

uint32_t tu_edpt_stream_write_xfer(tu_edpt_stream_t *s) {
  const uint16_t ff_count = tu_fifo_count(&s->ff);
  TU_VERIFY(ff_count > 0, 0); // skip if no data
  TU_VERIFY(stream_claim(s), 0);

  // Pull data from FIFO -> EP buf
  uint16_t count;
  if (s->ep_buf == NULL) {
    count = tu_fifo_count(&s->ff); // re-get count since fifo can be changed
  } else {
    count = tu_fifo_read_n(&s->ff, s->ep_buf, s->xfer_len);
  }

  if (count > 0) {
    TU_ASSERT(stream_xfer(s, count), 0);
    return count;
  } else {
    // Release endpoint since we don't make any transfer
    // Note: data is dropped if terminal is not connected
    stream_release(s);
    return 0;
  }
}

uint32_t tu_edpt_stream_write(tu_edpt_stream_t *s, const void *buffer, uint32_t bufsize) {
  TU_VERIFY(bufsize > 0);
  const uint16_t ret = tu_fifo_write_n(&s->ff, buffer, (uint16_t) tu_min32(bufsize, UINT16_MAX));

  // flush if fifo has more than packet size or
  // in rare case: fifo depth is configured too small (which never reach packet size)
  if ((tu_fifo_count(&s->ff) >= s->mps) || (tu_fifo_depth(&s->ff) < s->mps)) {
    tu_edpt_stream_write_xfer(s);
  }
  return ret;
}

uint32_t tu_edpt_stream_write_available(tu_edpt_stream_t *s) {
  return (uint32_t)tu_fifo_remaining(&s->ff);
}

//--------------------------------------------------------------------+
// Stream Read
//--------------------------------------------------------------------+
uint32_t tu_edpt_stream_read_xfer(tu_edpt_stream_t *s) {
  uint16_t available = tu_fifo_remaining(&s->ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  TU_VERIFY(available >= s->mps);
  TU_VERIFY(stream_claim(s), 0);
  available = tu_fifo_remaining(&s->ff); // re-get available since fifo can be changed

  if (available >= s->mps) {
    // multiple of packet size limit by ep bufsize
    uint16_t count = (uint16_t) (available & ~(s->mps - 1));
    count = tu_min16(count, s->xfer_len);
    TU_ASSERT(stream_xfer(s, count), 0);
    return count;
  } else {
    // Release endpoint since we don't make any transfer
    stream_release(s);
    return 0;
  }
}

uint32_t tu_edpt_stream_read(tu_edpt_stream_t *s, void *buffer, uint32_t bufsize) {
  const uint32_t num_read = tu_fifo_read_n(&s->ff, buffer, (uint16_t) tu_min32(bufsize, UINT16_MAX));
  tu_edpt_stream_read_xfer(s);
  return num_read;
}

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+

#if CFG_TUSB_DEBUG
#include <ctype.h>

#if CFG_TUSB_DEBUG >= CFG_TUH_LOG_LEVEL || CFG_TUSB_DEBUG >= CFG_TUD_LOG_LEVEL
char const* const tu_str_speed[] = {"Full", "Low", "High"};
char const* const tu_str_std_request[] = {
    "Get Status",
    "Clear Feature",
    "Reserved",
    "Set Feature",
    "Reserved",
    "Set Address",
    "Get Descriptor",
    "Set Descriptor",
    "Get Configuration",
    "Set Configuration",
    "Get Interface",
    "Set Interface",
    "Synch Frame"
};

char const* const tu_str_xfer_result[] = {
    "OK", "FAILED", "STALLED", "TIMEOUT", "ABORTED", "INVALID"
};
#endif

static void dump_str_line(uint8_t const* buf, uint16_t count) {
  tu_printf("  |");
  // each line is 16 bytes
  for (uint16_t i = 0; i < count; i++) {
    int ch = buf[i];
    tu_printf("%c", isprint(ch) ? ch : '.');
  }
  tu_printf("|\r\n");
}

/* Print out memory contents
 *  - buf   : buffer
 *  - count : number of item
 *  - indent: prefix spaces on every line
 */
void tu_print_mem(void const* buf, uint32_t count, uint8_t indent) {
  uint8_t const size = 1; // fixed 1 byte for now
  if (!buf || !count) {
    tu_printf("NULL\r\n");
    return;
  }

  uint8_t const* buf8 = (uint8_t const*) buf;
  char format[] = "%00X";
  format[2] += (uint8_t) (2 * size); // 1 byte = 2 hex digits
  const uint8_t item_per_line = 16 / size;

  for (unsigned int i = 0; i < count; i++) {
    unsigned int value = 0;

    if (i % item_per_line == 0) {
      // Print Ascii
      if (i != 0) {
        dump_str_line(buf8 - 16, 16);
      }
      for (uint8_t s = 0; s < indent; s++) {
        tu_printf(" ");
      }
      // print offset or absolute address
      tu_printf("%04X: ", 16 * i / item_per_line);
    }

    tu_memcpy_s(&value, sizeof(value), buf8, size);
    buf8 += size;

    tu_printf(" ");
    tu_printf(format, value);
  }

  // fill up last row to 16 for printing ascii
  const uint32_t remain = count % 16;
  uint8_t nback = (uint8_t) (remain ? remain : 16);
  if (remain > 0) {
    for (uint32_t i = 0; i < 16 - remain; i++) {
      tu_printf(" ");
      for (int j = 0; j < 2 * size; j++) {
        tu_printf(" ");
      }
    }
  }

  dump_str_line(buf8 - nback, nback);
}

#endif

#if CFG_TUSB_SYSVIEW_ENABLED

#if CFG_TUSB_OS == OPT_OS_FREERTOS
extern const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI;
#define SYSVIEW_OS_API  (&SYSVIEW_X_OS_TraceAPI)
#define SYSVIEW_OS_DESC ",O=FreeRTOS"
#else
#define SYSVIEW_OS_API  0
#define SYSVIEW_OS_DESC ""
#endif

#ifndef SYSVIEW_APP_NAME
#define SYSVIEW_APP_NAME    "TinyUSB"
#endif
#ifndef SYSVIEW_DEVICE_NAME
#define SYSVIEW_DEVICE_NAME "Cortex-M"
#endif
#ifndef SYSVIEW_RAM_BASE
#define SYSVIEW_RAM_BASE    (0x20000000)
#endif

// CPU clock (and DWT timestamp rate). Weak because non-CMSIS SDKs (Pico SDK) have no SystemCoreClock;
// declared here since no single vendor header provides it.
#if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
extern uint32_t SystemCoreClock;
#if defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

TU_ATTR_WEAK uint32_t tusb_sysview_cpu_freq(void) {
  return SystemCoreClock;
}

// Kernel objects named for SystemView, kept for the system description a later recorder connect
// replays. Names must be static: only the pointer is kept.
#define TU_SV_NAMED_MAX 4
static struct {
  const void* handle;
  const char* name;
} _sv_named[TU_SV_NAMED_MAX];
static uint8_t _sv_named_count;

void tu_sysview_name_resource(const void* handle, const char* name) {
  if (handle == NULL || name == NULL) {
    return; // silently ignored: naming is cosmetic, never worth failing a boot over
  }
  // a re-init (dynamic_switch's role switch) names the same static mutex again: update in place
  for (uint8_t i = 0; i < _sv_named_count; i++) {
    if (_sv_named[i].handle == handle) {
      _sv_named[i].name = name;
      SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) handle, name);
      return;
    }
  }
  if (_sv_named_count >= TU_SV_NAMED_MAX) {
    return; // table full and this is a genuinely new handle: drop it, cosmetic only
  }
  _sv_named[_sv_named_count].handle = handle;
  _sv_named[_sv_named_count].name = name;
  _sv_named_count++;
  // sent now too: Start()'s system description already ran, before the stacks created their mutexes
  SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) handle, name);
}

static void send_sys_desc(void) {
  SEGGER_SYSVIEW_SendSysDesc("N=" SYSVIEW_APP_NAME ",D=" SYSVIEW_DEVICE_NAME SYSVIEW_OS_DESC);
  SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");
  for (uint8_t i = 0; i < _sv_named_count; i++) {
    SEGGER_SYSVIEW_NameResource((U32) (uintptr_t) _sv_named[i].handle, _sv_named[i].name);
  }
}

#if CFG_TUSB_SYSVIEW_TIMESTAMP_BSP
  // ARMv7-M without DWT CYCCNT (MAX32665/6, NOCYCCNT=1), built as SEGGER_SYSVIEW_CORE_OTHER: the BSP
  // supplies SEGGER_SYSVIEW_X_GetTimestamp() and its rate, as 1 MHz needs a prescaler the part lacks.
  extern uint32_t SEGGER_SYSVIEW_X_GetTimestampFreq(void);
  #define SYSVIEW_TIMESTAMP_FREQ SEGGER_SYSVIEW_X_GetTimestampFreq()
#elif !defined(SEGGER_SYSVIEW_CORE) || (SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM3)
  // No DWT CYCCNT (ARMv6-M, RISC-V): hw/bsp/<family>/family.c supplies SEGGER_SYSVIEW_X_GetTimestamp(),
  // a free-running microsecond count; a family without one fails to link on that symbol.
  #define SYSVIEW_TIMESTAMP_FREQ 1000000u
#else
  // DWT cycle counter: SEGGER_SYSVIEW_ConfDefaults.h reads it inline, no BSP code needed.
  #define SYSVIEW_TIMESTAMP_FREQ tusb_sysview_cpu_freq()
#endif

// Active interrupt id for cores ConfDefaults does not read ICSR on: RISC-V (mcause), or a Cortex-M
// built as SEGGER_SYSVIEW_CORE_OTHER.
#if !defined(SEGGER_SYSVIEW_CORE) || \
    ((SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM3) && (SEGGER_SYSVIEW_CORE != SEGGER_SYSVIEW_CORE_CM0))
U32 SEGGER_SYSVIEW_X_GetInterruptId(void) {
  #if defined(__riscv) || defined(__riscv__)
  uint32_t mcause;
  __asm volatile ("csrr %0, mcause" : "=r" (mcause));
  return (U32) (mcause & 0xFFFu);
  #elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__) || \
        defined(__ARM_ARCH_8_1M_MAIN__) || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
  // ICSR.VECTACTIVE: 9 bits on v7-M/v8-M mainline, 6 on v6-M (upper bits read 0)
  return (*(volatile uint32_t*) 0xE000ED04u) & 0x1FFu;
  #else
  return 0; // unknown core: report "no interrupt" rather than a bogus id
  #endif
}
#endif

// Records only the outermost enter/exit: dual-role BSPs bracket their back-to-back self-wrapped
// tud_/tuh_int_handler() calls, so one interrupt is one span. Volatile since nested USB IRQs
// (ch32v20x HP over LP) can both reach it; non-atomic on purpose, as a lost update miscounts one
// span and recovers on the next balanced pair.
static volatile uint8_t _sv_isr_depth;

void tu_sysview_isr_enter(void) {
  if (_sv_isr_depth++ == 0) {
    SEGGER_SYSVIEW_RecordEnterISR();
  }
}

// PendSV pending at the outermost exit means the ISR returns through the scheduler, not to the
// task it preempted: record ExitISRToScheduler then, or SystemView shows a causeless switch.
// Cortex-M only; RISC-V has no PendSV and takes the plain exit record.
#if defined(SEGGER_SYSVIEW_CORE) && \
    ((SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3) || (SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM0))
  #define SV_ICSR_PENDSVSET_PENDING() \
    ((*(volatile uint32_t*) 0xE000ED04 /* SCB->ICSR */) & (1u << 28) /* PENDSVSET */)
#endif

void tu_sysview_isr_exit(void) {
  if (_sv_isr_depth != 0 && --_sv_isr_depth == 0) {
#if defined(SV_ICSR_PENDSVSET_PENDING)
    if (SV_ICSR_PENDSVSET_PENDING()) {
      SEGGER_SYSVIEW_RecordExitISRToScheduler();
    } else {
      SEGGER_SYSVIEW_RecordExitISR();
    }
#else
    SEGGER_SYSVIEW_RecordExitISR();
#endif
  }
}

void tu_sysview_init(void) {
  static bool inited = false;
  if (inited) { return; }
  inited = true;
#if defined(SEGGER_SYSVIEW_CORE) && (SEGGER_SYSVIEW_CORE == SEGGER_SYSVIEW_CORE_CM3)
  // DWT CYCCNT drives timestamps: enable it on target, not via a debugger (post-mortem too), unless
  // DWT_CTRL.NOCYCCNT (bit 25) says the counter is absent.
  (*(volatile unsigned int*) 0xE000EDFC) |= (1u << 24);   /* DEMCR.TRCENA */
  if (((*(volatile unsigned int*) 0xE0001000) & (1u << 25)) == 0) {
    (*(volatile unsigned int*) 0xE0001000) |= 1u;         /* DWT_CTRL.CYCCNTENA */
  }
#endif
  SEGGER_SYSVIEW_Init(SYSVIEW_TIMESTAMP_FREQ, tusb_sysview_cpu_freq(), SYSVIEW_OS_API, send_sys_desc);
  SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
  // no SEGGER_SYSVIEW_RegisterModule(): see TU_SV_EVENT_BASE
  SEGGER_SYSVIEW_Start(); /* self-start: host recorders only drain */
#ifdef SYSVIEW_DISABLE_EVENTS
  SEGGER_SYSVIEW_DisableEvents(SYSVIEW_DISABLE_EVENTS);
#endif
}

void tu_sysview_stack_report(void) {
// Bytes used at peak needs pxEndOfStack, which TaskStatus_t has only under this condition (task.h);
// the SYSVIEW families' FreeRTOSConfig.h set configRECORD_STACK_HIGH_ADDRESS=1. Headroom alone would read as usage.
#if CFG_TUSB_OS == OPT_OS_FREERTOS && (configUSE_TRACE_FACILITY == 1) && \
    ((portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1))
  // SEGGER's FreeRTOS task-table cap (family_support.cmake). Above it uxTaskGetSystemState() returns
  // 0, not a truncated list, so nothing is reported.
  #ifndef SYSVIEW_FREERTOS_MAX_NOF_TASKS
  #define SYSVIEW_FREERTOS_MAX_NOF_TASKS 16
  #endif
  // static: 16 TaskStatus_t is ~700 bytes, too much for usbd's stack
  static TaskStatus_t status[SYSVIEW_FREERTOS_MAX_NOF_TASKS];
  static char names[SYSVIEW_FREERTOS_MAX_NOF_TASKS][configMAX_TASK_NAME_LEN];
  // One task per call: a burst of SendTaskInfo() calls (each locking and writing the RTT ring)
  // dropped same54_xplained off the bus.
  static UBaseType_t next_idx = 0;
  static UBaseType_t n = 0;
  // usbd's and usbh's tasks both report, so the scheduler stays suspended across the whole report
  // (single-core ports only). It also keeps vTaskDelete() of another task from freeing its TCB
  // while its name is copied.
  vTaskSuspendAll();
  if (next_idx == 0) {
    // the snapshot scans every stack, so take it once per lap
    n = uxTaskGetSystemState(status, TU_ARRAY_SIZE(status), NULL);
    for (UBaseType_t t = 0; t < n; t++) {
      strncpy(names[t], status[t].pcTaskName, configMAX_TASK_NAME_LEN - 1);
      names[t][configMAX_TASK_NAME_LEN - 1] = '\0';
    }
  }
  if (n != 0) {
    UBaseType_t const i = next_idx;
    next_idx = (next_idx + 1 < n) ? next_idx + 1 : 0;

    SEGGER_SYSVIEW_TASKINFO info = {0};
    info.TaskID    = (U32)(uintptr_t) status[i].xHandle;
    info.sName     = names[i];
    info.Prio      = status[i].uxCurrentPriority;
    info.StackBase = (U32)(uintptr_t) status[i].pxStackBase;
    uint32_t const free_bytes = status[i].usStackHighWaterMark * sizeof(StackType_t);
    uint32_t const stack_size = (uint32_t)(uintptr_t) status[i].pxEndOfStack -
                                 (uint32_t)(uintptr_t) status[i].pxStackBase + sizeof(StackType_t);
    info.StackSize  = stack_size;
    info.StackUsage = (free_bytes < stack_size) ? (stack_size - free_bytes) : 0;
    SEGGER_SYSVIEW_SendTaskInfo(&info);
  }
  (void) xTaskResumeAll();
#endif
}

#endif // CFG_TUSB_SYSVIEW_ENABLED

#endif // host or device enabled
