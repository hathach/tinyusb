/* SPDX-License-Identifier: MIT */
#ifndef DWC2_TEST_H_
#define DWC2_TEST_H_

#include "host/hcd.h"
#include "host/usbh.h"
#include "portable/synopsys/dwc2/dwc2_type.h"

// Supply the controller port with RAM registers; no vendor SDK or physical MMIO.
#define TUSB_DWC2_COMMON_H
static dwc2_regs_t test_regs;
static const dwc2_controller_t _dwc2_controller[] = {{.reg_base = (uintptr_t) &test_regs}};
#define DWC2_REG(rhport) ((void) (rhport), &test_regs)

void dwc2_clock_init(uint8_t rhport, tusb_role_t role);
void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled);
bool dwc2_core_is_highspeed_phy(dwc2_regs_t* dwc2, bool prefer_hs_phy);
bool dwc2_core_init(uint8_t rhport, bool is_hs_phy, bool is_dma);
void dwc2_core_deinit(uint8_t rhport);
void dfifo_flush_tx(dwc2_regs_t* dwc2, uint8_t fnum);
void dfifo_flush_rx(dwc2_regs_t* dwc2);
bool tuh_bus_info_get(uint8_t daddr, tuh_bus_info_t* bus_info);
bool hub_tt_multi(uint8_t hub_addr);
bool hub_iso_split_slot(uint8_t used, uint16_t packet_size, uint8_t dir, uint8_t* slot);
bool hcd_dcache_clean(const void* addr, uint32_t data_size);
bool hcd_dcache_invalidate(const void* addr, uint32_t data_size);
void hcd_event_handler(hcd_event_t const* event, bool in_isr);
void tu_hwfifo_read(const volatile void* fifo, uint8_t* dest, uint16_t len,
                    const tu_hwfifo_access_t* access_mode);

#endif
