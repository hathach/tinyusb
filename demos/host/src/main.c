#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "boards/board.h"
#include "tusb.h"

#include "mouse_app.h"
#include "keyboard_app.h"

#if defined(__CODE_RED)
  #include <cr_section_macros.h>
  #include <NXP/crp.h>
  // Variable to store CRP value in. Will be placed automatically
  // by the linker when "Enable Code Read Protect" selected.
  // See crp.h header for more information
  __CRP const unsigned int CRP_WORD = CRP_NO_CRP ;
#endif

void print_greeting(void);
int main(void)
{
  uint32_t current_tick = system_ticks;

  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  keyboard_app_init();
  mouse_app_init();

  while (1)
  {

#if TUSB_CFG_OS == TUSB_OS_NONE
    tusb_task_runner();
    keyboard_app_task(NULL);
    mouse_app_task(NULL);
#endif

    if (current_tick + CFG_TICKS_PER_SECOND < system_ticks)
    {
      current_tick += CFG_TICKS_PER_SECOND;

      /* Toggle LED once per second */
      if ( (current_tick/CFG_TICKS_PER_SECOND) % 2)
      {
        board_leds(0x01, 0x00);
      }
      else
      {
        board_leds(0x00, 0x01);
      }
    }
  }

  return 0;
}

void print_greeting(void)
{
  printf("\r\n\
--------------------------------------------------------------------\
-                     Host Demo (a tinyusb example)\r\n\
- if you find any bugs or get any questions, feel free to file an\r\n\
- issue at https://github.com/hathach/tinyusb\r\n\
--------------------------------------------------------------------\r\n\r\n"
  );
}
