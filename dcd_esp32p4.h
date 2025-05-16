#ifndef _DCD_ESP32P4_H_
#define _DCD_ESP32P4_H_

#include <stdint.h>

void dcd_init(uint8_t rhport);
void dcd_int_enable(uint8_t rhport);
void dcd_int_disable(uint8_t rhport);
void dcd_int_handler(uint8_t rhport);
void dcd_connect(uint8_t rhport);
void dcd_disconnect(uint8_t rhport);

#endif
