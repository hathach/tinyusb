/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
