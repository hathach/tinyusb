#ifndef BOARD_H_
#define BOARD_H_

// This is a placeholder board header. Please edit me!
// #error "This is a placeholder board header. Please edit me!"

// TODO: Include actual CH32X035 SDK header if available
// #include "ch32x035.h"

// Example GPIO for LED
// #define LED_PORT              GPIOA
// #define LED_PIN               GPIO_PIN_5
// #define LED_STATE_ON          1

// Example GPIO for Button
// #define BUTTON_PORT           GPIOC
// #define BUTTON_PIN            GPIO_PIN_13
// #define BUTTON_STATE_ACTIVE   0

// Example UART for logs
// #define UART_DEV              USART1
// #define UART_TX_PORT          GPIOA
// #define UART_TX_PIN           GPIO_PIN_9
// #define UART_RX_PORT          GPIOA
// #define UART_RX_PIN           GPIO_PIN_10

void board_init(void);
void board_led_write(int state);
uint32_t board_button_read(void);
int board_uart_read(uint8_t* buf, int len);
int board_uart_write(void const * buf, int len);
uint32_t board_millis(void);

#endif // BOARD_H_
