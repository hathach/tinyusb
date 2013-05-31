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

void print_greeting(void);

void led_blinking_task(void * p_para);
void keyboard_device_app_task(void * p_para);

int main(void)
{
  uint32_t current_tick = system_ticks;

  board_init();
  tusb_init();

  print_greeting();
  while (1)
  {
    if (current_tick + 1000 < system_ticks)
    {
      current_tick += 1000;

      led_blinking_task(NULL);

      #if TUSB_CFG_DEVICE_HID_KEYBOARD
      keyboard_device_app_task(NULL);
      #endif

      #if TUSB_CFG_DEVICE_HID_MOUSE
      if (usb_isConfigured())
      {
          tusb_hid_mouse_send(0, 10, 10);
      }
      #endif
    }

    #if defined TUSB_CFG_DEVICE_CDC && 0
    if (usb_isConfigured())
    {
      uint8_t cdc_char;
      if( tusb_cdc_getc(&cdc_char) )
      {
        switch (cdc_char)
        {
          #ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
          case '1' :
          {
            uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA + 'e' - 'a'};
            tusbd_hid_keyboard_send_report(0x08, keys, 1); // windows + E --> open explorer
          }
          break;
          #endif

          #ifdef TUSB_CFG_DEVICE_HID_MOUSE
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

void led_blinking_task(void * p_para)
{
  static uint32_t led_on_mask = 0;

  board_leds(led_on_mask, 1 - led_on_mask);
  led_on_mask = 1 - led_on_mask; // toggle
}

#if TUSB_CFG_DEVICE_HID_KEYBOARD
void keyboard_device_app_task(void * p_para)
{
  if (usb_isConfigured())
  {
    static uint32_t count =0;
    if (count < 4)
    {
      uint8_t keys[6] = {0x04}; // A character
      tusbd_hid_keyboard_send_report(0x00, keys, 1);
      count++;
    }
  }
}
#endif

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  printf("\r\n\
--------------------------------------------------------------------\r\n\
-                     Device Demo (a tinyusb example)\r\n\
- if you find any bugs or get any questions, feel free to file an\r\n\
- issue at https://github.com/hathach/tinyusb\r\n\
--------------------------------------------------------------------\r\n\r\n"
  );
}
