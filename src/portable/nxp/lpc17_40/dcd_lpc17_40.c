/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && \
    (CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX)

#include "device/dcd.h"
#include "dcd_lpc17_40.h"
#include "chip.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define DCD_ENDPOINT_MAX 32

// The iso machinery (5th DD word + packet-size memory) costs USB RAM on every build;
// compile it only when a class that can open an iso endpoint is enabled. Keep this in
// sync with the classes that actually arm an iso endpoint: audio, video, BTH (voice),
// and vendor (its optional CFG_TUD_VENDOR_EP_ISO_* endpoints, exercised by usbtest).
#define DCD_ISO_ENABLED (CFG_TUD_AUDIO || CFG_TUD_VIDEO || CFG_TUD_VENDOR || CFG_TUD_BTH)

typedef struct TU_ATTR_ALIGNED(4)
{
  //------------- Word 0 -------------//
  uint32_t next;

  //------------- Word 1 -------------//
  uint16_t atle_mode       : 2; // 00: normal, 01: ATLE (auto length extraction)
  uint16_t next_valid      : 1;
  uint16_t                 : 1; ///< reserved
  uint16_t isochronous     : 1; // is an iso endpoint
  uint16_t max_packet_size : 11;

  volatile uint16_t buflen; // bytes for non-iso, number of packets for iso endpoint

  //------------- Word 2 -------------//
  volatile uint32_t buffer;

  //------------- Word 3 -------------//
  volatile uint16_t retired                : 1; // initialized to zero
  volatile uint16_t status                 : 4;
  volatile uint16_t iso_last_packet_valid  : 1;
  volatile uint16_t atle_lsb_extracted     : 1;	// used in ATLE mode
  volatile uint16_t atle_msb_extracted     : 1;	// used in ATLE mode
  volatile uint16_t atle_mess_len_position : 6; // used in ATLE mode
  uint16_t                                 : 2;

  volatile uint16_t present_count;  // For non-iso : The number of bytes transferred by the DMA engine
                                    // For iso : number of packets

#if DCD_ISO_ENABLED
  //------------- Word 4 -------------//
  volatile uint32_t iso_packet_size_addr; // iso only: pointer into iso packet-size memory,
                                          // advanced by hardware after each packet
#endif
}dma_desc_t;

TU_VERIFY_STATIC( sizeof(dma_desc_t) == (DCD_ISO_ENABLED ? 20 : 16), "size is not correct");

// Hardware fixes endpoint type by number: 3, 6, 9, 12 are the iso-capable ones.
// Constant per ep_id (= 2*epnum + dir) — unlike dd->isochronous, which dcd_edpt_xfer
// transiently zeroes while rebuilding the DD, this is safe to dispatch on from the ISR.
// TU_ATTR_UNUSED: every caller is under #if DCD_ISO_ENABLED, so non-iso builds don't
// reference it and clang -Wunused-function (fatal) would otherwise reject the build.
TU_ATTR_UNUSED TU_ATTR_ALWAYS_INLINE static inline bool ep_id_is_iso(uint8_t ep_id) {
  uint8_t const epnum = (uint8_t)(ep_id >> 1);
  return (epnum % 3) == 0 && (epnum != 0) && (epnum != 15);
}

#if DCD_ISO_ENABLED
// Isochronous packet-size memory (UM10562 12.15.6.3): one word per packet.
// IN : software fills Packet_length (bits 15:0), 0 = ZLP
// OUT: hardware writes Frame_number (31:17) | Packet_valid (16) | Packet_length (15:0)
// Iso-capable endpoint numbers are 3, 6, 9, 12 -> 8 slots (x2 directions).
// One packet moves per FRAME, so a deep queue only adds latency: 8 frames is plenty.
#define ISO_MAX_PACKETS 8
#define ISO_SLOT_COUNT  8
TU_ATTR_ALWAYS_INLINE static inline uint8_t iso_slot(uint8_t ep_id) {
  return (uint8_t)(((ep_id / 6) - 1) * 2 + (ep_id & 1)); // ep_id = 2*epnum + dir, epnum in {3,6,9,12}
}
#endif

