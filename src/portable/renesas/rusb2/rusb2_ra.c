#include "tusb_option.h"
#include "device/dcd.h"
#include "rusb2_type.h"
#include "rusb2_ra.h"

rusb2_controller_t rusb2_controller[] = {
    { .reg_base = R_USB_FS0_BASE, .irqnum = USBFS_INT_IRQn },
    #ifdef RUSB2_SUPPORT_HIGHSPEED
    { .reg_base = R_USB_HS0_BASE, .irqnum = USBHS_USB_INT_RESUME_IRQn },
    #endif
};

// Application API for setting IRQ number
void tud_rusb2_set_irqnum(uint8_t rhport, int32_t irqnum) {
    rusb2_controller[rhport].irqnum = irqnum;
}