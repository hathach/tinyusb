/**
 *  Framework specific file...
*/
#include "tusb_option.h"

#if CFG_TUD_ENABLED && (CFG_TUSB_MCU == OPT_MCU_CH32V307)
#include "device/dcd.h"
#include "bsp/board.h"
#include "ch32v30x.h"
#include "core_riscv.h"

void dcd_int_enable(uint8_t rhport) {
    (void)rhport;
    NVIC_EnableIRQ(OTG_FS_IRQn);
}
/**
*/
void dcd_int_disable(uint8_t rhport) {
    (void)rhport;
    NVIC_DisableIRQ(OTG_FS_IRQn);
}
void dcd_fs_platform_init(void)
{
    // put your clock etc initialization code here if not done already
}
void dcd_fs_platform_delay(int ms)
{
    board_delay(ms);
}

#endif
// EOF
