int  main (void)  { return 0; }

#include <stdbool.h>
#include <stdint.h>
#include "midih.h"
bool doit_usb  (uint8_t dev_addr,  midimsg_t* mp)  { return true; }
bool doit_virt (midih_port_t* to,  midimsg_t* mp)  { return true; }
