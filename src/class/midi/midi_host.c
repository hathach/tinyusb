/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "tusb_option.h"

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_MIDI)

#include "host/usbh.h"
#include "host/usbh_classdriver.h"

#include "midi_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#ifndef CFG_TUH_MAX_CABLES
  #define CFG_TUH_MAX_CABLES 16
#endif
#define CFG_TUH_MIDI_RX_BUFSIZE 64
#define CFG_TUH_MIDI_TX_BUFSIZE 64
#ifndef CFG_TUH_MIDI_EP_BUFSIZE
  #define CFG_TUH_MIDI_EP_BUFSIZE 64
#endif

// TODO: refactor to share code with the MIDI Device driver
typedef struct
{
  uint8_t buffer[4];
  uint8_t index;
  uint8_t total;
}midi_stream_t;

typedef struct
{
  uint8_t dev_addr;
  uint8_t itf_num;

  uint8_t ep_in;          // IN endpoint address
  uint8_t ep_out;         // OUT endpoint address
  uint16_t ep_in_max;     // min( CFG_TUH_MIDI_RX_BUFSIZE, wMaxPacketSize of the IN endpoint)
  uint16_t ep_out_max;    //  min( CFG_TUH_MIDI_TX_BUFSIZE, wMaxPacketSize of the OUT endpoint)

  uint8_t num_cables_rx;  // IN endpoint CS descriptor bNumEmbMIDIJack value
  uint8_t num_cables_tx;  // OUT endpoint CS descriptor bNumEmbMIDIJack value

  // For Stream read()/write() API
  // Messages are always 4 bytes long, queue them for reading and writing so the
  // callers can use the Stream interface with single-byte read/write calls.
  midi_stream_t stream_write;
  midi_stream_t stream_read;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  // Endpoint FIFOs
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;
 

  uint8_t rx_ff_buf[CFG_TUH_MIDI_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUH_MIDI_TX_BUFSIZE];

  #if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex;
  osal_mutex_def_t tx_ff_mutex;
  #endif

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUH_MIDI_EP_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUH_MIDI_EP_BUFSIZE];

  bool configured;
}midih_interface_t;

static midih_interface_t _midi_host;

//------------- Internal prototypes -------------//
static uint32_t write_flush(uint8_t dev_addr, midih_interface_t* midi);
#if 0
typedef struct
{
  uint8_t inst_count;
  hidh_interface_t instances[CFG_TUH_HID];
} hidh_device_t;

static hidh_device_t _hidh_dev[CFG_TUH_DEVICE_MAX];

//------------- Internal prototypes -------------//

// Get HID device & interface
TU_ATTR_ALWAYS_INLINE static inline hidh_device_t* get_dev(uint8_t dev_addr);
TU_ATTR_ALWAYS_INLINE static inline hidh_interface_t* get_instance(uint8_t dev_addr, uint8_t instance);
static uint8_t get_instance_id_by_itfnum(uint8_t dev_addr, uint8_t itf);
static uint8_t get_instance_id_by_epaddr(uint8_t dev_addr, uint8_t ep_addr);

//--------------------------------------------------------------------+
// Interface API
//--------------------------------------------------------------------+

uint8_t tuh_hid_instance_count(uint8_t dev_addr)
{
  return get_dev(dev_addr)->inst_count;
}

bool tuh_hid_mounted(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return (hid_itf->ep_in != 0) || (hid_itf->ep_out != 0);
}

uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->itf_protocol;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

uint8_t tuh_hid_get_protocol(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  return hid_itf->protocol_mode;
}

static bool set_protocol_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  uint8_t const itf_num     = (uint8_t) request->wIndex;
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  if (XFER_RESULT_SUCCESS == result) hid_itf->protocol_mode = (uint8_t) request->wValue;

  if (tuh_hid_set_protocol_complete_cb)
  {
    tuh_hid_set_protocol_complete_cb(dev_addr, instance, hid_itf->protocol_mode);
  }

  return true;
}

bool tuh_hid_set_protocol(uint8_t dev_addr, uint8_t instance, uint8_t protocol)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  TU_VERIFY(hid_itf->itf_protocol != HID_ITF_PROTOCOL_NONE);

  TU_LOG2("HID Set Protocol = %d\r\n", protocol);

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_PROTOCOL,
    .wValue   = protocol,
    .wIndex   = hid_itf->itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, set_protocol_complete) );
  return true;
}

