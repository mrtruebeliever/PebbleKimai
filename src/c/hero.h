#pragma once
#include <pebble.h>

// The landing screen: a hero card for the running activity (name + big ticking
// elapsed + project/customer) or, when idle, the last activity with a
// "start again" affordance. Select/Down jump into the activity list.
Window *hero_window(void);
void    hero_reload(void);     // repaint the card + header
void    hero_destroy(void);
