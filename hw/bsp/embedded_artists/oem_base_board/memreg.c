/*****************************************************************************
 *
 *   Copyright(C) 2011, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * Embedded Artists AB assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. Embedded Artists AB
 * reserves the right to make changes in the software without
 * notification. Embedded Artists AB also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 *****************************************************************************/



/******************************************************************************
 * Includes
 *****************************************************************************/
#include "../../board.h"

#if BOARD == BOARD_EA4357

#include "LPC43xx.h"
#include "lpc_types.h"
#include "lpc43xx_scu.h"
#include "memreg.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/


/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/

static void pinConfig(void)
{
  /* Set up EMC pin */
  scu_pinmux(   2  ,   9  ,  MD_PLN_FAST  ,  3  );//A0
  scu_pinmux(   2  ,  10  ,  MD_PLN_FAST  ,  3  );//A1
  scu_pinmux(   2  ,  11  ,  MD_PLN_FAST  ,  3  );//A2
  scu_pinmux(   2  ,  12  ,  MD_PLN_FAST  ,  3  );//A3
  scu_pinmux(   2  ,  13  ,  MD_PLN_FAST  ,  3  );//A4
  scu_pinmux(   1  ,   0  ,  MD_PLN_FAST  ,  2  );//A5
  scu_pinmux(   1  ,   1  ,  MD_PLN_FAST  ,  2  );//A6
  scu_pinmux(   1  ,   2  ,  MD_PLN_FAST  ,  2  );//A7
  scu_pinmux(   2  ,   8  ,  MD_PLN_FAST  ,  3  );//A8
  scu_pinmux(   2  ,   7  ,  MD_PLN_FAST  ,  3  );//A9
  scu_pinmux(   2  ,   6  ,  MD_PLN_FAST  ,  2  );//A10
  scu_pinmux(   2  ,   2  ,  MD_PLN_FAST  ,  2  );//A11
  scu_pinmux(   2  ,   1  ,  MD_PLN_FAST  ,  2  );//A12
  scu_pinmux(   2  ,   0  ,  MD_PLN_FAST  ,  2  );//A13
  scu_pinmux(   6  ,   8  ,  MD_PLN_FAST  ,  1  );//A14
  scu_pinmux(   6  ,   7  ,  MD_PLN_FAST  ,  1  );//A15
  scu_pinmux(  13  ,  16  ,  MD_PLN_FAST  ,  2  );//A16
  scu_pinmux(  13  ,  15  ,  MD_PLN_FAST  ,  2  );//A17
  scu_pinmux(  14  ,   0  ,  MD_PLN_FAST  ,  3  );//A18
  scu_pinmux(  14  ,   1  ,  MD_PLN_FAST  ,  3  );//A19
  scu_pinmux(  14  ,   2  ,  MD_PLN_FAST  ,  3  );//A20
  scu_pinmux(  14  ,   3  ,  MD_PLN_FAST  ,  3  );//A21
  scu_pinmux(  14  ,   4  ,  MD_PLN_FAST  ,  3  );//A22
  scu_pinmux(  10  ,   4  ,  MD_PLN_FAST  ,  3  );//A23

  scu_pinmux(   1  ,   7  ,  MD_PLN_FAST  ,  3  );//D0
  scu_pinmux(   1  ,   8  ,  MD_PLN_FAST  ,  3  );//D1
  scu_pinmux(   1  ,   9  ,  MD_PLN_FAST  ,  3  );//D2
  scu_pinmux(   1  ,  10  ,  MD_PLN_FAST  ,  3  );//D3
  scu_pinmux(   1  ,  11  ,  MD_PLN_FAST  ,  3  );//D4
  scu_pinmux(   1  ,  12  ,  MD_PLN_FAST  ,  3  );//D5
  scu_pinmux(   1  ,  13  ,  MD_PLN_FAST  ,  3  );//D6
  scu_pinmux(   1  ,  14  ,  MD_PLN_FAST  ,  3  );//D7
  scu_pinmux(   5  ,   4  ,  MD_PLN_FAST  ,  2  );//D8
  scu_pinmux(   5  ,   5  ,  MD_PLN_FAST  ,  2  );//D9
  scu_pinmux(   5  ,   6  ,  MD_PLN_FAST  ,  2  );//D10
  scu_pinmux(   5  ,   7  ,  MD_PLN_FAST  ,  2  );//D11
  scu_pinmux(   5  ,   0  ,  MD_PLN_FAST  ,  2  );//D12
  scu_pinmux(   5  ,   1  ,  MD_PLN_FAST  ,  2  );//D13
  scu_pinmux(   5  ,   2  ,  MD_PLN_FAST  ,  2  );//D14
  scu_pinmux(   5  ,   3  ,  MD_PLN_FAST  ,  2  );//D15
  scu_pinmux(  13  ,   2  ,  MD_PLN_FAST  ,  2  );//D16
  scu_pinmux(  13  ,   3  ,  MD_PLN_FAST  ,  2  );//D17
  scu_pinmux(  13  ,   4  ,  MD_PLN_FAST  ,  2  );//D18
  scu_pinmux(  13  ,   5  ,  MD_PLN_FAST  ,  2  );//D19
  scu_pinmux(  13  ,   6  ,  MD_PLN_FAST  ,  2  );//D20
  scu_pinmux(  13  ,   7  ,  MD_PLN_FAST  ,  2  );//D21
  scu_pinmux(  13  ,   8  ,  MD_PLN_FAST  ,  2  );//D22
  scu_pinmux(  13  ,   9  ,  MD_PLN_FAST  ,  2  );//D23
  scu_pinmux(  14  ,   5  ,  MD_PLN_FAST  ,  3  );//D24
  scu_pinmux(  14  ,   6  ,  MD_PLN_FAST  ,  3  );//D25
  scu_pinmux(  14  ,   7  ,  MD_PLN_FAST  ,  3  );//D26
  scu_pinmux(  14  ,   8  ,  MD_PLN_FAST  ,  3  );//D27
  scu_pinmux(  14  ,   9  ,  MD_PLN_FAST  ,  3  );//D28
  scu_pinmux(  14  ,  10  ,  MD_PLN_FAST  ,  3  );//D29
  scu_pinmux(  14  ,  11  ,  MD_PLN_FAST  ,  3  );//D30
  scu_pinmux(  14  ,  12  ,  MD_PLN_FAST  ,  3  );//D31

  scu_pinmux(   1  ,   3  ,  MD_PLN_FAST  ,  3  );//OE
  scu_pinmux(   1  ,   6  ,  MD_PLN_FAST  ,  3  );//WE

  scu_pinmux(   1  ,   4  ,  MD_PLN_FAST  ,  3  );//BLS0
  scu_pinmux(   6  ,   6  ,  MD_PLN_FAST  ,  1  );//BLS1
  scu_pinmux(  13  ,  13  ,  MD_PLN_FAST  ,  2  );//BLS2
  scu_pinmux(  13  ,  10  ,  MD_PLN_FAST  ,  2  );//BLS3

  scu_pinmux(   1  ,   5  ,  MD_PLN_FAST  ,  3  );//CS0
  scu_pinmux(   6  ,   3  ,  MD_PLN_FAST  ,  3  );//CS1
  scu_pinmux(  13  ,  12  ,  MD_PLN_FAST  ,  2  );//CS2
  scu_pinmux(  13  ,  11  ,  MD_PLN_FAST  ,  2  );//CS3

  scu_pinmux(   6  ,   4  ,  MD_PLN_FAST  ,  3  );//CAS
  scu_pinmux(   6  ,   5  ,  MD_PLN_FAST  ,  3  );//RAS

  scu_pinmux(   6  ,   9  ,  MD_PLN_FAST  ,  3  );//DYCS0
  scu_pinmux(   6  ,   1  ,  MD_PLN_FAST  ,  1  );//DYCS1
  scu_pinmux(  13  ,  14  ,  MD_PLN_FAST  ,  2  );//DYCS2
  scu_pinmux(  15  ,  14  ,  MD_PLN_FAST  ,  3  );//DYCS3

  scu_pinmux(   6  ,  11  ,  MD_PLN_FAST  ,  3  );//CKEOUT0
  scu_pinmux(   6  ,   2  ,  MD_PLN_FAST  ,  1  );//CKEOUT1
  scu_pinmux(  13  ,   1  ,  MD_PLN_FAST  ,  2  );//CKEOUT2
  scu_pinmux(  14  ,  15  ,  MD_PLN_FAST  ,  3  );//CKEOUT3

  scu_pinmux(   6  ,  12  ,  MD_PLN_FAST  ,  3  );//DQMOUT0
  scu_pinmux(   6  ,  10  ,  MD_PLN_FAST  ,  3  );//DQMOUT1
  scu_pinmux(  13  ,   0  ,  MD_PLN_FAST  ,  2  );//DQMOUT2
  scu_pinmux(  14  ,  13  ,  MD_PLN_FAST  ,  3  );//DQMOUT3
}



/******************************************************************************
 * Local Functions
 *****************************************************************************/



/******************************************************************************
 * Public Functions
 *****************************************************************************/



/******************************************************************************
 *
 * Description:
 *    Initialize the NOR Flash
 *
 *****************************************************************************/
uint32_t memreg_init (void)
{
  LPC_EMC->CONTROL   = 0x00000001;
  LPC_EMC->CONFIG    = 0x00000000;

  pinConfig();

  // Setup for 16-bit access
  LPC_EMC->STATICCONFIG2   = 0x00000001;

  return FALSE;
}

#endif

