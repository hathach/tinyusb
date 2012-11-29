#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include "tusb.h"

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP ;

int main(void) 
{
  uint32_t currentSecond, lastSecond;
  currentSecond = lastSecond = 0;

  SystemInit();

  systickInit(1);
  GPIOInit();

    #define CFG_LED_PORT                  (0)
    #define CFG_LED_PIN                   (7)
    #define CFG_LED_ON                    (1)
    #define CFG_LED_OFF                   (0)

  GPIOSetDir(CFG_LED_PORT, CFG_LED_PIN, 1);
  LPC_GPIO->CLR[CFG_LED_PORT] = (1 << CFG_LED_PIN);

  tusb_init();

  while (1)
  {
    currentSecond = systickGetSecondsActive();
    if (currentSecond != lastSecond)
    {
      /* Toggle LED once per second */
      lastSecond = currentSecond;
      GPIOSetBitValue(CFG_LED_PORT, CFG_LED_PIN, lastSecond % 2);

      #if !defined(CFG_USB_CDC)
      if (usb_isConfigured())
      {
        #ifdef CFG_CLASS_HID_KEYBOARD
          uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA};
          usb_hid_keyboard_sendKeys(0x00, keys, 1);
        #endif

        #ifdef CFG_CLASS_HID_MOUSE          
          usb_hid_mouse_send(0, 10, 10);
        #endif
      }
      #endif
    }
  }

  return 0;
}
