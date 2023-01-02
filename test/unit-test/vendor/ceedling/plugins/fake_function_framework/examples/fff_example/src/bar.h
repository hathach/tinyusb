#ifndef bar_H
#define bar_H

#include "custom_types.h"

void bar_turn_on(void);
void bar_print_message(const char * message);
void bar_print_message_formatted(const char * format, ...);
void bar_numbers(int one, int two, char three);
void bar_const_test(const char * a, char * const b, const int c);
custom_t bar_needs_custom_type(void);
const char * bar_return_const_ptr(int one);

#endif // bar_H
