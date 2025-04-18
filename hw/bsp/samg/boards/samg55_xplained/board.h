/* metadata:
   name: SAMG55 Xplained Pro
   url: https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAMG55-XPRO
*/

#ifndef BOARD_H
#define BOARD_H

#define LED_PIN               GPIO(GPIO_PORTA, 6)

#define BUTTON_PIN            GPIO(GPIO_PORTA, 2)
#define BUTTON_STATE_ACTIVE   0

#define UART_TX_PIN           GPIO(GPIO_PORTA, 28)
#define UART_RX_PIN           GPIO(GPIO_PORTA, 27)

#endif
