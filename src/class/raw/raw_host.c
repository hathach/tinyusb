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
	uint8_t const*  pDesc = NULL;
	uint8_t const*  pLast = NULL;  // BUG: Sometimes the last descriptor points at itself as being 'next'

	uint16_t        vid   = 0x0000;
	uint16_t        pid   = 0x0000;

	printf("# raw_open(port=%d, dev=%d, *desc=%p, max=%d)\r\n", rhport, dev_addr, itf_desc, max_len);
//	raw_open_pre_cb(rhport, dev_addr, itf_desc, max_len);

	// Why does this FAIL (only) the first time it is called? ...What is not initialised properly?
	tuh_vid_pid_get(dev_addr, &vid, &pid);
	printf("  VID=%04X, PID=%04X\r\n", vid, pid);

	// src/common/tusb_types.h
//	for (pDesc = (void*)itf_desc;  tu_desc_type(pDesc);  pDesc = tu_desc_next(pDesc)) {
	for (pDesc = (void*)itf_desc;  tu_desc_type(pDesc) && (pDesc != pLast);  pDesc = tu_desc_next((pLast = pDesc))) {
		//printf("  (pDesc) = 0x%p\r\n", pDesc);
		switch (tu_desc_type(pDesc)) {
			case TUSB_DESC_DEVICE                           : {  // 0x01,
				printf("    TUSB_DESC_DEVICE [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_CONFIGURATION                    : {  // 0x02,
				printf("    TUSB_DESC_CONFIGURATION [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_STRING                           : {  // 0x03,
				printf("    TUSB_DESC_STRING [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_INTERFACE                        : {  // 0x04,
				printf("    TUSB_DESC_INTERFACE [%02X]\r\n", tu_desc_type(pDesc));
				tusb_desc_interface_t const* p = (tusb_desc_interface_t const*)pDesc;
				printf("      desc.len   = %d\r\n"   , p->bLength           );  // descriptor size
				printf("      desc.type  = x%02X\r\n", p->bDescriptorType   );  // Descriptor Type
				printf("      desc.ifNum = %d\r\n"   , p->bInterfaceNumber  );  // interface number (zero based)
				printf("      desc.alt   = %d\r\n"   , p->bAlternateSetting );  // value to select this setting
				printf("      desc.epCnt = %d\r\n"   , p->bNumEndpoints     );  // number of endpoints (not couting ep0)
				printf("      desc.Class = %d\r\n"   , p->bInterfaceClass   );  // USB-IF class code
				printf("      desc.SubCl = %d\r\n"   , p->bInterfaceSubClass);  // USB-IF subclass code
				printf("      desc.proto = %d\r\n"   , p->bInterfaceProtocol);  // protocol code
				printf("      desc.ifStr = %d\r\n"   , p->iInterface        );  // index of string
				break;
			}
			case TUSB_DESC_ENDPOINT                         : {  // 0x05,
				printf("    TUSB_DESC_ENDPOINT [%02X]\r\n", tu_desc_type(pDesc));
//!TODO - MIDI
				break;
			}
			case TUSB_DESC_DEVICE_QUALIFIER                 : {  // 0x06,
				printf("    TUSB_DESC_DEVICE_QUALIFIER [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_OTHER_SPEED_CONFIG               : {  // 0x07,
				printf("    TUSB_DESC_OTHER_SPEED_CONFIG [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_INTERFACE_POWER                  : {  // 0x08,
				printf("    TUSB_DESC_INTERFACE_POWER [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_OTG                              : {  // 0x09,
				printf("    TUSB_DESC_OTG [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_DEBUG                            : {  // 0x0A,
				printf("    TUSB_DESC_DEBUG [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_INTERFACE_ASSOCIATION            : {  // 0x0B,
				printf("    TUSB_DESC_INTERFACE_ASSOCIATION [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_BOS                              : {  // 0x0F,
				printf("    TUSB_DESC_BOS [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_DEVICE_CAPABILITY                : {  // 0x10,
				printf("    TUSB_DESC_DEVICE_CAPABILITY [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_FUNCTIONAL                       : {  // 0x21,
			//case TUSB_DESC_CS_DEVICE                        : {  // 0x21,
				printf("    TUSB_DESC_FUNCTIONAL / TUSB_DESC_CS_DEVICE [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_CS_CONFIGURATION                 : {  // 0x22,
				printf("    TUSB_DESC_CS_CONFIGURATION [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_CS_STRING                        : {  // 0x23,
				printf("    TUSB_DESC_CS_STRING [%02X]\r\n", tu_desc_type(pDesc));
//!TODO 
				break;
			}
			case TUSB_DESC_CS_INTERFACE                     : {  // 0x24,
				printf("    TUSB_DESC_CS_INTERFACE [%02X]\r\n", tu_desc_type(pDesc));
//!TODO - MIDI
				break;
			}
			case TUSB_DESC_CS_ENDPOINT                      : {  // 0x25,
				printf("    TUSB_DESC_CS_ENDPOINT [%02X]\r\n", tu_desc_type(pDesc));
//!TODO - MIDI
				break;
			}
			case TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION    : {  // 0x30,
				printf("    TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			case TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION: {  // 0x31
				printf("    TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION [%02X]\r\n", tu_desc_type(pDesc));
				break;
			}
			default:
				printf("    Unknown Type [%02X]\r\n", tu_desc_type(pDesc));
				break;    
		}
	}







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
