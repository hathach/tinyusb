/********************************** (C) COPYRIGHT *******************************
* File Name          : system_ch32f20x.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/08/08
* Description        : CH32F20x Device Peripheral Access Layer System Source File.
*                      For CH32F208 HSE = 32Mhz
*                      For others   HSE = 8Mhz
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32f20x.h"

/*
* Uncomment the line corresponding to the desired System clock (SYSCLK) frequency (after
* reset the HSI is used as SYSCLK source).
* If none of the define below is enabled, the HSI is used as System clock source.
*/

//#define SYSCLK_FREQ_HSE    HSE_VALUE
//#define SYSCLK_FREQ_48MHz_HSE  48000000
//#define SYSCLK_FREQ_56MHz_HSE  56000000
//#define SYSCLK_FREQ_72MHz_HSE  72000000
#define SYSCLK_FREQ_96MHz_HSE  96000000
//#define SYSCLK_FREQ_120MHz_HSE  120000000
//#define SYSCLK_FREQ_144MHz_HSE  144000000
//#define SYSCLK_FREQ_HSI    HSI_VALUE
//#define SYSCLK_FREQ_48MHz_HSI  48000000
//#define SYSCLK_FREQ_56MHz_HSI  56000000
//#define SYSCLK_FREQ_72MHz_HSI  72000000
//#define SYSCLK_FREQ_96MHz_HSI  96000000
//#define SYSCLK_FREQ_120MHz_HSI  120000000
//#define SYSCLK_FREQ_144MHz_HSI  144000000


/* Uncomment the following line if you need to relocate your vector Table in Internal SRAM */
/* #define VECT_TAB_SRAM */

/* Vector Table base offset field This value must be a multiple of 0x200 */
#define VECT_TAB_OFFSET  0x0

/* Clock Definitions */
#ifdef SYSCLK_FREQ_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_HSE;              /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_48MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_48MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_56MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_56MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_72MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_72MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_96MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_96MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_120MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_120MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_144MHz_HSE
uint32_t SystemCoreClock         = SYSCLK_FREQ_144MHz_HSE;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_48MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_48MHz_HSI;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_56MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_56MHz_HSI;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_72MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_72MHz_HSI;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_96MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_96MHz_HSI;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_120MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_120MHz_HSI;        /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_144MHz_HSI
uint32_t SystemCoreClock         = SYSCLK_FREQ_144MHz_HSI;        /* System Clock Frequency (Core Clock) */
#else
uint32_t SystemCoreClock         = HSI_VALUE;                    /* System Clock Frequency (Core Clock) */

#endif

__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

/* system_private_function_proto_types */
static void SetSysClock( void );

#ifdef SYSCLK_FREQ_HSE
static void SetSysClockToHSE( void );
#elif defined SYSCLK_FREQ_48MHz_HSE
static void SetSysClockTo48_HSE( void );
#elif defined SYSCLK_FREQ_56MHz_HSE
static void SetSysClockTo56_HSE( void );
#elif defined SYSCLK_FREQ_72MHz_HSE
static void SetSysClockTo72_HSE( void );
#elif defined SYSCLK_FREQ_96MHz_HSE
static void SetSysClockTo96_HSE( void );
#elif defined SYSCLK_FREQ_120MHz_HSE
static void SetSysClockTo120_HSE( void );
#elif defined SYSCLK_FREQ_144MHz_HSE
static void SetSysClockTo144_HSE( void );
#elif defined SYSCLK_FREQ_48MHz_HSI
static void SetSysClockTo48_HSI( void );
#elif defined SYSCLK_FREQ_56MHz_HSI
static void SetSysClockTo56_HSI( void );
#elif defined SYSCLK_FREQ_72MHz_HSI
static void SetSysClockTo72_HSI( void );
#elif defined SYSCLK_FREQ_96MHz_HSI
static void SetSysClockTo96_HSI( void );
#elif defined SYSCLK_FREQ_120MHz_HSI
static void SetSysClockTo120_HSI( void );
#elif defined SYSCLK_FREQ_144MHz_HSI
static void SetSysClockTo144_HSI( void );

