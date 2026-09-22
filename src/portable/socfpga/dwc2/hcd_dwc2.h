
#ifndef __HCD_DWC2_H__
#define __HCD_DWC2_H__

#include <stdint.h>
#include <stdbool.h>
#include <common/tusb_types.h>

bool hcd_dwc2_init(uint8_t rhport, const tusb_rhport_init_t* rh_init);
void hcd_dwc2_int_enable (uint8_t rhport);
void hcd_dwc2_int_disable(uint8_t rhport);

uint32_t hcd_dwc2_frame_number(uint8_t rhport);
bool hcd_dwc2_port_connect_status(uint8_t rhport);
void hcd_dwc2_port_reset(uint8_t rhport);

void hcd_dwc2_port_reset_end(uint8_t rhport);
tusb_speed_t hcd_dwc2_port_speed_get(uint8_t rhport);
void hcd_dwc2_device_close(uint8_t rhport, uint8_t dev_addr);

bool hcd_dwc2_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t* desc_ep);
bool hcd_dwc2_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint32_t buflen);
bool hcd_dwc2_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr);
bool hcd_dwc2_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]);
bool hcd_dwc2_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr);

void hcd_dwc2_int_handler(uint8_t rhport, bool in_isr);

#endif