static bool set_report_complete(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_LOG2("HID Set Report complete\r\n");

  if (tuh_hid_set_report_complete_cb)
  {
    uint8_t const itf_num     = (uint8_t) request->wIndex;
    uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);

    uint8_t const report_type = tu_u16_high(request->wValue);
    uint8_t const report_id   = tu_u16_low(request->wValue);

    tuh_hid_set_report_complete_cb(dev_addr, instance, report_id, report_type, (result == XFER_RESULT_SUCCESS) ? request->wLength : 0);
  }

  return true;
}

bool tuh_hid_set_report(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, void* report, uint16_t len)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
  TU_LOG2("HID Set Report: id = %u, type = %u, len = %u\r\n", report_id, report_type, len);

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_REPORT,
    .wValue   = tu_u16(report_type, report_id),
    .wIndex   = hid_itf->itf_num,
    .wLength  = len
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, report, set_report_complete) );
  return true;
}

//--------------------------------------------------------------------+
// Interrupt Endpoint API
//--------------------------------------------------------------------+

bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // claim endpoint
  TU_VERIFY( usbh_edpt_claim(dev_addr, hid_itf->ep_in) );

  return usbh_edpt_xfer(dev_addr, hid_itf->ep_in, hid_itf->epin_buf, hid_itf->epin_size);
}

//bool tuh_n_hid_n_ready(uint8_t dev_addr, uint8_t instance)
//{
//  TU_VERIFY(tuh_n_hid_n_mounted(dev_addr, instance));
//
//  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);
//  return !usbh_edpt_busy(dev_addr, hid_itf->ep_in);
//}

//void tuh_hid_send_report(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t const* report, uint16_t len);
#endif
//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
void midih_init(void)
{
  tu_memclr(&_midi_host, sizeof(_midi_host));

  // config fifo
  tu_fifo_config(&_midi_host.rx_ff, _midi_host.rx_ff_buf, CFG_TUH_MIDI_RX_BUFSIZE, 1, false); // true, true
  tu_fifo_config(&_midi_host.tx_ff, _midi_host.tx_ff_buf, CFG_TUH_MIDI_TX_BUFSIZE, 1, false); // OBVS.

  #if CFG_FIFO_MUTEX
  tu_fifo_config_mutex(&_midi_host.rx_ff, NULL, osal_mutex_create(&_midi_host.rx_ff_mutex));
  tu_fifo_config_mutex(&_midi_host.tx_ff, osal_mutex_create(&_midi_host.tx_ff_mutex), NULL);
  #endif
}

bool midih_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{

  if ( ep_addr == _midi_host.ep_in)
  {
    if (0 == xferred_bytes)
    {
      return true; // No data to handle
    }

    // receive new data if available
    if (xferred_bytes)
    {
      // put in the RX FIFO only non-zero MIDI IN 4-byte packets
      uint8_t* buf = _midi_host.epin_buf;
      uint32_t npackets = xferred_bytes / 4;
      uint32_t packet_num;
      uint32_t packets_queued = 0;
      for (packet_num = 0; packet_num < npackets; packet_num++)
      {
        // some devices send back all zero packets even if there is no data ready
        uint32_t packet = (uint32_t)((*buf)<<24) | ((uint32_t)(*(buf+1))<<16) | ((uint32_t)(*(buf+2))<<8) | ((uint32_t)(*(buf+3)));
        if (packet != 0)
        {
          tu_fifo_write_n(&_midi_host.rx_ff, buf, 4);
          ++packets_queued;
          TU_LOG3("MIDI RX=%08x\r\n", packet);
        }
        buf += 4;
      }


      // invoke receive callback if available
      if (tuh_midi_rx_cb)
      {
        tuh_midi_rx_cb(dev_addr, packets_queued);
      }
    }
  }
  else if ( ep_addr == _midi_host.ep_out )
  {
    if (0 == write_flush(dev_addr, &_midi_host))
    {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP size and not zero
      if ( !tu_fifo_count(&_midi_host.tx_ff) && xferred_bytes && (0 == (xferred_bytes % _midi_host.ep_out_max)) )
      {
        if ( usbh_edpt_claim(dev_addr, _midi_host.ep_out) )
        {
          usbh_edpt_xfer(dev_addr, _midi_host.ep_out, XFER_RESULT_SUCCESS, 0);
        }
      }
    }
  }

  return true;
}

