#include "bsp/board_api.h"
#include "board.h"

// This is a placeholder board C file. Please implement me!
// #error "This is a placeholder board C file. Please implement me!"

void board_init(void) {
  // TODO: board_clock_init();
  // TODO: board_uart_init();
}

void board_led_write(int state) {
  (void)state;
}

uint32_t board_button_read(void) {
  return 0;
}

int board_uart_read(uint8_t* buf, int len) {
  (void)buf; (void)len;
  return 0;
}

int board_uart_write(void const * buf, int len) {
  (void)buf; (void)len;
  return 0;
}

uint32_t board_millis(void) {
  return 0;
}