#endif


/*********************************************************************
 * @fn      SystemInit
 *
 * @brief   Setup the microcontroller system Initialize the Embedded Flash Interface,
 *        the PLL and update the SystemCoreClock variable.
 *
 * @return  none
 */
void SystemInit( void )
{
    RCC->CTLR |= ( uint32_t )0x00000001;

#ifdef CH32F20x_D8C
    RCC->CFGR0 &= ( uint32_t )0xF8FF0000;
#else
    RCC->CFGR0 &= ( uint32_t )0xF0FF0000;
#endif

    RCC->CTLR &= ( uint32_t )0xFEF6FFFF;
    RCC->CTLR &= ( uint32_t )0xFFFBFFFF;
    RCC->CFGR0 &= ( uint32_t )0xFF80FFFF;
#ifdef CH32F20x_D8C
    RCC->CTLR &= ( uint32_t )0xEBFFFFFF;
    RCC->INTR = 0x00FF0000;
    RCC->CFGR2 = 0x00000000;
#else
    RCC->INTR = 0x009F0000;
#endif

    SetSysClock();

#ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM. */
#else
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH. */
#endif
}

/*********************************************************************
 * @fn      SystemCoreClockUpdate
 *
 * @brief   Update SystemCoreClock variable according to Clock Register Values.
 *
 * @return  none
 */
void SystemCoreClockUpdate( void )
{
    uint32_t tmp = 0, pllmull = 0, pllsource = 0;
	  uint8_t Pll_6_5 = 0;

#if defined (CH32F20x_D8C)
   	uint8_t Pll2mull = 0;

#endif

    tmp = RCC->CFGR0 & RCC_SWS;

    switch( tmp )
    {
        case 0x00:
            SystemCoreClock = HSI_VALUE;
            break;
        case 0x04:
            SystemCoreClock = HSE_VALUE;
            break;
        case 0x08:
            pllmull = RCC->CFGR0 & RCC_PLLMULL;
            pllsource = RCC->CFGR0 & RCC_PLLSRC;
            pllmull = ( pllmull >> 18 ) + 2;

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
            if( pllmull == 17 )
            {
                pllmull = 18;
            }
#else
            if( pllmull == 2 )
            {
                pllmull = 18;
            }
            if( pllmull == 15 )
            {
                pllmull = 13;  /* *6.5 */
                Pll_6_5 = 1;
            }
            if( pllmull == 16 )
            {
                pllmull = 15;
            }
            if( pllmull == 17 )
            {
                pllmull = 16;
            }
#endif

            if( pllsource == 0x00 )
            {
								if(EXTEN->EXTEN_CTR & EXTEN_PLL_HSI_PRE) SystemCoreClock = HSI_VALUE * pllmull;
								else SystemCoreClock = (HSI_VALUE >> 1) * pllmull;
            }
            else
            {
#if defined (CH32F20x_D8C)
	          if(RCC->CFGR2 & (1<<16)){ /* PLL2 */
              SystemCoreClock = HSE_VALUE/(((RCC->CFGR2 & 0xF0)>>4) + 1);  /* PREDIV2 */

              Pll2mull = (uint8_t)((RCC->CFGR2 & 0xF00)>>8);

              if(Pll2mull == 0) SystemCoreClock = (SystemCoreClock * 5)>>1;
              else if(Pll2mull == 1) SystemCoreClock = (SystemCoreClock * 25)>>1;
              else if(Pll2mull == 15) SystemCoreClock = SystemCoreClock * 20;
              else  SystemCoreClock = SystemCoreClock * (Pll2mull + 2);

              SystemCoreClock = SystemCoreClock/((RCC->CFGR2 & 0xF) + 1);  /* PREDIV1 */
          }
          else{/* HSE */
              SystemCoreClock = HSE_VALUE/((RCC->CFGR2 & 0xF) + 1);  /* PREDIV1 */
          }

          SystemCoreClock = SystemCoreClock * pllmull;
#else

#if defined (CH32F20x_D8W)
								if((RCC->CFGR0 & (3<<22)) == (3<<22))
								{
									SystemCoreClock = ((HSE_VALUE>>1)) * pllmull;
								}
								else
#endif
                if( ( RCC->CFGR0 & RCC_PLLXTPRE ) != ( uint32_t )RESET )
                {
#ifdef CH32F20x_D8W
                    SystemCoreClock = ( ( HSE_VALUE >> 2 ) >> 1 ) * pllmull;
#else
                    SystemCoreClock = ( HSE_VALUE >> 1 ) * pllmull;
#endif
                }
                else
                {
#ifdef CH32F20x_D8W
                    SystemCoreClock = ( HSE_VALUE >> 2 ) * pllmull;
#else
                    SystemCoreClock = HSE_VALUE * pllmull;
#endif

                }
#endif
            }

            if( Pll_6_5 == 1 ) SystemCoreClock = ( SystemCoreClock / 2 );

            break;
        default:
            SystemCoreClock = HSI_VALUE;
            break;
    }

    tmp = AHBPrescTable[( ( RCC->CFGR0 & RCC_HPRE ) >> 4 )];
    SystemCoreClock >>= tmp;
}



