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

volatile uint32_t system_tick = 0;
void SysTick_Handler (void)
{
  system_tick++;
}

int main(void)
{
  uint32_t current_tick = system_tick;

  board_init();
	tusb_init(0);

  while (1)
  {
    if (current_tick + 1000 < system_tick)
    {
      current_tick += 1000;
      board_leds(0x03, (current_tick/1000)%2); /* Toggle LED once per second */
    }
  }
}