void midih_close(uint8_t dev_addr)
{
  if (dev_addr == _midi_host.dev_addr)
  {
    if (tuh_midi_umount_cb)
      tuh_midi_umount_cb(dev_addr, 0);
    tu_fifo_clear(&_midi_host.rx_ff);
    tu_fifo_clear(&_midi_host.tx_ff);
    _midi_host.ep_in = 0;
    _midi_host.ep_in_max = 0;
    _midi_host.ep_out = 0;
    _midi_host.ep_out_max = 0;
    _midi_host.itf_num = 0;
    _midi_host.num_cables_rx = 0;
    _midi_host.num_cables_tx = 0;
    _midi_host.dev_addr = 255; // invalid
    _midi_host.configured = false;
    tu_memclr(&_midi_host.stream_read, sizeof(_midi_host.stream_read));
    tu_memclr(&_midi_host.stream_read, sizeof(_midi_host.stream_write));
  }
}

#if 0
// Invoked when device with midi interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);

}
#endif

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+
#if 0
static bool config_set_protocol             (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_get_report_desc          (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_get_report_desc_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

static void config_driver_mount_complete(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
#endif
bool midih_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  (void) rhport;
  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  // There can be just a MIDI interface or an audio and a MIDI interface. Only open the MIDI interface
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  uint16_t len_parsed = 0;
  if (AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass)
  {
    // This driver does not support audio streaming. However, if this is the audio control interface
    // there might be a MIDI interface following it. Search through every descriptor until a MIDI
    // interface is found or the end of the descriptor is found
    while (len_parsed < max_len && (desc_itf->bInterfaceClass != TUSB_CLASS_AUDIO || desc_itf->bInterfaceSubClass != AUDIO_SUBCLASS_MIDI_STREAMING))
    {
      len_parsed += desc_itf->bLength;
      p_desc = tu_desc_next(p_desc);
      desc_itf = (tusb_desc_interface_t const *)p_desc;
    }
    #if 0
    p_desc = tu_desc_next(p_desc);
    // p_desc now should point to the class-specific audio interface descriptor header
    audio_desc_cs_ac_interface_t const *p_cs_ac = (audio_desc_cs_ac_interface_t const *)p_desc;
    TU_VERIFY(p_cs_ac->bDescriptorType == TUSB_DESC_CS_INTERFACE);
    TU_VERIFY(p_cs_ac->bDescriptorSubType == AUDIO_CS_AC_INTERFACE_HEADER);
    if (p_cs_ac->bcdADC == 0x0200)
    {
      // skip the audio interface header
      p_desc += p_cs_ac->wTotalLength;
      len_parsed += p_cs_ac->wTotalLength;
    }
    else if (p_cs_ac->bcdADC == 0x0100)
    {
      // it's audio class 1.0
      audio_desc_cs_ac1_interface_t const *p_cs_ac1 = (audio_desc_cs_ac1_interface_t const *)p_desc;
      // skip the audio interface header
      p_desc += p_cs_ac1->wTotalLength;
      len_parsed += p_cs_ac1->wTotalLength;
    }
    else
    {
      return false;
    }
    desc_itf = (tusb_desc_interface_t const *)p_desc;
    #endif
    TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  }
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass);
  len_parsed += desc_itf->bLength;

  p_desc = tu_desc_next(p_desc);
  TU_LOG1("MIDI opening Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);
  // Find out if getting the MIDI class specific interface header or an endpoint descriptor
  // or a class-specific endpoint descriptor
  // Jack descriptors or element descriptors must follow the cs interface header,
  // but this driver does not support devices that contain element descriptors

  // assume it is an interface header
  midi_desc_header_t const *p_mdh = (midi_desc_header_t const *)p_desc;
  TU_VERIFY((p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE && p_mdh->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER) || 
    (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT && p_mdh->bDescriptorSubType == MIDI_MS_ENDPOINT_GENERAL) ||
    p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT);

  uint8_t prev_ep_addr = 0; // the CS endpoint descriptor is associated with the previous endpoint descrptor
  _midi_host.itf_num = desc_itf->bInterfaceNumber;
  tusb_desc_endpoint_t const* in_desc = NULL;
  tusb_desc_endpoint_t const* out_desc = NULL;
  while (len_parsed < max_len)
  {
    TU_VERIFY((p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) || 
      (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT && p_mdh->bDescriptorSubType == MIDI_MS_ENDPOINT_GENERAL) ||
      p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT);

    if (p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) {
      // The USB host doesn't really need this information unless it uses
      // the string descriptor for a jack or Element

      // assume it is an input jack
      midi_desc_in_jack_t const *p_mdij = (midi_desc_in_jack_t const *)p_desc;
      if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER)
      {
        TU_LOG2("Found MIDI Interface Header\r\b");
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_IN_JACK)
      {
        // Then it is an in jack. 
        TU_LOG2("Found in jack\r\n");
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_OUT_JACK)
      {
        // then it is an out jack
        TU_LOG2("Found out jack\r\n");
      }
      else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_ELEMENT)
      {
        // the it is an element;
        TU_LOG2("Found element\r\n");
      }
      else
      {
        TU_LOG2("Unknown CS Interface sub-type %u\r\n", p_mdij->bDescriptorSubType);
        TU_VERIFY(false); // unknown CS Interface sub-type
      }
      len_parsed += p_mdij->bLength;
    }
    else if (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT)
    {
      TU_LOG2("found CS_ENDPOINT Descriptor for %u\r\n", prev_ep_addr);
      TU_VERIFY(prev_ep_addr != 0);
      // parse out the mapping between the device's embedded jacks and the endpoints
      // Each embedded IN jack is assocated with an OUT endpoint
      midi_cs_desc_endpoint_t const* p_csep = (midi_cs_desc_endpoint_t const*)p_mdh;
      if (tu_edpt_dir(prev_ep_addr) == TUSB_DIR_OUT)
      {
        TU_VERIFY(_midi_host.ep_out == prev_ep_addr);
        TU_VERIFY(_midi_host.num_cables_tx == 0);
        _midi_host.num_cables_tx = p_csep->bNumEmbMIDIJack;
      }
      else
      {
        TU_VERIFY(_midi_host.ep_in == prev_ep_addr);
        TU_VERIFY(_midi_host.num_cables_rx == 0);
        _midi_host.num_cables_rx = p_csep->bNumEmbMIDIJack;
      }
      len_parsed += p_csep->bLength;
      prev_ep_addr = 0;
    }
    else if (p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT) {
      // parse out the bulk endpoint info
      tusb_desc_endpoint_t const *p_ep = (tusb_desc_endpoint_t const *)p_mdh;
      TU_LOG2("found ENDPOINT Descriptor for %u\r\n", p_ep->bEndpointAddress);
      if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT)
      {
        TU_VERIFY(_midi_host.ep_out == 0);
        TU_VERIFY(_midi_host.num_cables_tx == 0);
        _midi_host.ep_out = p_ep->bEndpointAddress;
        _midi_host.ep_out_max = p_ep->wMaxPacketSize;
        if (_midi_host.ep_out_max > CFG_TUH_MIDI_TX_BUFSIZE)
          _midi_host.ep_out_max = CFG_TUH_MIDI_TX_BUFSIZE;
        prev_ep_addr = _midi_host.ep_out;
        out_desc = p_ep;
      }
      else
      {
        TU_VERIFY(_midi_host.ep_in == 0);
        TU_VERIFY(_midi_host.num_cables_rx == 0);
        _midi_host.ep_in = p_ep->bEndpointAddress;
        _midi_host.ep_in_max = p_ep->wMaxPacketSize;
        if (_midi_host.ep_in_max > CFG_TUH_MIDI_RX_BUFSIZE)
          _midi_host.ep_in_max = CFG_TUH_MIDI_RX_BUFSIZE;
        prev_ep_addr = _midi_host.ep_in;
        in_desc = p_ep;
      }
      len_parsed += p_mdh->bLength;
    }
    p_desc = tu_desc_next(p_desc);
    p_mdh = (midi_desc_header_t const *)p_desc;
  }
  TU_VERIFY((_midi_host.ep_out != 0 && _midi_host.num_cables_tx != 0) ||
            (_midi_host.ep_in != 0 && _midi_host.num_cables_rx != 0));
  TU_LOG1("MIDI descriptor parsed successfully\r\n");

  if (in_desc)
  {
    TU_ASSERT(usbh_edpt_open(rhport, dev_addr, in_desc));
    // Some devices always return exactly the request length so transfers won't complete
    // unless you assume every transfer is the last one.
    usbh_edpt_force_last_buffer(dev_addr, _midi_host.ep_in, true);
  }
  if (out_desc)
  {
    TU_ASSERT(usbh_edpt_open(rhport, dev_addr, out_desc));
  }
  _midi_host.dev_addr = dev_addr;

  if (tuh_midi_mount_cb)
  {
    tuh_midi_mount_cb(dev_addr, _midi_host.ep_in, _midi_host.ep_out, _midi_host.num_cables_rx, _midi_host.num_cables_tx);
  }
  return true;
}