typedef struct
{
  // must be 128 byte aligned
  volatile dma_desc_t* udca[DCD_ENDPOINT_MAX];

  // TODO DMA does not support control transfer (0-1 are not used, offset to reduce memory)
  dma_desc_t dd[DCD_ENDPOINT_MAX];

  struct
  {
    uint8_t* out_buffer;
    uint8_t  out_bytes;
    volatile bool out_queued;   // an OUT xfer is queued; out_buffer may legitimately be NULL (status ZLP)
    volatile bool out_received; // indicate if data is already received in endpoint

    uint8_t  in_bytes;
  } control;

#if DCD_ISO_ENABLED
  // iso packet-size memory, must be DMA-reachable like the DDs
  volatile uint32_t iso_psize[ISO_SLOT_COUNT][ISO_MAX_PACKETS];
#endif

} dcd_data_t;

CFG_TUD_MEM_SECTION TU_ATTR_ALIGNED(128) static dcd_data_t _dcd;


//--------------------------------------------------------------------+
// SIE Command
//--------------------------------------------------------------------+

// The SIE command protocol (CmdCode + CCEMPTY/CDFULL handshake) and the
// slave-mode Ctrl/RxData/TxData registers are shared between thread-mode API
// calls and dcd_int_handler, and are not reentrant: an ISR preempting a
// thread-mode SIE sequence consumes its handshake flags and overwrites
// CmdCode (symptom: EP0 wedges/answers stale data right after SET_INTERFACE
// stall/clear-stall bursts overlapping bulk EOT interrupts). Mask only the
// USB interrupt around those sequences; safe to nest, including from the ISR.
static inline bool usb_irq_lock(void)
{
  bool const enabled = NVIC_GetEnableIRQ(USB_IRQn) != 0;
  if (enabled)
  {
    NVIC_DisableIRQ(USB_IRQn); // CMSIS already ends this with DSB+ISB
  }
  return enabled;
}

static inline void usb_irq_unlock(bool enabled)
{
  if (enabled) NVIC_EnableIRQ(USB_IRQn);
}

static void sie_cmd_code (sie_cmdphase_t phase, uint8_t code_data)
{
  LPC_USB->DevIntClr = (DEV_INT_COMMAND_CODE_EMPTY_MASK | DEV_INT_COMMAND_DATA_FULL_MASK);
  LPC_USB->CmdCode   = (phase << 8) | (code_data << 16);

  uint32_t const wait_flag = (phase == SIE_CMDPHASE_READ) ? DEV_INT_COMMAND_DATA_FULL_MASK : DEV_INT_COMMAND_CODE_EMPTY_MASK;
  while ((LPC_USB->DevIntSt & wait_flag) == 0) {}

  LPC_USB->DevIntClr = wait_flag;
}

static void sie_write (uint8_t cmd_code, uint8_t data_len, uint8_t data)
{
  bool const lock = usb_irq_lock();

  sie_cmd_code(SIE_CMDPHASE_COMMAND, cmd_code);

  if (data_len)
  {
    sie_cmd_code(SIE_CMDPHASE_WRITE, data);
  }

  usb_irq_unlock(lock);
}

static uint8_t sie_read (uint8_t cmd_code)
{
  bool const lock = usb_irq_lock();

  sie_cmd_code(SIE_CMDPHASE_COMMAND , cmd_code);
  sie_cmd_code(SIE_CMDPHASE_READ    , cmd_code);
  uint8_t const data = (uint8_t) LPC_USB->CmdData;

  usb_irq_unlock(lock);
  return data;
}

//--------------------------------------------------------------------+
// PIPE HELPER
//--------------------------------------------------------------------+
static inline uint8_t ep_addr2idx(uint8_t ep_addr)
{
  return (uint8_t)(2*(ep_addr & 0x0F) + ((ep_addr & TUSB_DIR_IN_MASK) ? 1 : 0));
}

static void set_ep_size(uint8_t ep_id, uint16_t max_packet_size)
{
  // ReEp RMW + the EP_RLZED handshake share DevIntSt with the ISR: a bus reset
  // from dcd_int_handler writes DevIntClr = 0xFFFFFFFF and would consume the
  // flag this spin waits on, hanging it forever -> same lock as the SIE paths.
  bool const lock = usb_irq_lock();

  // follows example in 11.10.4.2
  LPC_USB->ReEp    |= TU_BIT(ep_id);
  LPC_USB->EpInd    = ep_id; // select index before setting packet size
  LPC_USB->MaxPSize = max_packet_size;

  while ((LPC_USB->DevIntSt & DEV_INT_ENDPOINT_REALIZED_MASK) == 0) {}
  LPC_USB->DevIntClr = DEV_INT_ENDPOINT_REALIZED_MASK;

  usb_irq_unlock(lock);
}


