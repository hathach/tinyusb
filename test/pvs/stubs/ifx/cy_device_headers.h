// Analysis-only stand-in for the Infineon PSE84 (CM33, secure) device header: just what dwc2_ifx.h uses.
// Values follow mtb-template-pse8xxgp pse846gps2dbzq3a_s.h and ip/cyip_usbhs.h.
#ifndef PVS_STUB_CY_DEVICE_HEADERS_H
#define PVS_STUB_CY_DEVICE_HEADERS_H

#include <stdint.h>

typedef enum {
  NonMaskableInt_IRQn            = -14,
  HardFault_IRQn                 = -13,
  MemoryManagement_IRQn          = -12,
  BusFault_IRQn                  = -11,
  UsageFault_IRQn                = -10,
  SecureFault_IRQn               = -9,
  SVCall_IRQn                    = -5,
  DebugMonitor_IRQn              = -4,
  PendSV_IRQn                    = -2,
  SysTick_IRQn                   = -1,
  usbhs_interrupt_usbhsctrl_IRQn = 174,
} IRQn_Type;

#define __CM33_REV             0x0001U
#define __NVIC_PRIO_BITS       3
#define __Vendor_SysTickConfig 0
#define __VTOR_PRESENT         1
#define __MPU_PRESENT          1
#define __FPU_PRESENT          1
#define __DSP_PRESENT          1
#define __SAUREGION_PRESENT    1
#define __ICACHE_PRESENT       0
#define __DCACHE_PRESENT       0

#include "core_cm33.h"

typedef struct {
  __IOM uint32_t SUBSYSTEM_CTL;
  __IOM uint32_t PHY_FUNC_CTL_1;
  __IOM uint32_t PHY_FUNC_CTL_2;
  __IM  uint32_t RESERVED[61];
} USBHS_SS_Type;

typedef struct {
  __IM uint32_t DWC_OTG_INTREG[0x40000 / 4];
  USBHS_SS_Type SS;
} USBHS_Type;

#define USBHS_BASE 0x54900000UL

#define USBHS_SS_SUBSYSTEM_CTL_SS_ENABLE_Msk               0x1UL
#define USBHS_SS_SUBSYSTEM_CTL_USB_MODE_Msk                0x2UL
#define USBHS_SS_SUBSYSTEM_CTL_AHB_MASTER_SYNC_Msk         0x8UL
#define USBHS_SS_SUBSYSTEM_CTL_USB_CTRL_SCALEDOWN_MODE_Msk 0x30UL
#define USBHS_SS_PHY_FUNC_CTL_2_RES_TUNING_SEL_Msk         0x800UL
#define USBHS_SS_PHY_FUNC_CTL_2_EFUSE_SEL_Msk              0x1000UL

#endif