/*********************************************************************
 * @fn      SetSysClock
 *
 * @brief   Configures the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClock( void )
{
#ifdef SYSCLK_FREQ_HSE
    SetSysClockToHSE();
#elif defined SYSCLK_FREQ_48MHz_HSE
    SetSysClockTo48_HSE();
#elif defined SYSCLK_FREQ_56MHz_HSE
    SetSysClockTo56_HSE();
#elif defined SYSCLK_FREQ_72MHz_HSE
    SetSysClockTo72_HSE();
#elif defined SYSCLK_FREQ_96MHz_HSE
    SetSysClockTo96_HSE();
#elif defined SYSCLK_FREQ_120MHz_HSE
    SetSysClockTo120_HSE();
#elif defined SYSCLK_FREQ_144MHz_HSE
    SetSysClockTo144_HSE();
#elif defined SYSCLK_FREQ_48MHz_HSI
    SetSysClockTo48_HSI();
#elif defined SYSCLK_FREQ_56MHz_HSI
    SetSysClockTo56_HSI();
#elif defined SYSCLK_FREQ_72MHz_HSI
    SetSysClockTo72_HSI();
#elif defined SYSCLK_FREQ_96MHz_HSI
    SetSysClockTo96_HSI();
#elif defined SYSCLK_FREQ_120MHz_HSI
    SetSysClockTo120_HSI();
#elif defined SYSCLK_FREQ_144MHz_HSI
    SetSysClockTo144_HSI();

#endif

    /* If none of the define above is enabled, the HSI is used as System clock
     * source (default after reset)
    */
}


#ifdef SYSCLK_FREQ_HSE

/*********************************************************************
 * @fn      SetSysClockToHSE
 *
 * @brief   Sets HSE as System clock source and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockToHSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;


    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV1;

         /* Select HSE as system clock source
          *   CH32F20x_D6 (HSE=8Mhz)
          *   CH32F20x_D8 (HSE=8Mhz)
          *   CH32F20x_D8W (HSE=32Mhz)
          */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_HSE;

        /* Wait till HSE is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x04 )
        {
        }
    }
    else
    {
        /* If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_48MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo48_HSE
 *
 * @brief   Sets System clock frequency to 48MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo48_HSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;


    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );
    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

        /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 6 = 48 MHz (HSE=8Mhz)
         *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 6 = 48 MHz (HSE=8Mhz)
         *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/4 * 6 = 48 MHz(HSE=32Mhz)
         */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL6 );
