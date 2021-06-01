////////////////////////////////////////////////////////////////////////////////
/// @file    adcx.h
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
#ifndef __ADCX_H
#define __ADCX_H

// Files includes

#include <string.h>

#include "mm32_device.h"



#include "hal_conf.h"
#include <stdio.h>

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
typedef s32 (*PtrFun)();

////////////////////////////////////////////////////////////////////////////////
/// @brief ADC GPIO enumerate definition.
/// @anchor EM_ADC_PORT
////////////////////////////////////////////////////////////////////////////////

typedef enum {
    emADC1 = 0,
    emADC2 = 1,
} EM_ADC_PORT;


////////////////////////////////////////////////////////////////////////////////
/// @brief ADC enumerate definition.
/// @anchor ADCCHANNEL_TypeDef
////////////////////////////////////////////////////////////////////////////////
typedef enum {
    ADC1ch0                 = (emADC1 << 8) | (ADC_Channel_0),
    ADC1ch1                 = (emADC1 << 8) | (ADC_Channel_1),
    ADC1ch2                 = (emADC1 << 8) | (ADC_Channel_2),
    ADC1ch3                 = (emADC1 << 8) | (ADC_Channel_3),
    ADC1ch4                 = (emADC1 << 8) | (ADC_Channel_4),
    ADC1ch5                 = (emADC1 << 8) | (ADC_Channel_5),
    ADC1ch6                 = (emADC1 << 8) | (ADC_Channel_6),
    ADC1ch7                 = (emADC1 << 8) | (ADC_Channel_7),




} emADCch;



/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Variables
/// @{
#ifdef _ADCX_C_
#define GLOBAL







#else
#define GLOBAL extern







#endif

GLOBAL vu16 ADCVAL;
GLOBAL vu16 ADC_flag;
GLOBAL float fValue;
GLOBAL vu16 ADCValue[20];
GLOBAL float ADCVolatge;
GLOBAL vu16 ADCFilterValue;
GLOBAL vu8 ADCFilterflag;




#undef GLOBAL

/// @}


////////////////////////////////////////////////////////////////////////////////
/// @defgroup MM32_Exported_Functions
/// @{



void ADC1BasicConfigWithParameter(void);
void ADC1ChannelConfigWithParameter(void);
void ADC1PinConfigWithParameter(void);

void ADCConfig(ADCCHANNEL_TypeDef adc_channel);
void EXTIX_Init(void);
u16 Get_Adc_Average(u8 times);
u16 ADC1_SingleChannel_Get(void);
void GPIO_Clock_Set(GPIO_TypeDef* GPIOn, FunctionalState state);
void GPIO_PinConfigAnalog(GPIO_TypeDef* GPIOn, u16 gpio_pin);
void ADCSingleChannelInit(ADCCHANNEL_TypeDef ADC_Channel_x);
void DMAInit(void);
void ADCFilter(void);
void Get_ADCVolatge(void);

/// @}


/// @}

/// @}


////////////////////////////////////////////////////////////////////////////////
#endif
////////////////////////////////////////////////////////////////////////////////
