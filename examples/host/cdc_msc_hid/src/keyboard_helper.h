#ifndef KEYBOARD_HELPER_H
#define KEYBAORD_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#include "tusb.h"

// look up new key in previous keys
inline bool find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
{
  for(uint8_t i = 0; i < 6; i++)
  {
    if (p_report->keycode[i] == keycode)  return true;
  }

  return false;
}

inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode)
{
  return keycode > 128 ? 0 :
    hid_keycode_to_ascii_tbl [keycode][modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT) ? 1 : 0];
}

void print_kbd_report(hid_keyboard_report_t *prev_report, hid_keyboard_report_t const *new_report)
{

  printf("Report: ");
  uint8_t c;

  // I assume it's possible to have up to 6 keypress events per report?
  for (uint8_t i = 0; i < 6; i++)
  {
    // Check for key presses
    if (new_report->keycode[i])
    {
      // If not in prev report then it is newly pressed
      if ( !find_key_in_report(prev_report, new_report->keycode[i]) )
        c = keycode_to_ascii(new_report->modifier, new_report->keycode[i]);
        printf("press %c ", c);
    }

    // Check for key depresses (i.e. was present in prev report but not here)
    if (prev_report->keycode[i])
    {
      // If not present in the current report then depressed
      if (!find_key_in_report(new_report, prev_report->keycode[i]))
      {
        c = keycode_to_ascii(prev_report->modifier, prev_report->keycode[i]);
        printf("depress %c ", c);
      }
    }
  }

  printf("\n");
}

#endif