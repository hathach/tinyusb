/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 BlueChip
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

#ifndef _TUSB_MIDI_HOST_H_
#define _TUSB_MIDI_HOST_H_

#include "common/tusb_common.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef __cplusplus
	extern "C" {
#endif

//----------------------------------------------------------------------------- ----------------------------------------
void  midih_init       (void) ;
bool  midih_open       (uint8_t rhport,  uint8_t dev_addr,  tusb_desc_interface_t const *desc_itf,  uint16_t max_len) ;
bool  midih_set_config (uint8_t dev_addr,  uint8_t itf_num) ;
bool  midih_xfer_cb    (uint8_t dev_addr,  uint8_t ep_addr,  xfer_result_t event,  uint32_t xferred_bytes) ;
void  midih_close      (uint8_t dev_addr) ;

#ifdef __cplusplus
	}
#endif

#endif /* _TUSB_MIDI_HOST_H_ */
