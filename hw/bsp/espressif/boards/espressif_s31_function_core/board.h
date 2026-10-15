/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, Ha Thach (tinyusb.org)
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

/* metadata:
   name: Espressif S31 Function CoreBoard
   url: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/index.html
   note: J4 is a host-wired Type-A port that always supplies VBUS; device role needs an A-to-A cable with VBUS cut
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

// The HS OTG port is the Type-A jack (J4), meant for host role: its VBUS switch (TPS2051C) is
// always enabled. Device role needs an A-to-A link, which back-feeds the PC unless VBUS is cut.

#define NEOPIXEL_PIN          60

#define BUTTON_PIN            61
#define BUTTON_STATE_ACTIVE   0

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
