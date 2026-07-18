#pragma once
#include <pebble.h>

// A transient full-screen message (used for operation errors). Pushes a window
// with the given text and pops it automatically after a short delay.
void toast_show(const char *text);
