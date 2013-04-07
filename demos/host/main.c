#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "boards/board.h"
#include "tusb.h"

#if defined(__CODE_RED)
  #include <cr_section_macros.h>
  #include <NXP/crp.h>
  // Variable to store CRP value in. Will be placed automatically
  // by the linker when "Enable Code Read Protect" selected.
  // See crp.h header for more information
  __CRP const unsigned int CRP_WORD = CRP_NO_CRP ;
#endif

int main(void)
{
  uint32_t current_tick = system_ticks;

  board_init();
  tusb_init();

  printf("reset\n");
  while (1)
  {
    tusb_task_runner();
    keyboard_app_task();
    mouse_app_task();

    if (current_tick + 10*CFG_TICKS_PER_SECOND < system_ticks)
    {
      current_tick = system_ticks;
//      board_leds(0x01, (current_tick/CFG_TICKS_PER_SECOND)%2); /* Toggle LED once per second */
      printf("tinyusb: " __DATE__ "\t" __TIME__ "\n"); // toggle leds on EA4357 is quite troublesome
    }
  }

  return 0;
}
