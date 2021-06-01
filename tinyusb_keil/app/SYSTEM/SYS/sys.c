////////////////////////////////////////////////////////////////////////////////
/// @file     sys.c
/// @author   AE TEAM
/// @brief    THIS FILE PROVIDES ALL THE SYSTEM FUNCTIONS.
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
#define _SYS_C_

// Files includes
#include <string.h>
#include "mm32_device.h"


#include "sys.h"




////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MM32_Example_Layer
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SYS
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SYS_Exported_Constants
/// @{

/// @}




#define CUSTOM_HSEFREQ                  8000000UL

#define CUSTOM_LATENCY                  0
#define CUSTOM_PLLMUL                   1
#define CUSTOM_PLLDIV                   2
#define CUSTOM_SYSCLKSRC                2
#define CUSTOM_PLLSRC                   0
#define CUSTOM_SYSTICK_FREQ             1000

#define HSI_CHECK_COUNTER               2
#define HSI_TIMEROUT_COUNTER            1000

#define HSE_Checking_Counter            10
#define HSE_TIMEROUT_COUNTER            5000

#define LSI_CHECK_COUNTER               2
#define LSI_TIMEROUT_COUNTER            1000

#define PLL_CHECK_COUNTER               2
#define PLL_TIMEROUT_COUNTER            1000

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MM32_Exported_Functions
/// @{


uc32 cRCC_ClockPrescalerTable[] = {
    // HAPB2                      LAPB1                         AHB/1                       AHB2                  AHB3
    RCC_CFGR_PPRE2_DIV1,       RCC_CFGR_PPRE1_DIV2,          RCC_CFGR_HPRE_DIV1          //  0,                   0,  // HSI_6d_1x,                 //  SYSTEMCLK_HSI_8MHz,
};
ErrorStatus exRCC_ClkPrescaler_Init(u8  system_clock_sel)
{
    // AHB, APB1, APB2
    RCC->CFGR |= (cRCC_ClockPrescalerTable[2]) & RCC_CFGR_HPRE;
    RCC->CFGR |= (cRCC_ClockPrescalerTable[1]) & RCC_CFGR_PPRE1;
    RCC->CFGR |= (cRCC_ClockPrescalerTable[0]) & RCC_CFGR_PPRE2;
    return SUCCESS;
}
static void DELAY_Nop(void)
{
    __IO u32 i;
    i = 256;
    while (i--);
}


