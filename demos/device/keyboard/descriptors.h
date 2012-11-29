/*
 * descriptors.h
 *
 *  Created on: Nov 26, 2012
 *      Author: hathachtware License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_

#include "tusb.h"

#define CFG_USB_STRING_MANUFACTURER   "tinyUSB"
#define CFG_USB_STRING_PRODUCT        "Device Keyboard"
#define CFG_USB_STRING_SERIAL          "1234"
#define CFG_USB_VENDORID              0x1FC9

/* USB Serial uses the MCUs unique 128-bit chip ID via an IAP call = 32 hex chars */
#define USB_STRING_SERIAL_LEN     32

#define USB_STRING_LEN(n) (2 + ((n)<<1))

typedef PRE_PACK struct POST_PACK _USB_STR_DESCRIPTOR
{
  USB_COMMON_DESCRIPTOR LangID;
  uint16_t strLangID[1];

  USB_COMMON_DESCRIPTOR Manufacturer;
  uint16_t strManufacturer[sizeof(CFG_USB_STRING_MANUFACTURER)-1]; // exclude null-character

  USB_COMMON_DESCRIPTOR Product;
  uint16_t strProduct[sizeof(CFG_USB_STRING_PRODUCT)-1]; // exclude null-character

  USB_COMMON_DESCRIPTOR Serial;
  uint16_t strSerial[sizeof(CFG_USB_STRING_SERIAL)-1];
} USB_STR_DESCRIPTOR;

// USB Interface Assosication Descriptor
#define  USB_DEVICE_CLASS_IAD        USB_DEVICE_CLASS_MISCELLANEOUS
#define  USB_DEVICE_SUBCLASS_IAD     0x02
#define  USB_DEVICE_PROTOCOL_IAD     0x01

// USB Interface Association Descriptor
typedef PRE_PACK struct POST_PACK _USB_INTERFACE_ASSOCIATION_DESCRIPTOR
{
  uint8_t bLength;           /**< Size of descriptor*/
  uint8_t bDescriptorType;   /**< Other_speed_Configuration Type*/

  uint8_t bFirstInterface;   /**< Index of the first associated interface. */
  uint8_t bInterfaceCount;   /**< Total number of associated interfaces. */

  uint8_t bFunctionClass;    /**< Interface class ID. */
  uint8_t bFunctionSubClass; /**< Interface subclass ID. */
  uint8_t bFunctionProtocol; /**< Interface protocol ID. */

  uint8_t iFunction;         /**< Index of the string descriptor describing the interface association. */
} USB_INTERFACE_ASSOCIATION_DESCRIPTOR;

///////////////////////////////////////////////////////////////////////
// Interface Assosication Descriptor if device is CDC + other class
#define IAD_DESC_REQUIRED ( defined(CFG_CLASS_CDC) && (CLASS_HID) )

#ifdef CFG_CLASS_CDC
  #define INTERFACES_OF_CDC           2
#else
  #define INTERFACES_OF_CDC           0
#endif

#ifdef CFG_CLASS_HID_KEYBOARD
  #define INTERFACES_OF_HID_KEYBOARD  1
#else
  #define INTERFACES_OF_HID_KEYBOARD  0
#endif

#ifdef CFG_CLASS_HID_MOUSE
  #define INTERFACES_OF_HID_MOUSE     1
#else
  #define INTERFACES_OF_HID_MOUSE     0
#endif

#ifdef CFG_USB_HID_GENERIC
  #define INTERFACES_OF_HID_GENERIC   1
#else
  #define INTERFACES_OF_HID_GENERIC   0
#endif

#ifdef CFG_USB_MASS_STORAGE
  #define INTERFACES_OF_MASS_STORAGE  2
#else
  #define INTERFACES_OF_MASS_STORAGE  0
#endif

