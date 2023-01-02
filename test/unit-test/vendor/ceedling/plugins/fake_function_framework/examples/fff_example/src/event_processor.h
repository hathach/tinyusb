#include <stdbool.h>

void event_deviceReset(void);
void event_volumeKnobMaxed(void);
void event_powerReadingUpdate(int powerReading);
void event_modeSelectButtonPressed(void);
void event_devicePoweredOn(void);
void event_keyboardCheckTimerExpired(void);
void event_newDataAvailable(int data);

bool eventProcessor_isLastEventComplete(void);