#if 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
  printf("MIDI endpoints OK. MIDI Interface opened\r\n");
}
#endif

bool tuh_midi_configured(void)
{
  return _midi_host.configured;
}
bool midih_set_config(uint8_t dev_addr, uint8_t itf_num)
{
  (void) itf_num;
  if (dev_addr == _midi_host.dev_addr)
    _midi_host.configured = true;
  // TODO I don't think there are any special config things to do for MIDI
  #if 0
  uint8_t const instance    = get_instance_id_by_itfnum(dev_addr, itf_num);
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // Idle rate = 0 mean only report when there is changes
  uint16_t const idle_rate = 0;

  // SET IDLE request, device can stall if not support this request
  TU_LOG2("HID Set Idle \r\n");
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HID_REQ_CONTROL_SET_IDLE,
    .wValue   = idle_rate,
    .wIndex   = itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, (hid_itf->itf_protocol != HID_ITF_PROTOCOL_NONE) ? config_set_protocol : config_get_report_desc) );
#endif
  return true;
}
#if 0
static void config_driver_mount_complete(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  hidh_interface_t* hid_itf = get_instance(dev_addr, instance);

  // enumeration is complete
  tuh_hid_mount_cb(dev_addr, instance, desc_report, desc_len);

  // notify usbh that driver enumeration is complete
  usbh_driver_set_config_complete(dev_addr, hid_itf->itf_num);
}