ErrorStatus SYSCLK_Init(SYSTEMCLK_TypeDef  system_clock_sel)
{

    u32 check_count = 0;
    u32 timer_out = 0;
//------------------------------------------------------------------------------
    u8 latency_pllmul_plldiv_sysclksrc_pllsrc_table[] = {
        //| latency      | PLLMUL     |    PLLDIV     | SYSCLK          |   PLLSRC
        0,              0,              0,          0,              0,      // HSI_6d_1x,       //  SYSTEMCLK_HSI_8MHz,
        0,              0,              0,          2,              0,      // HSI_4d_1x,       //  SYSTEMCLK_HSI_12MHz,
        0,              1,              0,          2,              0,      // HSI_4d_2x,       //  SYSTEMCLK_HSI_24MHz,
        1,              3,              0,          2,              0,      // HSI_4d_4x,       //  SYSTEMCLK_HSI_48MHz,
        2,              5,              0,          2,              0,      // HSI_4d_6x,       //  SYSTEMCLK_HSI_72MHz,
        3,              7,              0,          2,              0,      // HSI_4d_8x        //  SYSTEMCLK_HSI_96MHz,
        0,              0,              0,          1,              0,      // HSE_1x,          //  SYSTEMCLK_HSE_8MHz,         Based HSE = 8MHz
        0,              2,              1,          2,              1,      // HSE_3_2x,        //  SYSTEMCLK_HSE_12MHz,        Based HSE = 8MHz
        0,              2,              0,          2,              1,      // HSE_3x,          //  SYSTEMCLK_HSE_24MHz,        Based HSE = 8MHz
        1,              5,              0,          2,              1,      // HSE_6x,          //  SYSTEMCLK_HSE_48MHz,        Based HSE = 8MHz
        2,              8,              0,          2,              1,      // HSE_9x,          //  SYSTEMCLK_HSE_72MHz,        Based HSE = 8MHz
        3,              11,             0,          2,              1,      // HSE_12x,         //  SYSTEMCLK_HSE_96MHz,        Based HSE = 8MHz
        0,              0,              0,          2,              2,      // HSEDIV2_1x,      //  SYSTEMCLK_HSEDIV2_4MHz,     Based HSE = 8MHz Div2
        0,              1,              0,          2,              2,      // HSEDIV2_2x,      //  SYSTEMCLK_HSEDIV2_8MHz,     Based HSE = 8MHz Div2
        0,              2,              0,          2,              2,      // HSEDIV2_3x,      //  SYSTEMCLK_HSEDIV2_12MHz,    Based HSE = 8MHz Div2
        0,              5,              0,          2,              2,      // HSEDIV2_6x,      //  SYSTEMCLK_HSEDIV2_24MHz,    Based HSE = 8MHz Div2
        1,              11,             0,          2,              2,      // HSEDIV2_12x,     //  SYSTEMCLK_HSEDIV2_48MHz,    Based HSE = 8MHz Div2
        2,              17,             0,          2,              2,      // HSEDIV2_18x,     //  SYSTEMCLK_HSEDIV2_72MHz,    Based HSE = 8MHz Div2
        3,              23,             0,          2,              2,      // HSEDIV2_24x,     //  SYSTEMCLK_HSEDIV2_96MHz,    Based HSE = 8MHz Div2
        0,              0,              0,          3,              0,      // LSI_1x,          //  SYSTEMCLK_LSI_40KHz,
        //| latency      | PLLMUL     |    PLLDIV     | SYSCLK          |   PLLSRC
        CUSTOM_LATENCY, CUSTOM_PLLMUL, CUSTOM_PLLDIV, CUSTOM_SYSCLKSRC, CUSTOM_PLLSRC  // Custom_Freq,  //  SYSTEMCLK_CUSTOM_Freq,
    };
//SYSCLKSRC   0 -- HSI/6  ,  1 HSE   2 PLL   3 LSI
//PLLSRC      0 -- HSI/4  ,  1 HSE   2 HSE/2


    u8 latency, pllmul, plldiv, sysclksrc, pllsrc;
    if(system_clock_sel > (sizeof(latency_pllmul_plldiv_sysclksrc_pllsrc_table) / 5)) {
        system_clock_sel = SYSTEMCLK_HSI_8MHz;
    }
    //pArray = &latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB];
    latency   = latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB];
    pllmul    = latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB + 1];
    plldiv    = latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB + 2];
    sysclksrc = latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB + 3];
    pllsrc    = latency_pllmul_plldiv_sysclksrc_pllsrc_table[system_clock_sel * RCC_ROW_TB + 4];
    //------------------------------------------------------------------------------

    gSystemClockValue = 8000000UL;                                  // default clock
    RCC->CR |= RCC_CR_HSION;

    check_count = HSI_CHECK_COUNTER;
    timer_out = HSI_TIMEROUT_COUNTER;
    while (1) {
        DELAY_Nop();
        if (timer_out == 0) {
            return ERROR;
        }
        timer_out--;
        if (RCC->CR & RCC_CR_HSIRDY) {
            if (check_count == 0 ) {
                break;
            }
            check_count--;
        }
    }
    // Clock Switch to
    RCC->CFGR |= (0 << RCC_CFGR_SW_Pos) & RCC_CFGR_SW;

    while (((RCC->CFGR & RCC_CFGR_SWS) >> 2) != 0);

    // Flash
    FLASH->ACR |= FLASH_ACR_PRFTBE;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= latency;
    exRCC_ClkPrescaler_Init(0);


    if  ( (sysclksrc == 1) || (pllsrc == 1) || (pllsrc == 2) ) {
        //use HSE
        RCC->CR |= RCC_CR_HSEON;
        check_count = HSE_Checking_Counter;
        timer_out = HSE_TIMEROUT_COUNTER;
        while (1) {
            DELAY_Nop();
            if (timer_out == 0) {
                return ERROR;
            }
            timer_out--;
            if (RCC->CR & RCC_CR_HSERDY) {
                if (check_count == 0 ) {
                    break;
                }
                check_count--;
            }
        }
        gSystemClockValue = CUSTOM_HSEFREQ;
    }

    if   (sysclksrc == 3)  {
        RCC->CSR |= RCC_CSR_LSION;

        check_count = LSI_CHECK_COUNTER;
        timer_out = LSI_TIMEROUT_COUNTER;
        while (1) {
            DELAY_Nop();
            if (timer_out == 0) {
                return ERROR;
            }
            timer_out--;
            if (RCC->CSR & RCC_CSR_LSIRDY) {
                if (check_count == 0 ) {
                    break;
                }
                check_count--;
            }
        }
        gSystemClockValue = 40000;
    }

    if(sysclksrc == 2) {

//------------------------------------------------------------------------------
        if (((SCB->CPUID & COREID_MASK) == 0) && ((u32) * ((u32*)(0x40013400))  == (0xCC4460B1U)) ) {
            while(1);//MCU is M0 q version, please check Core #define and Target MCU
        }
        // Set PLL CLK source
        if (pllsrc == 1) {
            RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC;
            gSystemClockValue = CUSTOM_HSEFREQ;
        }
        else if (pllsrc == 2) {
            RCC->PLLCFGR |= RCC_PLLCFGR_PLLXTPRE;
            RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC;
            gSystemClockValue = CUSTOM_HSEFREQ / 2;
        }
        else {
            RCC->CFGR &= ~RCC_PLLCFGR_PLLSRC;
            gSystemClockValue = 8000000UL;
        }
        // Set PLL MUL and DIV
        RCC->PLLCFGR |= (((u32)pllmul) << RCC_PLLCFGR_PLL_DN_Pos) & RCC_PLLCFGR_PLL_DN;   //
        RCC->PLLCFGR |= (((u32)plldiv) << RCC_PLLCFGR_PLL_DP_Pos) & RCC_PLLCFGR_PLL_DP;   //
        RCC->CR |= RCC_CR_PLLON;
        gSystemClockValue = ( gSystemClockValue / ((u32)(((u32)plldiv) + 1)) ) * ((u32)(((u32)pllmul) + 1));
        // Set PLL ON and wait PLL ready

        check_count = PLL_CHECK_COUNTER;
        timer_out = PLL_TIMEROUT_COUNTER;
        while (1) {
            DELAY_Nop();
            if (timer_out == 0) {
                return ERROR;
            }
            timer_out--;
            if (RCC->CR & RCC_CR_PLLRDY) {
                if (check_count == 0 ) {
                    break;
                }
                check_count--;
            }
        }

    }
    // Clock Switch to
    RCC->CFGR |= (((u32)sysclksrc) << RCC_CFGR_SW_Pos) & RCC_CFGR_SW;
    while (((RCC->CFGR & RCC_CFGR_SWS) >> 2) != ((u32)sysclksrc & RCC_CFGR_SW));

    return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief  Resets the RCC clock configuration to default state.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void RCC_SetDefault(void)
{
    SET_BIT(RCC->CR, RCC_CR_HSION);
    CLEAR_BIT(RCC->CFGR, RCC_CFGR_SW);


    CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON );
    CLEAR_BIT(RCC->PLLCFGR, RCC_PLLCFGR_PLL_DN | RCC_PLLCFGR_PLL_DP);//0x0018031C);//
    CLEAR_BIT(RCC->CR, RCC_CR_HSEBYP);
    CLEAR_REG(RCC->CFGR);
    CLEAR_REG(RCC->CIR);
}
////////////////////////////////////////////////////////////////////////////////
///  @brief  Setup the microcontroller system
///          Initialize the Embedded Flash Interface, the PLL and update the
///  @param  None.
///  @retval None.
////////////////////////////////////////////////////////////////////////////////

void SystemReInit(SYSTEMCLK_TypeDef  system_clock)
{
    RCC_SetDefault();
    SYSCLK_Init(system_clock);
}


/// @}


/// @}

/// @}