//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+
static void bus_reset(void)
{
  // step 7 : slave mode set up
  LPC_USB->EpIntClr     = 0xFFFFFFFF; // clear all pending interrupt
  LPC_USB->DevIntClr    = 0xFFFFFFFF; // clear all pending interrupt
  LPC_USB->EpIntEn      = 0x03UL;     // control endpoint cannot use DMA, non-control all use DMA
  LPC_USB->EpIntPri     = 0x03UL;     // fast for control endpoint

  // step 8 : DMA set up
  LPC_USB->EpDMADis     = 0xFFFFFFFF; // firstly disable all dma
  LPC_USB->DMARClr      = 0xFFFFFFFF; // clear all pending interrupt
  LPC_USB->EoTIntClr    = 0xFFFFFFFF;
  LPC_USB->NDDRIntClr   = 0xFFFFFFFF;
  LPC_USB->SysErrIntClr = 0xFFFFFFFF;

  tu_memclr(&_dcd, sizeof(dcd_data_t));
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;

  //------------- user manual 11.13 usb device controller initialization -------------//
  // step 6 : set up control endpoint
  set_ep_size(0, CFG_TUD_ENDPOINT0_SIZE);
  set_ep_size(1, CFG_TUD_ENDPOINT0_SIZE);

  bus_reset();

  LPC_USB->DevIntEn = (DEV_INT_DEVICE_STATUS_MASK | DEV_INT_ENDPOINT_FAST_MASK | DEV_INT_ENDPOINT_SLOW_MASK | DEV_INT_ERROR_MASK);
  LPC_USB->UDCAH = (uint32_t) _dcd.udca;
  LPC_USB->DMAIntEn = (DMA_INT_END_OF_XFER_MASK /*| DMA_INT_NEW_DD_REQUEST_MASK*/ | DMA_INT_ERROR_MASK);

  dcd_connect(rhport);

  // Clear pending IRQ
  NVIC_ClearPendingIRQ(USB_IRQn);

  return true;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  // Response with status first before changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0, false);

  sie_write(SIE_CMDCODE_SET_ADDRESS, 1, 0x80 | dev_addr); // 7th bit is : device_enable

  // Also Set Configure Device to enable non-control endpoint response
  sie_write(SIE_CMDCODE_CONFIGURE_DEVICE, 1, 1);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, SIE_DEV_STATUS_CONNECT_STATUS_MASK);
}

void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  sie_write(SIE_CMDCODE_DEVICE_STATUS, 1, 0);
}

void dcd_sof_enable(uint8_t rhport, bool en)
{
  (void) rhport;
  (void) en;

  // TODO implement later
}

//--------------------------------------------------------------------+
// CONTROL HELPER
//--------------------------------------------------------------------+
static inline uint8_t byte2dword(uint8_t bytes)
{
  return (uint8_t)((bytes + 3) / 4); // length in dwords
}

static void control_ep_write(void const * buffer, uint8_t len)
{
  uint32_t const * buf32 = (uint32_t const *) buffer;
  bool const lock = usb_irq_lock(); // Ctrl/TxData + SIE sequence must not interleave with the ISR

  LPC_USB->Ctrl   = USBCTRL_WRITE_ENABLE_MASK; // logical endpoint = 0
  LPC_USB->TxPLen = (uint32_t) len;

  for (uint8_t count = 0; count < byte2dword(len); count++)
  {
    LPC_USB->TxData = *buf32; // NOTE: cortex M3 have no problem with alignment
    buf32++;
  }

  LPC_USB->Ctrl = 0;

  // select control IN & validate the endpoint
  sie_write(SIE_CMDCODE_ENDPOINT_SELECT+1, 0, 0);
  sie_write(SIE_CMDCODE_BUFFER_VALIDATE  , 0, 0);

  usb_irq_unlock(lock);
}