#endif
//--------------------------------------------------------------------+
// Stream API
//--------------------------------------------------------------------+
static uint32_t write_flush(uint8_t dev_addr, midih_interface_t* midi)
{
  // No data to send
  if ( !tu_fifo_count(&midi->tx_ff) ) return 0;

  // skip if previous transfer not complete
  TU_VERIFY( usbh_edpt_claim(dev_addr, midi->ep_out) );

  uint16_t count = tu_fifo_read_n(&midi->tx_ff, midi->epout_buf, midi->ep_out_max);

  if (count)
  {
    TU_ASSERT( usbh_edpt_xfer(dev_addr, midi->ep_out, midi->epout_buf, count), 0 );
    return count;
  }else
  {
    // Release endpoint since we don't make any transfer
    usbh_edpt_release(dev_addr, midi->ep_out);
    return 0;
  }
}

bool tuh_midi_read_poll( void )
{
  // MIDI bulk endpoints are shared with the control endpoints. None can be busy before we start a transfer
  bool control_edpt_not_busy = !usbh_edpt_busy(_midi_host.dev_addr,0) && !usbh_edpt_busy(_midi_host.dev_addr,0x80);
  bool out_edpt_not_busy = true;
  if (_midi_host.num_cables_tx > 0)
    out_edpt_not_busy = !usbh_edpt_busy(_midi_host.dev_addr,_midi_host.ep_out);
  if (!usbh_edpt_busy(_midi_host.dev_addr, _midi_host.ep_in) && control_edpt_not_busy && out_edpt_not_busy)
  {
    TU_LOG3("Requesting poll IN endpoint %d\r\n", _midi_host.ep_in);
    TU_ASSERT(usbh_edpt_xfer(_midi_host.dev_addr, _midi_host.ep_in, _midi_host.epin_buf, _midi_host.ep_in_max), 0);
  }
  return true;
}

