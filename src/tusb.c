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

#if TUSB_OPT_HOST_ENABLED || TUSB_OPT_DEVICE_ENABLED

#include "tusb.h"

static bool _initialized = false;

// TODO clean up
#if TUSB_OPT_DEVICE_ENABLED
#include "device/usbd_pvt.h"
#endif

bool tusb_init(void)
{
  // skip if already initialized
  if (_initialized) return true;

#if TUSB_OPT_HOST_ENABLED
  TU_VERIFY( usbh_init() ); // init host stack
#endif

#if TUSB_OPT_DEVICE_ENABLED
  TU_VERIFY ( usbd_init() ); // init device stack
#endif

  _initialized = true;

  return TUSB_ERROR_NONE;
}

bool tusb_inited(void)
{
  return _initialized;
}

/*------------------------------------------------------------------*/
/* Debug
 *------------------------------------------------------------------*/
#if CFG_TUSB_DEBUG
char const* const tusb_strerr[TUSB_ERROR_COUNT] = { ERROR_TABLE(ERROR_STRING) };
#endif

#endif // host or device enabled