static uint8_t control_ep_read(void * buffer, uint8_t len)
{
  bool const lock = usb_irq_lock(); // Ctrl/RxData + SIE sequence must not interleave with the ISR

  LPC_USB->Ctrl = USBCTRL_READ_ENABLE_MASK; // logical endpoint = 0
  while ((LPC_USB->RxPLen & USBRXPLEN_PACKET_READY_MASK) == 0) {} // TODO blocking, should have timeout

  len = tu_min8(len, (uint8_t) (LPC_USB->RxPLen & USBRXPLEN_PACKET_LENGTH_MASK) );
  uint32_t *buf32 = (uint32_t*) buffer;

  for (uint8_t count=0; count < byte2dword(len); count++)
  {
    *buf32 = LPC_USB->RxData;
    buf32++;
  }

  LPC_USB->Ctrl = 0;

  // select control OUT & clear the endpoint
  sie_write(SIE_CMDCODE_ENDPOINT_SELECT+0, 0, 0);
  sie_write(SIE_CMDCODE_BUFFER_CLEAR     , 0, 0);

  usb_irq_unlock(lock);
  return len;
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const ep_id = ep_addr2idx(p_endpoint_desc->bEndpointAddress);

  // Endpoint type is fixed to endpoint number (1 interrupt, 2 bulk, 3 iso, ...).
  // Iso endpoints are armed via dcd_edpt_iso_alloc/activate, never through here
  // (TUP_DCD_EDPT_ISO_ALLOC is defined for this IP), so only bulk/interrupt land here.
  switch ( p_endpoint_desc->bmAttributes.xfer )
  {
    case TUSB_XFER_INTERRUPT:
      TU_ASSERT((epnum % 3) == 1);
      break;

    case TUSB_XFER_BULK:
      TU_ASSERT((epnum % 3) == 2 || (epnum == 15));
      break;

    default:
      break;
  }

  //------------- Realize Endpoint with Max Packet Size -------------//
  const uint16_t ep_size = tu_edpt_packet_size(p_endpoint_desc);
  set_ep_size(ep_id, ep_size);

  //------------- first DD prepare -------------//
  dma_desc_t* const dd = &_dcd.dd[ep_id];
  tu_memclr(dd, sizeof(dma_desc_t)); // non-iso: isochronous stays 0
  dd->max_packet_size = ep_size;
  dd->retired = 1; // invalid at first

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS + ep_id, 1, 0);    // clear all endpoint status

  return true;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
#if DCD_ISO_ENABLED
  (void)rhport;
  uint8_t const ep_id = ep_addr2idx(ep_addr);

  // hardware fixes iso to endpoint numbers 3, 6, 9, 12
  TU_ASSERT(ep_id_is_iso(ep_id));
  TU_ASSERT(largest_packet_size > 0);

  set_ep_size(ep_id, largest_packet_size);

  dma_desc_t* const dd = &_dcd.dd[ep_id];
  tu_memclr(dd, sizeof(dma_desc_t));
  dd->isochronous     = 1;
  dd->max_packet_size = largest_packet_size;
  dd->retired         = 1; // invalid at first

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS + ep_id, 1, 0);
  return true;
#else
  (void)rhport; (void)ep_addr; (void)largest_packet_size;
  return false;
#endif
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
#if DCD_ISO_ENABLED
  (void)rhport;
  uint8_t const ep_id = ep_addr2idx(desc_ep->bEndpointAddress);
  dma_desc_t* const dd = &_dcd.dd[ep_id];

  // same fixed-number rule as alloc: without it a rejected-but-ignored alloc (classes
  // discard that return) would set isochronous on a non-iso ep_id and underflow iso_slot()
  TU_ASSERT(ep_id_is_iso(ep_id));

  // kill any armed transfer from a previous alternate setting
  LPC_USB->EpDMADis = TU_BIT(ep_id);
  _dcd.udca[ep_id] = NULL;

  dd->isochronous     = 1;
  dd->max_packet_size = tu_edpt_packet_size(desc_ep);
  dd->retired         = 1;

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS + ep_id, 1, 0);
  return true;
#else
  (void)rhport; (void)desc_ep;
  return false;
#endif
}

