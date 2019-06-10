/*
    This module implements some business logic to test.

    Signal events by calling the functions on the module.
*/

#include <stdio.h>
#include <string.h>
#include "event_processor.h"
#include "display.h"

void event_deviceReset(void)
{
    //printf ("Device reset\n");
    display_turnOffStatusLed();
}

void event_volumeKnobMaxed(void)
{
    display_setVolume(11);
}

void event_powerReadingUpdate(int powerReading)
{
    if (powerReading >= 5)
    {
        display_turnOnStatusLed();
    }
}

void event_modeSelectButtonPressed(void)
{
    static int mode = 0;

    if (mode == 0)
    {
        display_setModeToMinimum();
        mode++;
    }
    else if (mode == 1)
    {
        display_setModeToMaximum();
        mode++;
    }
    else if (mode == 2)
    {
        display_setModeToAverage();
        mode++;
    }
    else
    {
        mode = 0;
    }
}

void event_devicePoweredOn(void)
{
    if (display_isError())
    {
        display_powerDown();
    }
}

void event_keyboardCheckTimerExpired(void)
{
    char userEntry[100];

    display_getKeyboardEntry(userEntry, 100);

    if (strcmp(userEntry, "sleep") == 0)
    {
        display_powerDown();
    }
}

static bool event_lastComplete = false;

/* Function called when the display update is complete. */
static void displayUpdateComplete(void)
{
    event_lastComplete = true;
}

void event_newDataAvailable(int data)
{
    event_lastComplete = false;
    display_updateData(data, displayUpdateComplete);
}

bool eventProcessor_isLastEventComplete(void)
{
    return event_lastComplete;
}
