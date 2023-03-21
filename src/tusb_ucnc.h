/*
	Name: tusb_ucnc.h
	Description: TinyUSB CDC for µCNC.

	Copyright: Copyright (c) João Martins
	Author: João Martins
	Date: 21-03-2023

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/


#ifndef _TUSB_UCNC_H_
#define _TUSB_UCNC_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "tusb_config.h"
#include "tusb.h"

#define tusb_cdc_init tusb_init
#define tusb_cdc_isr_handler() tud_int_handler(0)
#define tusb_cdc_task tud_task
#define tusb_cdc_available() tud_cdc_n_available(0)
#define tusb_cdc_read() tud_cdc_n_read_char(0)
#define tusb_cdc_flush() tud_cdc_n_write_flush(0)
#define tusb_cdc_write(ch) tud_cdc_n_write_char(0, ch)
#define tusb_cdc_write_available() tud_cdc_n_write_available(0);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_UCNC_H_ */