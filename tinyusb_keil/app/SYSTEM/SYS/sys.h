////////////////////////////////////////////////////////////////////////////////
/// @file     sys.h
/// @author   AE TEAM
/// @brief    THIS FILE PROVIDES ALL THE SYSTEM FIRMWARE FUNCTIONS.
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
#ifndef __SYS_H
#define __SYS_H

// Files includes

#include "hal_conf.h"
#include  "stdio.h"



////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Example_Layer
/// @brief MM32 Example Layer
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_RESOURCE
/// @brief MM32 Examples resource modules
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Constants
/// @{



#define COREID_MASK             (0x00000070U)


/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Enumeration
/// @{




////////////////////////////////////////////////////////////////////////////////
/// @brief  System clock configuration
////////////////////////////////////////////////////////////////////////////////
typedef enum {
    SYSTEMCLK_HSI_8MHz = 0,
    SYSTEMCLK_HSI_12MHz,
    SYSTEMCLK_HSI_24MHz,
    SYSTEMCLK_HSI_48MHz,
    SYSTEMCLK_HSI_72MHz,
    SYSTEMCLK_HSI_96MHz,
    SYSTEMCLK_HSE_8MHz,
    SYSTEMCLK_HSE_12MHz,
    SYSTEMCLK_HSE_24MHz,
    SYSTEMCLK_HSE_48MHz,
    SYSTEMCLK_HSE_72MHz,
    SYSTEMCLK_HSE_96MHz,
    SYSTEMCLK_HSEDIV2_4MHz,
    SYSTEMCLK_HSEDIV2_8MHz,
    SYSTEMCLK_HSEDIV2_12MHz,
    SYSTEMCLK_HSEDIV2_24MHz,
    SYSTEMCLK_HSEDIV2_48MHz,
    SYSTEMCLK_HSEDIV2_72MHz,
    SYSTEMCLK_HSEDIV2_96MHz,
    SYSTEMCLK_LSI_40KHz,
#if defined(__CUSTOM_FREQ)
    SYSTEMCLK_CUSTOM_Freq,
#endif
} SYSTEMCLK_TypeDef;
#define RCC_LATENCY_TB         0
#define RCC_PLLMUL_TB          1
#define RCC_PLLDIV_TB          2
#define RCC_SYSCLKSRC_TB       3
#define RCC_PLLSRC_TB          4
#define RCC_ROW_TB             5




/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Variables
/// @{
#ifdef _SYS_C_
#define GLOBAL





GLOBAL u32 gSystemClockValue = 8000000UL;
#else
#define GLOBAL extern


GLOBAL u32 gSystemClockValue;



#endif




#undef GLOBAL

/// @}


////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Functions
/// @{

void SystemReInit(SYSTEMCLK_TypeDef  system_clock);


/// @}


/// @}

/// @}


////////////////////////////////////////////////////////////////////////////////
#endif
////////////////////////////////////////////////////////////////////////////////
