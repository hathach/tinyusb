#include <stdbool.h>

void display_turnOffStatusLed(void);
void display_turnOnStatusLed(void);
void display_setVolume(int level);
void display_setModeToMinimum(void);
void display_setModeToMaximum(void);
void display_setModeToAverage(void);
bool display_isError(void);
void display_powerDown(void);
void display_updateData(int data, void(*updateCompleteCallback)(void));

/*
    The entry is returned (up to `length` bytes) in the provided `entry` buffer.
*/
void display_getKeyboardEntry(char * entry, int length);
