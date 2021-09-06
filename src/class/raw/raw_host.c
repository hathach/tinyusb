/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 BlueChip
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

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_RAW)

//----------------------------------------------------------------------------- ----------------------------------------
// INCLUDE
//
#include "host/usbh.h"
#include "raw_host.h"

//----------------------------------------------------------------------------- ----------------------------------------
// MACRO CONSTANT TYPEDEF

//----------------------------------------------------------------------------- ----------------------------------------
// INTERNAL OBJECT & FUNCTION DECLARATION
//
typedef
	struct {
		int i;
	}
raw_dev_t;

raw_dev_t  raw_dev[CFG_TUH_DEVICE_MAX] ;

//+============================================================================ ========================================
// USBH-CLASS API [1/5] : .init
//
void  raw_init (void)
{
	printf("# raw_init()\r\n");
//	raw_init_pre_cb(raw_dev);

	tu_memclr(raw_dev, sizeof(raw_dev));

//	raw_init_post_cb(raw_dev);
}

//+============================================================================ ========================================
// USBH-CLASS API [2/5] : .open
//
bool  raw_open (uint8_t rhport,  uint8_t dev_addr,  tusb_desc_interface_t const *itf_desc,  uint16_t max_len)
{
	uint16_t  vid, pid;

	printf("# raw_open(port=%d, dev=%d, *desc=%p, max=%d)\r\n", rhport, dev_addr, itf_desc, max_len);
	// src/common/tusb_types.h
	printf("    desc.len   = %d\r\n", itf_desc->bLength           );  // descriptor size
	printf("    desc.type  = %d\r\n", itf_desc->bDescriptorType   );  // INTERFACE Descriptor Type
	printf("    desc.ifNum = %d\r\n", itf_desc->bInterfaceNumber  );  // interface number (zero based)
	printf("    desc.alt   = %d\r\n", itf_desc->bAlternateSetting );  // value to select this setting
	printf("    desc.epCnt = %d\r\n", itf_desc->bNumEndpoints     );  // number of endpoints (not couting ep0)
	printf("    desc.Class = %d\r\n", itf_desc->bInterfaceClass   );  // USB-IF class code
	printf("    desc.SubCl = %d\r\n", itf_desc->bInterfaceSubClass);  // USB-IF subclass code
	printf("    desc.proto = %d\r\n", itf_desc->bInterfaceProtocol);  // protocol code
	printf("    desc.ifStr = %d\r\n", itf_desc->iInterface        );  // index of string
//	raw_open_pre_cb(rhport, dev_addr, itf_desc, max_len);

	// Why does this FAIL (only) the first time it is called? ...What is not initialised properly?
	tuh_vid_pid_get(dev_addr, &vid, &pid);
	printf("  VID=%04X, PID=%04X\r\n", vid, pid);




//	raw_open_post_cb(rhport, dev_addr, itf_desc, max_len);
	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [3/5] : .set_config
//
bool  raw_set_config (uint8_t dev_addr,  uint8_t itf_num)
{
	uint16_t  vid, pid;

	printf("# raw_set_config(dev=%d, num=%d)\r\n", dev_addr, itf_num);
//	raw_set_config_pre_cb(dev_addr, itf_num);

	tuh_vid_pid_get(dev_addr, &vid, &pid);
	printf("  VID=%04X, PID=%04X\r\n", vid, pid);

//	raw_set_config_post_cb(dev_addr, itf_num);
	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [4/5] : .xfer_cb
//
bool  raw_xfer_cb (uint8_t dev_addr,  uint8_t ep_addr,  xfer_result_t event,  uint32_t xferred_bytes)
{
	printf("# raw_xfer_cb(dev=%d, ep=%d, event=%d, bytes=%d)\r\n", dev_addr, ep_addr, event, xferred_bytes);
//	raw_xfer_pre_cb(dev_addr, ep_addr, event, xferred_bytes);


//	raw_xfer_post_cb(dev_addr, ep_addr, event, xferred_bytes);
	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [5/5] : .close
//
void  raw_close (uint8_t dev_addr)
{
	printf("# raw_close(dev=%d)\r\n\r\n", dev_addr);
//	raw_close_pre_cb(dev_addr);

	TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX, );
//	tu_memclr((raw_data_t*)get_itf(dev_addr), sizeof(raw_data_t));

//	raw_close_post_cb(dev_addr);
	return;
}

#endif
