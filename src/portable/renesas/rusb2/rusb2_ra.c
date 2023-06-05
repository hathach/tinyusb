#include "tusb_option.h"
#include "rusb2_ra.h"

#if (CFG_TUSB_RHPORT0_MODE != 0)
IRQn_Type _usb_fs_irqn = USBFS_INT_IRQn;
void tud_set_irq_usbfs(IRQn_Type q) {
  _usb_fs_irqn = q;
}
#endif

#if (CFG_TUSB_RHPORT1_MODE != 0)
IRQn_Type _usb_hs_irqn = USBHS_USB_INT_RESUME_IRQn;
void tud_set_irq_usbhs(IRQn_Type q) {
  _usb_hs_irqn = q;
}
#endif