#else
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL6_EXTEN );
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_56MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo56_HSE
 *
 * @brief   Sets System clock frequency to 56MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo56_HSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

        /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 7 = 56 MHz (HSE=8Mhz)
         *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 7 = 56 MHz (HSE=8Mhz)
         *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/4 * 7 = 56 MHz(HSE=32Mhz)
         */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL7 );
#else
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL7_EXTEN );
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
        {
        }

        /* Select PLL as system clock source */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_72MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo72_HSE
 *
 * @brief   Sets System clock frequency to 72MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo72_HSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

         /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 9 = 72 MHz (HSE=8Mhz)
          *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 9 = 72 MHz (HSE=8Mhz)
          *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/4 * 9 = 72 MHz(HSE=32Mhz)
          */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE |
                                    RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL9 );
#else
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL9_EXTEN );
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}


#elif defined SYSCLK_FREQ_96MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo96_HSE
 *
 * @brief   Sets System clock frequency to 96MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo96_HSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

        /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 12 = 96 MHz (HSE=8Mhz)
         *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 12 = 96 MHz (HSE=8Mhz)
         *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/4 * 12 = 96 MHz(HSE=32Mhz)
         */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE |
                                    RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL12 );
#else
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL12_EXTEN );
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}


#elif defined SYSCLK_FREQ_120MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo120_HSE
 *
 * @brief   Sets System clock frequency to 120MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo120_HSE(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

  RCC->CTLR |= ((uint32_t)RCC_HSEON);

  /* Wait till HSE is ready and if Time out is reached exit */
  do
  {
    HSEStatus = RCC->CTLR & RCC_HSERDY;
    StartUpCounter++;
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CTLR & RCC_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }

  if (HSEStatus == (uint32_t)0x01)
  {
#if defined (CH32F20x_D8W)
		RCC->CFGR0 |= (uint32_t)(3<<22);
    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV2;
#else
    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
#endif

    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

     /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 15 = 120 MHz (HSE=8Mhz)
      *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 15 = 120 MHz (HSE=8Mhz)
      *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/2 * 15 = 240 MHz(HSE=32Mhz)
      */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                        RCC_PLLMULL));

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL15);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL15_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  {
        /*
         * If HSE fails to start-up, the application will have wrong clock
     * configuration. User can add here some code to deal with this error
         */
  }
}



#elif defined SYSCLK_FREQ_144MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo144_HSE
 *
 * @brief   Sets System clock frequency to 144MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo144_HSE( void )
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ( ( uint32_t )RCC_HSEON );

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    }
    while( ( HSEStatus == 0 ) && ( StartUpCounter != HSE_STARTUP_TIMEOUT ) );

    if( ( RCC->CTLR & RCC_HSERDY ) != RESET )
    {
        HSEStatus = ( uint32_t )0x01;
    }
    else
    {
        HSEStatus = ( uint32_t )0x00;
    }

    if( HSEStatus == ( uint32_t )0x01 )
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

        /*  CH32F20x_D6-PLL configuration: PLLCLK = HSE * 18 = 144 MHz (HSE=8Mhz)
         *  CH32F20x_D8-PLL configuration: PLLCLK = HSE * 18 = 144 MHz (HSE=8Mhz)
         *  CH32F20x_D8W-PLL configuration: PLLCLK = HSE/4 * 18 = 144 MHz(HSE=32Mhz)
         */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE |
                                    RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL18 );
#else
        RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL18_EXTEN );
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
        RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
        * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_48MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo48_HSI
 *
 * @brief   Sets System clock frequency to 48MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo48_HSI( void )
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

		/* HCLK = SYSCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
		/* PCLK2 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
		/* PCLK1 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

		/*  PLL configuration: PLLCLK = HSI * 6 = 48 MHz */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL6 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL6_EXTEN );
