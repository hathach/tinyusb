/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Portions Copyright (c) 2022 Travis Robinson (libusbdotnet@gmail.com)
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

#include "tusb.h"

// QinHeng Electronics
#define USB_VID   0x1A86
// CH341 serial converter
#define USB_PID   0x7523

//--------------------------------------------------------------------+
// Generic Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_GEN_INTERFACE_DESCRIPTOR(_itfnum, _altsetting, _num_endpoints, _itfclass, _itfsubclass, _itfprotocol, _itfstridx) \
	9, TUSB_DESC_INTERFACE, _itfnum, _altsetting, _num_endpoints, _itfclass, _itfsubclass, _itfprotocol, _itfstridx

#define TUD_GEN_BULK_EP_DESCRIPTOR(_epnum, _epsize) \
	7, TUSB_DESC_ENDPOINT, _epnum, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

#define TUD_GEN_INTERRUPT_EP_DESCRIPTOR(_epnum, _epsize, _interval) \
	7, TUSB_DESC_ENDPOINT, _epnum, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _interval

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
#if TUD_OPT_HIGH_SPEED
		// atleast 0x0200 required for HS
    .bcdUSB             = 0x0200,
#else
		// this is what's in a genuine CH340G's descriptor
    .bcdUSB             = 0x0110,
#endif
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0263, // CH340G

	  // A CH340G only has a product string.  All other string indexes are 0.
    // The driver doesn't seen to care so we will specify all of them.
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
  ITF_NUM_CH341,
  ITF_NUM_TOTAL
};

// interface + 3 endpoints
#define TUD_CH341_DESC_LEN  (9+7+7+7)
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CH341_DESC_LEN)

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_CH341_0_NOTIF   0x81
  #define EPNUM_CH341_0_OUT     0x02
  #define EPNUM_CH341_0_IN      0x82
#elif CFG_TUSB_MCU == OPT_MCU_SAMG || CFG_TUSB_MCU ==  OPT_MCU_SAMX7X
  // SAMG & SAME70 don't support a same endpoint number with different direction IN and OUT
  //    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_CH341_0_NOTIF   0x81
  #define EPNUM_CH341_0_OUT     0x02
  #define EPNUM_CH341_0_IN      0x83
#else
  #define EPNUM_CH341_0_NOTIF   0x81
  #define EPNUM_CH341_0_OUT     0x02
  #define EPNUM_CH341_0_IN      0x82
#endif

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),
  TUD_GEN_INTERFACE_DESCRIPTOR(0, 0, 3, 0xFF, 1, 2 , 0),
  TUD_GEN_BULK_EP_DESCRIPTOR(EPNUM_CH341_0_IN, CFG_TUD_CH341_EP_TX_MAX_PACKET),
  TUD_GEN_BULK_EP_DESCRIPTOR(EPNUM_CH341_0_OUT, CFG_TUD_CH341_EP_RX_MAX_PACKET),
	TUD_GEN_INTERRUPT_EP_DESCRIPTOR(EPNUM_CH341_0_NOTIF, 8, 1)
 };

#if TUD_OPT_HIGH_SPEED

// TEST: Im not sure if the ch341 windows driver will allow high speed descriptors.
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

uint8_t const desc_hs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),
  TUD_GEN_INTERFACE_DESCRIPTOR(0, 0, 3, 0xFF, 1, 2 , 0),
  TUD_GEN_BULK_EP_DESCRIPTOR(EPNUM_CH341_0_IN, CFG_TUD_CH341_EP_TX_MAX_PACKET),
  TUD_GEN_BULK_EP_DESCRIPTOR(EPNUM_CH341_0_OUT, CFG_TUD_CH341_EP_RX_MAX_PACKET),
	TUD_GEN_INTERRUPT_EP_DESCRIPTOR(EPNUM_CH341_0_NOTIF, 8, 1)
};

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  
  .bDeviceClass       = 0xFF,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // if link speed is high return fullspeed config, and vice versa
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_fs_configuration : desc_hs_configuration;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "CH341 serial converter",      // 2: Product
  "123456",                      // 3: Serials, should use chip ID
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}
