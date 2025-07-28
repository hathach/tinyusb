#include "device/dcd.h"
#include <stdint.h>
#define INT_SOFT 1
#include <stdio.h>

#define DEBUG Debug_UART0
#include "CH58x_common.h"
#include "core_riscv.h"
#include <stdbool.h>

void DebugInit(void)
 {
     GPIOA_SetBits(GPIO_Pin_14);
     GPIOPinRemap(ENABLE, RB_PIN_UART0);
     GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
     GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
     UART0_DefInit();
 }

 __INTERRUPT
 __HIGH_CODE
 void USB_IRQHandler(void)
 {
     dcd_int_handler(0);
 }

// Function to disable or mitigate the IWDG
void IWDG_Disable(void) {
    // Step 1: Reload counter to prevent immediate reset
    R32_IWDG_KR = 0xAAAA; // Reload counter with RLR value

    // Step 2: Lift write protection
    R32_IWDG_KR = 0x5555; // Clear WR_PROTECT (bit 30)

    // Step 3: Wait for PVU (bit 15) to be 0 (no update in progress)
    while (R32_IWDG_CFG & (1 << 15)); // Wait for PVU = 0

    // Step 4: Set STOP_EN to allow stopping in debug mode
    R32_IWDG_CFG |= (1 << 29); // Set STOP_EN (bit 29)

    // Step 5: Set maximum reload value and prescaler to extend timeout
    R32_IWDG_CFG = (R32_IWDG_CFG & ~(0x7 << 12)) | (0x7 << 12); // PR = 111 (divide by 512)
    R32_IWDG_CFG = (R32_IWDG_CFG & ~0xFFF) | 0xFFF; // RLR = 0xFFF (max)

    // Step 6: Reload counter again
    R32_IWDG_KR = 0xAAAA;

    // Optional: Verify IWDG_EN is off (if hardware allows)
    if (R32_IWDG_CFG & (1 << 31)) {
        // Debug: Watchdog still enabled (hardware-enabled?)
        printf("hard dog failure\n");
    }
}


 void disable_watchdog(void) {
    // Reset counter first to prevent overflow during configuration
    R8_WDOG_COUNT = 0x00;
    
    // Disable reset and interrupt, clear interrupt flag
    R8_RST_WDOG_CTRL = (1 << 4); // Set RB_WDOG_INT_FLAG to clear it, bits 1 and 2 to 0
    
    // Verify the register state (optional, for debugging)
    if (R8_RST_WDOG_CTRL & ((1 << 1) | (1 << 2))) {
        // Handle error: Watchdog still enabled
        // Add debug output (e.g., toggle GPIO or UART print)
        printf("dog failure\n");
    }
}


void board_init(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(CLK_SOURCE_HSE_PLL_62_4MHz);
    IWDG_Disable();

    DebugInit();        //配置串口1用来prinft来debug
    R16_PIN_CONFIG |= RB_PIN_USB_EN | RB_UDP_PU_EN; // enable usb pins
    printf("start\n");
    
}



//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) 
{
    (void) state;
    //printf("led write, %d\n", state);
    return;
}

uint32_t board_button_read(void) 
{
    return 0;
}

int board_uart_read(uint8_t* buf, int len) 
{
    (void) buf;
    (void) len;
    return 0;
}

int board_uart_write(void const* buf, int len) 
{
    UART0_SendString((uint8_t*)buf, len);
    return len;
}

  
uint32_t board_millis(void) {
    //DelayMs(10);
    //printf("milis\n");
    return 0;
  }