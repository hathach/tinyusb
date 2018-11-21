/**************************************************************************/
/*!
    @file     tusb_descriptors.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb.h"

//--------------------------------------------------------------------+
// STRING DESCRIPTORS
//--------------------------------------------------------------------+

// array of pointer to string descriptors
uint16_t const * const string_desc_arr [] =
{
    // 0: is supported language = English
    TUD_DESC_STRCONV(0x0409),

    // 1: Manufacturer
    TUD_DESC_STRCONV('t', 'i', 'n', 'y', 'u', 's', 'b', '.', 'o', 'r', 'g'),

    // 2: Product
    TUD_DESC_STRCONV('t', 'i', 'n', 'y', 'u', 's', 'b', ' ', 'd', 'e', 'v', 'i', 'c', 'e'),

    // 3: Serials TODO use chip ID
    TUD_DESC_STRCONV('1', '2', '3', '4', '5', '6'),

#if CFG_TUD_CDC
    // 4: CDC Interface
    TUD_DESC_STRCONV('t','u','s','b',' ','c','d','c'),
#endif

#if CFG_TUD_MSC
    // 5: MSC Interface
    TUD_DESC_STRCONV('t','u','s','b',' ','m','s','c'),
#endif

#if CFG_TUD_HID_KEYBOARD
    // 6: Keyboard
    TUD_DESC_STRCONV('t','u','s','b',' ','k','e','y','b','o','a','r','d'),
#endif

#if CFG_TUD_HID_MOUSE
    // 7: Mouse
    TUD_DESC_STRCONV('t','u','s','b',' ','m', 'o','u','s','e'),
#endif

};

// tud_desc_set is required by tinyusb stack
// since CFG_TUD_DESC_AUTO is enabled, we only need to set string_arr 
tud_desc_set_t tud_desc_set =
{
    .device     = NULL,
    .config     = NULL,

    .string_arr   = (uint8_t const **) string_desc_arr,
    .string_count = sizeof(string_desc_arr)/sizeof(string_desc_arr[0]),

    .hid_report =
    {
        .generic       = NULL,
        .boot_keyboard = NULL,
        .boot_mouse    = NULL
    }
};