#endif

		/* Enable PLL */
		RCC->CTLR |= RCC_PLLON;
		/* Wait till PLL is ready */
		while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
		{
		}
		/* Select PLL as system clock source */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
		RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
		/* Wait till PLL is used as system clock source */
		while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
		{
		}
}

#elif defined SYSCLK_FREQ_56MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo56_HSI
 *
 * @brief   Sets System clock frequency to 56MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo56_HSI( void )
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

		/* HCLK = SYSCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
		/* PCLK2 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
		/* PCLK1 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

		/*  PLL configuration: PLLCLK = HSI * 7 = 56 MHz */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL7 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL7_EXTEN );
#endif

		/* Enable PLL */
		RCC->CTLR |= RCC_PLLON;
		/* Wait till PLL is ready */
		while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
		{
		}
		/* Select PLL as system clock source */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
		RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
		/* Wait till PLL is used as system clock source */
		while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
		{
		}
}

#elif defined SYSCLK_FREQ_72MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo72_HSI
 *
 * @brief   Sets System clock frequency to 72MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo72_HSI( void )
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

		/* HCLK = SYSCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
		/* PCLK2 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
		/* PCLK1 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

		/*  PLL configuration: PLLCLK = HSI * 9 = 72 MHz */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL9 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL9_EXTEN );
#endif

		/* Enable PLL */
		RCC->CTLR |= RCC_PLLON;
		/* Wait till PLL is ready */
		while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
		{
		}
		/* Select PLL as system clock source */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
		RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
		/* Wait till PLL is used as system clock source */
		while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
		{
		}
}


#elif defined SYSCLK_FREQ_96MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo96_HSI
 *
 * @brief   Sets System clock frequency to 96MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo96_HSI( void )
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

		/* HCLK = SYSCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
		/* PCLK2 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
		/* PCLK1 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

		/*  PLL configuration: PLLCLK = HSI * 12 = 96 MHz */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL12 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL12_EXTEN );
#endif

		/* Enable PLL */
		RCC->CTLR |= RCC_PLLON;
		/* Wait till PLL is ready */
		while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
		{
		}
		/* Select PLL as system clock source */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
		RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
		/* Wait till PLL is used as system clock source */
		while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
		{
		}
}


#elif defined SYSCLK_FREQ_120MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo120_HSI
 *
 * @brief   Sets System clock frequency to 120MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo120_HSI(void)
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;

    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 15 = 120 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                        RCC_PLLMULL));

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL15 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL15_EXTEN );
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}


#elif defined SYSCLK_FREQ_144MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo144_HSI
 *
 * @brief   Sets System clock frequency to 144MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo144_HSI( void )
{
		EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

		/* HCLK = SYSCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_HPRE_DIV1;
		/* PCLK2 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE2_DIV1;
		/* PCLK1 = HCLK */
		RCC->CFGR0 |= ( uint32_t )RCC_PPRE1_DIV2;

		/*  PLL configuration: PLLCLK = HSI * 18 = 144 MHz */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL ) );

#if defined (CH32F20x_D6) || defined (CH32F20x_D8) || defined (CH32F20x_D8W)
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL18 );
#else
		RCC->CFGR0 |= ( uint32_t )( RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL18_EXTEN );
#endif

		/* Enable PLL */
		RCC->CTLR |= RCC_PLLON;
		/* Wait till PLL is ready */
		while( ( RCC->CTLR & RCC_PLLRDY ) == 0 )
		{
		}
		/* Select PLL as system clock source */
		RCC->CFGR0 &= ( uint32_t )( ( uint32_t )~( RCC_SW ) );
		RCC->CFGR0 |= ( uint32_t )RCC_SW_PLL;
		/* Wait till PLL is used as system clock source */
		while( ( RCC->CFGR0 & ( uint32_t )RCC_SWS ) != ( uint32_t )0x08 )
		{
		}
}


#endif