uint32_t tuh_midi_stream_write (uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize)
{
  bool control_edpt_busy = usbh_edpt_busy(_midi_host.dev_addr,0) || usbh_edpt_busy(_midi_host.dev_addr,0x80);
  bool in_edpt_busy = false;
  if (_midi_host.num_cables_rx > 0)
    in_edpt_busy = usbh_edpt_busy(_midi_host.dev_addr,_midi_host.ep_in);
  if (control_edpt_busy || in_edpt_busy || usbh_edpt_busy(_midi_host.dev_addr, _midi_host.ep_out))
  {
    return 0; // can't send a packet now
  }
  TU_VERIFY(cable_num < _midi_host.num_cables_tx);
  midi_stream_t stream = _midi_host.stream_write;

  uint32_t i = 0;
  while ( (i < bufsize) && (tu_fifo_remaining(&_midi_host.tx_ff) >= 4) )
  {
    uint8_t const data = buffer[i];
    i++;

    if ( stream.index == 0 )
    {
      //------------- New event packet -------------//

      uint8_t const msg = data >> 4;

      stream.index = 2;
      stream.buffer[1] = data;

      // Check to see if we're still in a SysEx transmit.
      if ( stream.buffer[0] == MIDI_CIN_SYSEX_START )
      {
        if ( data == MIDI_STATUS_SYSEX_END )
        {
          stream.buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
          stream.total = 2;
        }
        else
        {
          stream.total = 4;
        }
      }
      else if ( (msg >= 0x8 && msg <= 0xB) || msg == 0xE )
      {
        // Channel Voice Messages
        stream.buffer[0] = (cable_num << 4) | msg;
        stream.total = 4;
      }
      else if ( msg == 0xC || msg == 0xD)
      {
        // Channel Voice Messages, two-byte variants (Program Change and Channel Pressure)
        stream.buffer[0] = (cable_num << 4) | msg;
        stream.total = 3;
      }
      else if ( msg == 0xf )
      {
        // System message
        if ( data == MIDI_STATUS_SYSEX_START )
        {
          stream.buffer[0] = MIDI_CIN_SYSEX_START;
          stream.total = 4;
        }
        else if ( data == MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME || data == MIDI_STATUS_SYSCOM_SONG_SELECT )
        {
          stream.buffer[0] = MIDI_CIN_SYSCOM_2BYTE;
          stream.total = 3;
        }
        else if ( data == MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER )
        {
          stream.buffer[0] = MIDI_CIN_SYSCOM_3BYTE;
          stream.total = 4;
        }
        else
        {
          stream.buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
          stream.total = 2;
        }
      }
      else
      {
        // Pack individual bytes if we don't support packing them into words.
        stream.buffer[0] = cable_num << 4 | 0xf;
        stream.buffer[2] = 0;
        stream.buffer[3] = 0;
        stream.index = 2;
        stream.total = 2;
      }
    }
    else
    {
      //------------- On-going (buffering) packet -------------//

      TU_ASSERT(stream.index < 4, i);
      stream.buffer[stream.index] = data;
      stream.index++;

      // See if this byte ends a SysEx.
      if ( stream.buffer[0] == MIDI_CIN_SYSEX_START && data == MIDI_STATUS_SYSEX_END )
      {
        stream.buffer[0] = MIDI_CIN_SYSEX_START + (stream.index - 1);
        stream.total = stream.index;
      }
    }

    // Send out packet
    if ( stream.index == stream.total )
    {
      // zeroes unused bytes
      for(uint8_t idx = stream.total; idx < 4; idx++) stream.buffer[idx] = 0;

      uint16_t const count = tu_fifo_write_n(&_midi_host.tx_ff, stream.buffer, 4);

      // complete current event packet, reset stream
      stream.index = 0;
      stream.total = 0;

      // FIFO overflown, since we already check fifo remaining. It is probably race condition
      TU_ASSERT(count == 4, i);
    }
  }

  write_flush(_midi_host.dev_addr, &_midi_host);

  return i;
}


//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
uint8_t tuh_midih_get_num_tx_cables (void)
{
  TU_VERIFY(_midi_host.ep_out != 0); // returns 0 if fails
  return _midi_host.num_cables_tx;
}

uint8_t tuh_midih_get_num_rx_cables (void)
{
  TU_VERIFY(_midi_host.ep_in != 0); // returns 0 if fails
  return _midi_host.num_cables_rx;
}

