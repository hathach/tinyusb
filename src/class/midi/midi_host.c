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

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_MIDI)

//----------------------------------------------------------------------------- ----------------------------------------
// INCLUDE
//
#include "host/usbh.h"
#include "host/usbh_classdriver.h"
#include "midi_host.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// temp split of C code - will not trigger a recompile
#include "decode.c"
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define LOGF(...) printf(__VA_ARGS__)

//----------------------------------------------------------------------------- ----------------------------------------
// INTERNAL OBJECT & FUNCTION DECLARATION
//
typedef
	struct {
		int i;
	}
midi_dev_t;

static midi_dev_t  midi_dev[CFG_TUH_DEVICE_MAX];

//+============================================================================ ========================================
// USBH-CLASS API [1/5] : .init
//
void  midih_init (void)
{
	LOGF("# midi_init()\r\n");

	tu_memclr(midi_dev, sizeof(midi_dev));
	dec_logfSet(printf);
}

//+============================================================================ ========================================
// USBH-CLASS API [2/5] : .open
//
uint8_t buf[256];

static bool strHandler(uint8_t dev_addr,  tusb_control_request_t const* request,  xfer_result_t result)
{
	dec_hexdump("<String", 64, buf);
	return true;
}

bool  midih_open (uint8_t rhport,  uint8_t dev_addr,  tusb_desc_interface_t const *desc_itf,  uint16_t max_len)
{

	const uint8_t*  pDesc = NULL;
	const uint8_t*  pEnd  = NULL;

	LOGF("# midi_open(port=%d, dev=%d, *desc=%p, max=%d)\r\n", rhport, dev_addr, desc_itf, max_len);

	dec_descriptor(desc_itf);


	if (desc_itf->bInterfaceClass != TUSB_CLASS_AUDIO) {
		LOGF("# Not an AUDIO interface -> not a MIDI interface. Rejecting...\r\n"        );
//		return false;  //!  don't return false yet .. still gonna run everything past the decoder
	} else {
		LOGF("# Found AUDIO interface @%u:%u...\r\n", desc_itf->bInterfaceNumber, dev_addr);
		LOGF("# Subclass : ");
		switch (desc_itf->bInterfaceSubClass) {  // the stack doesnt enum audio class interface subclasses
			case 0x01 : LOGF("AUDIOCONTROL -> not a MIDI interface. Rejecting...\r\n"    );  break ;//return false;
			case 0x02 : LOGF("AUDIOSTREAMING -> not a MIDI interface. Rejecting...\r\n"  );  break ;//return false;
			case 0x03 : LOGF("MIDISTREAMING -> Found a MIDI interface!!\r\n"             );  break ;
			default   : LOGF("**ERROR** Unknown audio sub-type\r\n"                      );  break ;//return false;
		}
	}

	LOGF("\r\nvvv=================================================================vvv\r\n");
	for (pDesc = (void*)desc_itf, pEnd = pDesc + max_len;  pDesc < pEnd;  pDesc = tu_desc_next(pDesc)) {
		dec_descriptor(pDesc);
	}//for
	LOGF("^^^=====================================================================^^^\r\n\r\n");

// 0x80 (10000000B - Request type)
// 0x06 (Get_Descriptor request)
// 0x0301 (high byte = STRING descriptor(3), low byte = index(1))
// 0x0409 (language - UNICODE for english-us)
// 0x0012 (length)	
printf("#########################################################################\r\n");
	uint8_t request[] = {0x80, 0x06, 0x01, 0x03, 0x09, 0x04, 0x12, 0x00};
	TU_ASSERT( tuh_control_xfer(0, (tusb_control_request_t*)request, buf, strHandler) );
printf("#########################################################################\r\n");
	
	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [3/5] : .set_config
//
bool  midih_set_config (uint8_t dev_addr,  uint8_t itf_num)
{
	uint16_t  vid, pid;

	tuh_vid_pid_get(dev_addr, &vid, &pid);

	LOGF("# midi_set_config(dev=%d, num=%d) // VID:PID=%04X:%04X\r\n", dev_addr, itf_num, vid, pid);

	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [4/5] : .xfer_cb
//
bool  midih_xfer_cb (uint8_t dev_addr,  uint8_t ep_addr,  xfer_result_t event,  uint32_t xferred_bytes)
{
	LOGF("# midi_xfer_cb(dev=%d, ep=%d, event=%d, bytes=%d)\r\n", dev_addr, ep_addr, event, xferred_bytes);

	return true;
}

//+============================================================================ ========================================
// USBH-CLASS API [5/5] : .close
//
void  midih_close (uint8_t dev_addr)
{
	LOGF("# midi_close(dev=%d)\r\n\r\n", dev_addr);

	TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX, );

//	tu_memclr((midi_data_t*)get_itf(dev_addr), sizeof(midi_data_t));

	return;
}

#endif
