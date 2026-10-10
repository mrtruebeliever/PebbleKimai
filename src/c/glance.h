#pragma once
#include <pebble.h>

// The app's line in the launcher ("app glance"). While a timer runs it shows
// the elapsed time, ticking live, and the activity; otherwise it is empty.
// Called when the app closes, from the timer state the phone last confirmed.
void glance_update(void);