#define INTERFACE_INDEX_CDC           0
#define INTERFACE_INDEX_HID_KEYBOARD (INTERFACE_INDEX_CDC          + INTERFACES_OF_CDC          )
#define INTERFACE_INDEX_HID_MOUSE    (INTERFACE_INDEX_HID_KEYBOARD + INTERFACES_OF_HID_KEYBOARD )
#define INTERFACE_INDEX_HID_GENERIC  (INTERFACE_INDEX_HID_MOUSE    + INTERFACES_OF_HID_MOUSE    )
#define INTERFACE_INDEX_MASS_STORAGE (INTERFACE_INDEX_HID_GENERIC  + INTERFACES_OF_HID_GENERIC  )

#define TOTAL_INTEFACES              (INTERFACES_OF_CDC + INTERFACES_OF_HID_KEYBOARD + INTERFACES_OF_HID_MOUSE + INTERFACES_OF_HID_GENERIC + INTERFACES_OF_MASS_STORAGE)

// Bitmap: MassStorage | Generic | Mouse | Key | CDC
#define PRODUCTID_BITMAP(interface, n)  ( (INTERFACES_OF_##interface ? 1 : 0) << (n) )
#define USB_PRODUCT_ID                  (0x2000 | ( PRODUCTID_BITMAP(CDC, 0) | PRODUCTID_BITMAP(HID_KEYBOARD, 1) | PRODUCTID_BITMAP(HID_MOUSE, 2) | \
                                                    PRODUCTID_BITMAP(HID_GENERIC, 3) | PRODUCTID_BITMAP(MASS_STORAGE, 4) ) )

///////////////////////////////////////////////////////////////////////
typedef struct
{
  USB_CONFIGURATION_DESCRIPTOR                Config;

#if IAD_DESC_REQUIRED
  USB_INTERFACE_ASSOCIATION_DESCRIPTOR        CDC_IAD;
#endif

#ifdef CFG_CLASS_CDC
  //CDC - Serial
  //CDC Control Interface
  USB_INTERFACE_DESCRIPTOR                    CDC_CCI_Interface;
  CDC_HEADER_DESCRIPTOR                       CDC_Header;
  CDC_ABSTRACT_CONTROL_MANAGEMENT_DESCRIPTOR  CDC_ACM;
  CDC_UNION_1SLAVE_DESCRIPTOR                 CDC_Union;
  USB_ENDPOINT_DESCRIPTOR                     CDC_NotificationEndpoint;

  //CDC Data Interface
  USB_INTERFACE_DESCRIPTOR                    CDC_DCI_Interface;
  USB_ENDPOINT_DESCRIPTOR                     CDC_DataOutEndpoint;
  USB_ENDPOINT_DESCRIPTOR                     CDC_DataInEndpoint;
#endif

#ifdef CFG_CLASS_HID_KEYBOARD
  //Keyboard HID Interface
  USB_INTERFACE_DESCRIPTOR                    HID_KeyboardInterface;
  HID_DESCRIPTOR                              HID_KeyboardHID;
  USB_ENDPOINT_DESCRIPTOR                     HID_KeyboardEndpoint;
#endif

#ifdef CFG_CLASS_HID_MOUSE
  //Mouse HID Interface
  USB_INTERFACE_DESCRIPTOR                    HID_MouseInterface;
  HID_DESCRIPTOR                              HID_MouseHID;
  USB_ENDPOINT_DESCRIPTOR                     HID_MouseEndpoint;
#endif

  unsigned char                               ConfigDescTermination;
} USB_FS_CONFIGURATION_DESCRIPTOR;

extern const USB_DEVICE_DESCRIPTOR USB_DeviceDescriptor;
extern const USB_FS_CONFIGURATION_DESCRIPTOR USB_FsConfigDescriptor;
extern const USB_STR_DESCRIPTOR USB_StringDescriptor;

extern const uint8_t HID_KeyboardReportDescriptor[];
extern const uint8_t HID_MouseReportDescriptor[];

#endif
