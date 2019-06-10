#include "foo.h"
#include "bar.h"
#include "subfolder/zzz.h"

void foo_turn_on(void) {
  bar_turn_on();
  zzz_sleep(1, "sleepy");
}

void foo_print_message(const char * message) {
  bar_print_message(message);
}

void foo_print_special_message(void) {
  bar_print_message_formatted("The numbers are %d, %d and %d", 1, 2, 3);
}
