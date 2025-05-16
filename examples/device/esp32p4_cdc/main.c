#include "tusb.h"

int main(void)
{
  board_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
  }

  return 0;
}