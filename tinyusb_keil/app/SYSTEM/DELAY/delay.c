////////////////////////////////////////////////////////////////////////////////
/// @file    delay.c
/// @author  AE TEAM
/// @brief   THIS FILE PROVIDES ALL THE SYSTEM FUNCTIONS.
////////////////////////////////////////////////////////////////////////////////
/// @attention
///
/// THE EXISTING FIRMWARE IS ONLY FOR REFERENCE, WHICH IS DESIGNED TO PROVIDE
/// CUSTOMERS WITH CODING INFORMATION ABOUT THEIR PRODUCTS SO THEY CAN SAVE
/// TIME. THEREFORE, MINDMOTION SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT OR
/// CONSEQUENTIAL DAMAGES ABOUT ANY CLAIMS ARISING OUT OF THE CONTENT OF SUCH
/// HARDWARE AND/OR THE USE OF THE CODING INFORMATION CONTAINED HEREIN IN
/// CONNECTION WITH PRODUCTS MADE BY CUSTOMERS.
///
/// <H2><CENTER>&COPY; COPYRIGHT MINDMOTION </CENTER></H2>
////////////////////////////////////////////////////////////////////////////////

// Define to prevent recursive inclusion
#define _DELAY_C_

// Files includes
#include "delay.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MM32_Hardware_Abstract_Layer
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup DELAY
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup DELAY_Exported_Functions
/// @{



#define USE_SYSTICK_DELAY 1
//1 = use systick as the delay,
//0 = use NOP loop as the delay

//if use systick for delay, need to call function DELAY_Init(u8 pclk2) firstly

#if USE_SYSTICK_DELAY
extern u32 SystemCoreClock;
static __IO u32 sTimingDelay;

////////////////////////////////////////////////////////////////////////////////
/// @brief  Initialize systick for delay function
/// @note   This function should affected by chip version /8 or /1.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void DELAY_Init(void)
{
    if (SysTick_Config(SystemCoreClock / 1000)) {
        while (1);
    }
    NVIC_SetPriority(SysTick_IRQn, 0x0);
}
////////////////////////////////////////////////////////////////////////////////
/// @brief  decrease per 1ms until counter value is zero
/// @note   None.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
static void TimingDelayDecrement(void)
{
    if (sTimingDelay != 0x00) {
        sTimingDelay--;
    }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  SysTick_Handler is call from interrupt map
/// @note   Call delay count function.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void SysTick_Handler(void)
{
    TimingDelayDecrement();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Set the count as delay counter, it is break until counter goes to 0.
/// @note   use sTimingDelay.
/// @param  count specifies the delay tick number
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void DELAY_Ms(__IO u32 count)
{
    sTimingDelay = count;

    while(sTimingDelay != 0);
}
#else
static __IO u32 sDelayNopNumber;
////////////////////////////////////////////////////////////////////////////////
/// @brief  Initialize for delay function
/// @note   None.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void DELAY_Init(void)
{
    //Custom
    sDelayNopNumber = 100;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief  Set the count as delay counter, it is break until counter goes to 0.
/// @note   None.
/// @param  count specifies the delay tick number
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void DELAY_Us(__IO u32 count)
{
    u32 i = 0;
    while(count--) {
        i = sDelayNopNumber;
        while(i--);
    }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief  Set the count as delay counter, it is break until counter goes to 0.
/// @note   None.
/// @param  count specifies the delay tick number
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void DELAY_Ms(__IO u32 count)
{
    u32 i = 0;
    while(count--) {
        i = sDelayNopNumber * 1000;
        while(i--);
    }
}
#endif


/// @}

/// @}

/// @}
