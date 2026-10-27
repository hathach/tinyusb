// Analysis-only stand-in for the Infineon PDL clock and syslib API dwc2_ifx.h uses (mtb-dsl-pse8xxgp prototypes).
#ifndef PVS_STUB_CY_SYSCLK_H
#define PVS_STUB_CY_SYSCLK_H

#include <stdint.h>

typedef enum {
  CY_SYSCLK_SUCCESS   = 0x00UL,
  CY_SYSCLK_BAD_PARAM = 0x01UL,
} cy_en_sysclk_status_t;

void Cy_SysClk_PeriGroupSlaveInit(uint32_t periNum, uint32_t groupNum, uint32_t slaveNum, uint32_t clkHfNum);
cy_en_sysclk_status_t Cy_SysClk_PeriGroupSetDivider(uint32_t groupNum, uint32_t divider);

// cy_syslib.h, which the PDL's cy_sysclk.h reaches through cy_device.h
void Cy_SysLib_Delay(uint32_t milliseconds);

#endif
