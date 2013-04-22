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


#include "lpc_types.h"
#include "lpc43xx_gpio.h"
#include "lpc43xx_scu.h"
#include "joystick.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define GPIO_PIN_LEFT    (1<<9)
#define GPIO_PIN_RIGHT   (1<<12)
#define GPIO_PIN_UP      (1<<10)
#define GPIO_PIN_DOWN    (1<<13)
#define GPIO_PIN_CENTER  (1<<8)

#define GPIO_PORT  4


/******************************************************************************
 * External global variables
 *****************************************************************************/
// TODO move later
/* Pin mode defines, more in line with the definitions in the LPC1800/4300 user manual */
/* Defines for SFSPx_y pin configuration registers                                     */
#define PDN_ENABLE        (1 << 3)	// Pull-down enable
#define PDN_DISABLE       (0 << 3)      // Pull-down disable
#define PUP_ENABLE        (0 << 4)      // Pull-up enable
#define PUP_DISABLE       (1 << 4)	// Pull-up disable
#define SLEWRATE_SLOW	  (0 << 5)	// Slew rate for low noise with medium speed
#define SLEWRATE_FAST	  (1 << 5)	// Slew rate for medium noise with fast speed
#define INBUF_ENABLE	  (1 << 6)	// Input buffer
#define INBUF_DISABLE	  (0 << 6)	// Input buffer
#define FILTER_ENABLE	  (0 << 7)	// Glitch filter (for signals below 30MHz)
#define FILTER_DISABLE	  (1 << 7)	// No glitch filter (for signals above 30MHz)
#define DRIVE_8MA         (1 << 8)	// Drive strength of 8mA
#define DRIVE_14MA        (1 << 9)	// Drive strength of 14mA
#define DRIVE_20MA        (3 << 8)	// Drive strength of 20mA


/* Configuration examples for various I/O pins */
#define EMC_IO	        (PUP_ENABLE  | PDN_ENABLE  | SLEWRATE_FAST | INBUF_ENABLE  | FILTER_DISABLE)
#define LCD_PINCONFIG   (PUP_DISABLE | PDN_DISABLE | SLEWRATE_FAST | INBUF_ENABLE  | FILTER_DISABLE)
#define CLK_IN	        (PUP_ENABLE  | PDN_ENABLE  | SLEWRATE_FAST | INBUF_ENABLE  | FILTER_DISABLE)
#define CLK_OUT	        (PUP_ENABLE  | PDN_ENABLE  | SLEWRATE_FAST | INBUF_ENABLE  | FILTER_DISABLE)
#define GPIO_PUP	(PUP_ENABLE  | PDN_DISABLE | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE )
#define GPIO_PDN	(PUP_DISABLE | PDN_ENABLE  | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE )
#define GPIO_NOPULL	(PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE )
#define UART_RX_TX	(PUP_DISABLE | PDN_ENABLE  | SLEWRATE_SLOW | INBUF_ENABLE  | FILTER_ENABLE )
#define SSP_IO	        (PUP_ENABLE  | PDN_ENABLE  | SLEWRATE_FAST | INBUF_ENABLE  | FILTER_DISABLE)
/******************************************************************************
 * Local variables
 *****************************************************************************/


/******************************************************************************
 * Local Functions
 *****************************************************************************/


/******************************************************************************
 * Public Functions
 *****************************************************************************/

/******************************************************************************
 *
 * Description:
 *    Initialize the Joystick Driver
 *
 *****************************************************************************/
void joystick_init (void)
{
  /* set to GPIO function for the 5 pins used with the joystick */
  scu_pinmux(	0xa	,	1	,	GPIO_NOPULL	,	FUNC0	);//GPIO4[8]
  scu_pinmux(	0xa	,	2	,	GPIO_NOPULL	,	FUNC0	);//GPIO4[9]
  scu_pinmux(	0xa	,	3	,	GPIO_NOPULL	,	FUNC0	);//GPIO4[10]
  scu_pinmux(	0x9	,	0	,	GPIO_NOPULL	,	FUNC0	);//GPIO4[12]
  scu_pinmux(	0x9	,	1	,	GPIO_NOPULL	,	FUNC0	);//GPIO4[13]

  /* set the pins as inputs */
  GPIO_SetDir(GPIO_PORT, GPIO_PIN_LEFT, 0);
  GPIO_SetDir(GPIO_PORT, GPIO_PIN_RIGHT, 0);
  GPIO_SetDir(GPIO_PORT, GPIO_PIN_UP, 0);
  GPIO_SetDir(GPIO_PORT, GPIO_PIN_DOWN, 0);
  GPIO_SetDir(GPIO_PORT, GPIO_PIN_CENTER, 0);
}

/******************************************************************************
 *
 * Description:
 *    Read the joystick status
 *
 * Returns:
 *   The joystick status. The returned value is a bit mask. More than one
 *   direction may be active at any given time (e.g. UP and RIGHT)
 *
 *****************************************************************************/
uint8_t joystick_read(void)
{
    uint8_t status = 0;
    uint32_t pinVal = 0;

    pinVal = GPIO_ReadValue(GPIO_PORT);
    pinVal = pinVal;

    if ((pinVal & GPIO_PIN_DOWN) == 0) {
      status |= JOYSTICK_DOWN;
    }

    if ((pinVal & GPIO_PIN_RIGHT) == 0) {
      status |= JOYSTICK_RIGHT;
    }

    if ((pinVal & GPIO_PIN_UP) == 0) {
      status |= JOYSTICK_UP;
    }

    if ((pinVal & GPIO_PIN_LEFT) == 0) {
      status |= JOYSTICK_LEFT;
    }

    if ((pinVal & GPIO_PIN_CENTER) == 0) {
      status |= JOYSTICK_CENTER;
    }

    return status;
}





