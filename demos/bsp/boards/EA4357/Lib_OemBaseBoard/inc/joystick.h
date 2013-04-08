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
#ifndef __JOYSTICK_H
#define __JOYSTICK_H


#define JOYSTICK_CENTER 0x01
#define JOYSTICK_UP     0x02
#define JOYSTICK_DOWN   0x04
#define JOYSTICK_LEFT   0x08
#define JOYSTICK_RIGHT  0x10


void joystick_init (void);
uint8_t joystick_read(void);



#endif /* end __JOYSTICK_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/
