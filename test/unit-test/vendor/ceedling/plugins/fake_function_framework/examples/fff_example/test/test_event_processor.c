#include "unity.h"
#include "event_processor.h"
#include "mock_display.h"
#include <string.h>

void setUp (void)
{
}

void tearDown (void)
{
}
/*
    Test that a single function was called.
*/
void
test_whenTheDeviceIsReset_thenTheStatusLedIsTurnedOff()
{
    // When
    event_deviceReset();

    // Then
    TEST_ASSERT_EQUAL(1, display_turnOffStatusLed_fake.call_count);
    // or use the helper macro...
    TEST_ASSERT_CALLED(display_turnOffStatusLed);
}

/*
    Test that a single function is NOT called.
*/
void
test_whenThePowerReadingIsLessThan5_thenTheStatusLedIsNotTurnedOn(void)
{
    // When
    event_powerReadingUpdate(4);

    // Then
    TEST_ASSERT_EQUAL(0, display_turnOnStatusLed_fake.call_count);
    // or use the helper macro...
    TEST_ASSERT_NOT_CALLED(display_turnOffStatusLed);
}

/*
    Test that a single function was called with the correct argument.
*/
void
test_whenTheVolumeKnobIsMaxed_thenVolumeDisplayIsSetTo11(void)
{
    // When
    event_volumeKnobMaxed();

    // Then
    TEST_ASSERT_EQUAL(1, display_setVolume_fake.call_count);
    // or use the helper macro...
    TEST_ASSERT_CALLED(display_setVolume);
    TEST_ASSERT_EQUAL(11, display_setVolume_fake.arg0_val);
}

/*
    Test a sequence of calls.
*/

void
test_whenTheModeSelectButtonIsPressed_thenTheDisplayModeIsCycled(void)
{
    // When
    event_modeSelectButtonPressed();
    event_modeSelectButtonPressed();
    event_modeSelectButtonPressed();

    // Then
    TEST_ASSERT_EQUAL_PTR((void *)display_setModeToMinimum, fff.call_history[0]);
    TEST_ASSERT_EQUAL_PTR((void *)display_setModeToMaximum, fff.call_history[1]);
    TEST_ASSERT_EQUAL_PTR((void *)display_setModeToAverage, fff.call_history[2]);
    // or use the helper macros...
    TEST_ASSERT_CALLED_IN_ORDER(0, display_setModeToMinimum);
    TEST_ASSERT_CALLED_IN_ORDER(1, display_setModeToMaximum);
    TEST_ASSERT_CALLED_IN_ORDER(2, display_setModeToAverage);
}

/*
    Mock a return value from a function.
*/
void
test_givenTheDisplayHasAnError_whenTheDeviceIsPoweredOn_thenTheDisplayIsPoweredDown(void)
{
    // Given
    display_isError_fake.return_val = true;

    // When
    event_devicePoweredOn();

    // Then
    TEST_ASSERT_EQUAL(1, display_powerDown_fake.call_count);
    // or use the helper macro...
    TEST_ASSERT_CALLED(display_powerDown);
}

/*
	Mock a sequence of calls with return values.
*/

/*
    Mocking a function with a value returned by reference.
*/
void
test_givenTheUserHasTypedSleep_whenItIsTimeToCheckTheKeyboard_theDisplayIsPoweredDown(void)
{
    // Given
    char mockedEntry[] = "sleep";
    void return_mock_value(char * entry, int length)
    {
        if (length > strlen(mockedEntry))
        {
            strncpy(entry, mockedEntry, length);
        }
    }
    display_getKeyboardEntry_fake.custom_fake = return_mock_value;

    // When
    event_keyboardCheckTimerExpired();

    // Then
    TEST_ASSERT_EQUAL(1, display_powerDown_fake.call_count);
    // or use the helper macro...
    TEST_ASSERT_CALLED(display_powerDown);
}

/*
    Mock a function with a function pointer parameter.
*/
void
test_givenNewDataIsAvailable_whenTheDisplayHasUpdated_thenTheEventIsComplete(void)
{
    // A mock function for capturing the callback handler function pointer.
    void(*registeredCallback)(void) = 0;
    void mock_display_updateData(int data, void(*callback)(void))
    {
        //Save the callback function.
        registeredCallback = callback;
    }
    display_updateData_fake.custom_fake = mock_display_updateData;

    // Given
    event_newDataAvailable(10);

    // When
    if (registeredCallback != 0)
    {
        registeredCallback();
    }

    // Then
    TEST_ASSERT_EQUAL(true, eventProcessor_isLastEventComplete());
}
