#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include "boards/board.h"
#include "tusb.h"

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP ;

volatile uint32_t system_tick = 0;

void SysTick_Handler (void)
{
  system_tick++;
}

int main(void) 
{
  uint32_t current_tick = system_tick;

  board_init();
  tusb_init();

  while (1)
  {
    if (current_tick + 1000 < system_tick)
    {
      current_tick += 1000;
      board_leds(0x01, (current_tick/1000)%2); /* Toggle LED once per second */

      #ifndef CFG_CLASS_CDC
      if (usb_isConfigured())
      {
        #ifdef CFG_CLASS_HID_KEYBOARD
          uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA};
          tusb_hid_keyboard_sendKeys(0x00, keys, 1);
        #endif

        #ifdef CFG_CLASS_HID_MOUSE          
          tusb_hid_mouse_send(0, 10, 10);
        #endif
      }
      #endif
    }

    #ifdef CFG_CLASS_CDC
    if (usb_isConfigured())
    {
      uint8_t cdc_char;
      if( tusb_cdc_getc(&cdc_char) )
      {
        switch (cdc_char)
        {
          #ifdef CFG_CLASS_HID_KEYBOARD
          case '1' :
          {
            uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA + 'e' - 'a'};
            tusb_hid_keyboard_sendKeys(0x08, keys, 1); // windows + E --> open explorer
          }
          break;
          #endif

          #ifdef CFG_CLASS_HID_MOUSE
          case '2' :
            tusb_hid_mouse_send(0, 10, 10);
          break;
          #endif

          default :
            cdc_char = toupper(cdc_char);
            tusb_cdc_putc(cdc_char);
          break;

        }
      }
    }
#endif
  }

  return 0;
}