void dcd_edpt_close_all (uint8_t rhport)
{
  (void) rhport;
  // TODO implement dcd_edpt_close_all()
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  if ( tu_edpt_number(ep_addr) == 0 )
  {
    sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+0, 1, SIE_SET_ENDPOINT_STALLED_MASK | SIE_SET_ENDPOINT_CONDITION_STALLED_MASK);
  }else
  {
    uint8_t ep_id = ep_addr2idx( ep_addr );
    sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, SIE_SET_ENDPOINT_STALLED_MASK);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t ep_id = ep_addr2idx(ep_addr);

  sie_write(SIE_CMDCODE_ENDPOINT_SET_STATUS+ep_id, 1, 0);
}

static bool control_xact(uint8_t rhport, uint8_t dir, uint8_t * buffer, uint8_t len)
{
  (void) rhport;

  if ( dir )
  {
    _dcd.control.in_bytes = len;
    control_ep_write(buffer, len);
  }else
  {
    // guard the out_received/out_buffer handshake against the EP0 OUT ISR
    bool const lock = usb_irq_lock();

    if ( _dcd.control.out_received )
    {
      // Already received the DATA OUT packet
      _dcd.control.out_received = false;

      uint8_t received = control_ep_read(buffer, len);
      // event queued with in_isr=true, which skips the queue's own locking: keep the
      // USB IRQ masked across it, or a real ISR completion could interleave the write
      dcd_event_xfer_complete(0, 0, received, XFER_RESULT_SUCCESS, true);
      usb_irq_unlock(lock);
    }else
    {
      // buffer is NULL for a status-stage ZLP: signal the pending xfer explicitly,
      // NOT via out_buffer != NULL — a NULL-buffer queue mistaken for "nothing queued"
      // leaves out_received stale and poisons the next control OUT data stage.
      _dcd.control.out_buffer = buffer;
      _dcd.control.out_bytes  = len;
      _dcd.control.out_queued = true;
      usb_irq_unlock(lock);
    }
  }

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes, bool is_isr)
{
  (void) is_isr;
  // Control transfer is not DMA support, and must be done in slave mode
  if ( tu_edpt_number(ep_addr) == 0 )
  {
    return control_xact(rhport, tu_edpt_dir(ep_addr), buffer, (uint8_t) total_bytes);
  }
  else
  {
    uint8_t ep_id = ep_addr2idx(ep_addr);
    dma_desc_t* dd = &_dcd.dd[ep_id];

    // Prepare DMA descriptor
    // Isochronous & max packet size must be preserved, Other fields of dd should be clear
    uint16_t const ep_size = dd->max_packet_size;
    uint8_t  is_iso = dd->isochronous;

#if DCD_ISO_ENABLED
    if ( is_iso )
    {
      // iso: buflen counts packets; per-packet sizes live in the packet-size memory.
      // One packet moves per frame (UM10562 12.15.6: DMA request is raised for
      // DMA-enabled iso endpoints on every FRAME interrupt, both directions).
      // Validate BEFORE touching the DD: bailing out mid-rebuild would leave a
      // zeroed (retired=0 -> serviceable) descriptor armed for the frame engine.
      TU_ASSERT(ep_size > 0);
      uint16_t const packets = (total_bytes > 0) ? (uint16_t) tu_div_ceil(total_bytes, ep_size) : 1;
      TU_ASSERT(packets <= ISO_MAX_PACKETS);

      uint8_t const slot = iso_slot(ep_id);
      uint16_t remain = total_bytes;
      for ( uint16_t i = 0; i < packets; i++ )
      {
        uint16_t const pkt_len = tu_min16(remain, ep_size);
        // IN: length to send (0 = ZLP). OUT: hardware writes back
        // Frame_number|Packet_valid|Packet_length -- prefill 0 so a frame the
        // hardware never wrote (missed/invalid) cannot read back as data.
        _dcd.iso_psize[slot][i] = (ep_id & 1) ? pkt_len : 0;
        remain = (uint16_t)(remain - pkt_len);
      }

      tu_memclr(dd, sizeof(dma_desc_t));
      dd->isochronous = 1;
      dd->max_packet_size = ep_size;
      dd->buffer = (uint32_t) buffer;
      dd->buflen = packets;
      dd->iso_packet_size_addr = (uint32_t) &_dcd.iso_psize[slot][0];

      _dcd.udca[ep_id] = dd;
      LPC_USB->EpDMAEn = TU_BIT(ep_id); // frame-triggered: no DMARSet, no EpIntEn
    }
    else
#else
    (void) is_iso;
#endif
    {
      tu_memclr(dd, sizeof(dma_desc_t));
      dd->max_packet_size = ep_size;
      dd->buffer = (uint32_t) buffer;
      dd->buflen = total_bytes;

      _dcd.udca[ep_id] = dd;

      if ( ep_id % 2 )
      {
        // Clear EP interrupt before Enable DMA
        // EpIntEn read-modify-write races the ISR's own RMWs -> lock
        bool const lock = usb_irq_lock();
        LPC_USB->EpIntEn &= ~TU_BIT(ep_id);
        LPC_USB->EpDMAEn = TU_BIT(ep_id);
        usb_irq_unlock(lock);

        // endpoint IN need to actively raise DMA request
        LPC_USB->DMARSet = TU_BIT(ep_id);
      }else
      {
        // Enable DMA
        LPC_USB->EpDMAEn = TU_BIT(ep_id);
      }
    }

    return true;
  }
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

// handle control xfer (slave mode)
static void control_xfer_isr(uint8_t rhport, uint32_t ep_int_status)
{
  // Control out complete
  if ( ep_int_status & TU_BIT(0) )
  {
    bool is_setup = sie_read(SIE_CMDCODE_ENDPOINT_SELECT+0) & SIE_SELECT_ENDPOINT_SETUP_RECEIVED_MASK;

    LPC_USB->EpIntClr = TU_BIT(0);

    if (is_setup)
    {
      uint8_t setup_packet[8];
      control_ep_read(setup_packet, 8); // TODO read before clear setup above

      // a new SETUP voids any half-finished control state
      _dcd.control.out_queued   = false;
      _dcd.control.out_received = false;
      _dcd.control.out_buffer   = NULL;
      _dcd.control.out_bytes    = 0;

      dcd_event_setup_received(rhport, setup_packet, true);
    }
    else if ( _dcd.control.out_queued )
    {
      // software queued transfer previously (out_buffer NULL = status ZLP)
      uint8_t received = control_ep_read(_dcd.control.out_buffer, _dcd.control.out_bytes);

      _dcd.control.out_queued = false;
      _dcd.control.out_buffer = NULL;
      _dcd.control.out_bytes = 0;

      dcd_event_xfer_complete(rhport, 0, received, XFER_RESULT_SUCCESS, true);
    }else
    {
      // hardware auto ack packet -> mark as received
      _dcd.control.out_received = true;
    }
  }

  // Control In complete
  if ( ep_int_status & TU_BIT(1) )
  {
    LPC_USB->EpIntClr = TU_BIT(1);
    dcd_event_xfer_complete(rhport, TUSB_DIR_IN_MASK, _dcd.control.in_bytes, XFER_RESULT_SUCCESS, true);
  }
}

// handle bus event signal
static void bus_event_isr(uint8_t rhport)
{
  uint8_t const dev_status = sie_read(SIE_CMDCODE_DEVICE_STATUS);
  if (dev_status & SIE_DEV_STATUS_RESET_MASK)
  {
    bus_reset();
    dcd_event_bus_reset(rhport, TUSB_SPEED_FULL, true);
  }

  if (dev_status & SIE_DEV_STATUS_CONNECT_CHANGE_MASK)
  {
    // device is disconnected, require using VBUS (P1_30)
    dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
  }

  if (dev_status & SIE_DEV_STATUS_SUSPEND_CHANGE_MASK)
  {
    if (dev_status & SIE_DEV_STATUS_SUSPEND_MASK)
    {
      dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
    }
    else
    {
      dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
    }
  }
}

// Helper to complete a DMA descriptor for non-control transfer
static void dd_complete_isr(uint8_t rhport, uint8_t ep_id)
{
  dma_desc_t* const dd = &_dcd.dd[ep_id];
  uint8_t result = (dd->status == DD_STATUS_NORMAL || dd->status == DD_STATUS_DATA_UNDERUN) ? XFER_RESULT_SUCCESS : XFER_RESULT_FAILED;
  uint8_t const ep_addr = (ep_id / 2) | ((ep_id & 0x01) ? TUSB_DIR_IN_MASK : 0);

  uint32_t xferred_bytes;
#if DCD_ISO_ENABLED
  if ( ep_id_is_iso(ep_id) )
  {
    // present_count is in packets; actual byte counts are in the packet-size memory
    // (IN: as programmed by us, OUT: Packet_length written back by hardware,
    //  guarded by Packet_valid -- a frame with no packet must count as 0)
    uint8_t const slot = iso_slot(ep_id);
    uint16_t const packets = tu_min16(dd->present_count, ISO_MAX_PACKETS);
    xferred_bytes = 0;
    for (uint16_t i = 0; i < packets; i++)
    {
      uint32_t const psize = _dcd.iso_psize[slot][i];
      if ( (ep_id & 1) || (psize & TU_BIT(16)) )
      {
        xferred_bytes += (psize & 0xFFFFu);
      }
    }
  }
  else
#endif
  {
    xferred_bytes = dd->present_count;
  }

  dcd_event_xfer_complete(rhport, ep_addr, (uint16_t) xferred_bytes, result, true);
}

// main USB IRQ handler
void dcd_int_handler(uint8_t rhport)
{
  uint32_t const dev_int_status = LPC_USB->DevIntSt & LPC_USB->DevIntEn;
  LPC_USB->DevIntClr = dev_int_status;// Acknowledge handled interrupt

  // Bus event
  if (dev_int_status & DEV_INT_DEVICE_STATUS_MASK)
  {
    bus_event_isr(rhport);
  }

  // Endpoint interrupt
  uint32_t const ep_int_status = LPC_USB->EpIntSt & LPC_USB->EpIntEn;

  // Control Endpoint are fast
  if (dev_int_status & DEV_INT_ENDPOINT_FAST_MASK)
  {
    // Note clear USBEpIntClr will also clear the setup received bit --> clear after handle setup packet
    // Only clear USBEpIntClr 1 endpoint each, and should wait for CDFULL bit set
    control_xfer_isr(rhport, ep_int_status);
  }

  // non-control IN are slow
  if (dev_int_status & DEV_INT_ENDPOINT_SLOW_MASK)
  {
    for ( uint8_t ep_id = 3; ep_id < DCD_ENDPOINT_MAX; ep_id += 2 )
    {
      if ( tu_bit_test(ep_int_status, ep_id) )
      {
        LPC_USB->EpIntClr = TU_BIT(ep_id);

        // Clear Ep interrupt for next DMA
        LPC_USB->EpIntEn &= ~TU_BIT(ep_id);

        dd_complete_isr(rhport, ep_id);
      }
    }
  }

  // DMA transfer complete (RAM <-> EP) for Non-Control
  // OUT: USB transfer is fully complete
  // IN : UBS transfer is still on-going -> enable EpIntEn to know when it is complete
  uint32_t const dma_int_status = LPC_USB->DMAIntSt & LPC_USB->DMAIntEn;
  if (dma_int_status & DMA_INT_END_OF_XFER_MASK)
  {
    uint32_t const eot = LPC_USB->EoTIntSt;
    LPC_USB->EoTIntClr = eot; // acknowledge interrupt source

    for ( uint8_t ep_id = 2; ep_id < DCD_ENDPOINT_MAX; ep_id++ )
    {
      if ( tu_bit_test(eot, ep_id) )
      {
        // dispatch on the hardware's fixed ep-number/type map, NOT dd->isochronous:
        // thread-mode dcd_edpt_xfer transiently zeroes the DD while rebuilding it
#if DCD_ISO_ENABLED
        if ( ep_id_is_iso(ep_id) )
        {
          // iso: last packet already left with its frame; complete both directions here
          dd_complete_isr(rhport, ep_id);
        }
        else
#endif
        if ( ep_id & 0x01 )
        {
          // IN enable EpInt for end of usb transfer
          LPC_USB->EpIntEn |= TU_BIT(ep_id);
        }else
        {
          // OUT
          dd_complete_isr(rhport, ep_id);
        }
      }
    }
  }

  // Errors
  if ( (dev_int_status & DEV_INT_ERROR_MASK) || (dma_int_status & DMA_INT_ERROR_MASK) )
  {
    uint32_t error_status = sie_read(SIE_CMDCODE_READ_ERROR_STATUS);
    (void) error_status;
    TU_BREAKPOINT();
  }
}
#endif