uint32_t tuh_midi_stream_read (uint8_t dev_addr, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize)
{
  uint32_t bytes_buffered = 0;
  if (_midi_host.dev_addr == dev_addr)
  {
    TU_ASSERT(p_cable_num);
    TU_ASSERT(p_buffer);
    TU_ASSERT(bufsize);
    uint8_t one_byte;
    if (!tu_fifo_peek(&_midi_host.rx_ff, &one_byte))
    {
      return 0;
    }
    *p_cable_num = (one_byte >> 4) & 0xf;
    uint32_t nread = tu_fifo_read_n(&_midi_host.rx_ff, _midi_host.stream_read.buffer, 4);
    static uint16_t cable_sysex_in_progress; // bit i is set if received MIDI_STATUS_SYSEX_START but not MIDI_STATUS_SYSEX_END
    while (nread == 4 && bytes_buffered < bufsize)
    {
      *p_cable_num=(_midi_host.stream_read.buffer[0] & 0xf) >> 4;
      uint8_t bytes_to_add_to_stream = 0;
      if (*p_cable_num < _midi_host.num_cables_rx)
      {
        // ignore the CIN field; too many devices out there encode this wrong
        uint8_t status = _midi_host.stream_read.buffer[1];
        uint16_t cable_mask = 1 << *p_cable_num;
        if (status <= MIDI_MAX_DATA_VAL || status == MIDI_STATUS_SYSEX_START)
        {
          if (status == MIDI_STATUS_SYSEX_START)
          {
            cable_sysex_in_progress |= cable_mask;
          }
          // only add the packet if a sysex message is in progress
          if (cable_sysex_in_progress & cable_mask)
          {
            ++bytes_to_add_to_stream;
            uint8_t idx;
            for (idx = 2; idx < 4; idx++)
            {
              if (_midi_host.stream_read.buffer[idx] <= MIDI_MAX_DATA_VAL)
              {
                ++bytes_to_add_to_stream;
              }
              else if (_midi_host.stream_read.buffer[idx] == MIDI_STATUS_SYSEX_END)
              {
                ++bytes_to_add_to_stream;
                cable_sysex_in_progress &= ~cable_mask;
                idx = 4; // force the loop to exit; I hate break statements in loops
              }
            }
          }
        }
        else if (status < MIDI_STATUS_SYSEX_START)
        {
          // then it is a channel message either three bytes or two
          uint8_t fake_cin = (status & 0xf0) >> 4;
          switch (fake_cin)
          {
            case MIDI_CIN_NOTE_OFF:
            case MIDI_CIN_NOTE_ON:
            case MIDI_CIN_POLY_KEYPRESS:
            case MIDI_CIN_CONTROL_CHANGE:
            case MIDI_CIN_PITCH_BEND_CHANGE:
              bytes_to_add_to_stream = 3;
              break;
            case MIDI_CIN_PROGRAM_CHANGE:
            case MIDI_CIN_CHANNEL_PRESSURE:
              bytes_to_add_to_stream = 2;
              break;
            default:
              break; // Should not get this
          }
          cable_sysex_in_progress &= ~cable_mask;
        }
        else if (status < MIDI_STATUS_SYSREAL_TIMING_CLOCK)
        {
          switch (status)
          {
            case MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME:
            case MIDI_STATUS_SYSCOM_SONG_SELECT:
              bytes_to_add_to_stream = 2;
              break;
            case MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER:
              bytes_to_add_to_stream = 3;
              break;
            case MIDI_STATUS_SYSCOM_TUNE_REQUEST:
            case MIDI_STATUS_SYSEX_END:
              bytes_to_add_to_stream = 1;
              break;
            default:
              break;
            cable_sysex_in_progress &= ~cable_mask;
          }
        }
        else
        {
          // Real-time message: can be inserted into a sysex message,
          // so do don't clear cable_sysex_in_progress bit
          bytes_to_add_to_stream = 1;
        }
      }
      uint8_t idx;
      for (idx = 1; idx <= bytes_to_add_to_stream; idx++)
      {
        *p_buffer++ = _midi_host.stream_read.buffer[idx];
      }
      bytes_buffered += bytes_to_add_to_stream;
      nread = 0;
      if (tu_fifo_peek(&_midi_host.rx_ff, &one_byte))
      {
        uint8_t new_cable = (one_byte >> 4) & 0xf;
        if (new_cable == *p_cable_num)
        {
          // still on the same cable. Continue reading the stream
          nread = tu_fifo_read_n(&_midi_host.rx_ff, _midi_host.stream_read.buffer, 4);
        }
      }
    }
  }

  return bytes_buffered;
}
#endif
