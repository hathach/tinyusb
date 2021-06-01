////////////////////////////////////////////////////////////////////////////////
/// @file    exti.h
/// @author  AE TEAM
/// @brief   THIS FILE PROVIDES ALL THE SYSTEM FIRMWARE FUNCTIONS.
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
#ifndef __EXTI_H
#define __EXTI_H

// Files includes
#include <string.h>

#include "mm32_device.h"



#include "hal_conf.h"


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


/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Enumeration
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @brief XXXX enumerate definition.
/// @anchor XXXX
////////////////////////////////////////////////////////////////////////////////



/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Variables
/// @{
#ifdef _EXTI_C_
#define GLOBAL







#else
#define GLOBAL extern





#endif
#define EMINIBOARD


#if defined (MINIBOARD)
#define SW1_GPIO_Port  GPIOC
#define SW1_Pin   GPIO_Pin_13
#define SW2_GPIO_Port  GPIOA
#define SW2_Pin   GPIO_Pin_0
#endif
#if defined (EMINIBOARD)
#define SW1_GPIO_Port  GPIOB
#define SW1_Pin   GPIO_Pin_1
#define SW2_GPIO_Port  GPIOB
#define SW2_Pin   GPIO_Pin_2
#endif
#define SW3_GPIO_Port  GPIOB
#define SW3_Pin   GPIO_Pin_10
#define SW4_GPIO_Port  GPIOB
#define SW4_Pin   GPIO_Pin_0

#define KEY1                    GPIO_ReadInputDataBit(SW1_GPIO_Port,SW1_Pin)    //read key1
#define WK_UP                   GPIO_ReadInputDataBit(SW2_GPIO_Port,SW2_Pin)    //read key2
#define KEY3                    GPIO_ReadInputDataBit(SW3_GPIO_Port,SW3_Pin)    //read key3
#define KEY4                    GPIO_ReadInputDataBit(SW4_GPIO_Port,SW4_Pin)    //read key4

#define KEY1_PRES               1                                               //KEY1
#define WKUP_PRES               2                                               //WK_UP
#define KEY3_PRES               3                                               //KEY3
#define KEY4_PRES               4




#undef GLOBAL

/// @}


////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Functions
/// @{

void KEY_Init(void);
static void EXTI_NVIC_Init(void);
static void EXTI_NVIC_Config(void);
void EXTI_Config(void);

/// @}


/// @}

/// @}


////////////////////////////////////////////////////////////////////////////////
#endif
////////////////////////////////////////////////////////////////////////////////
