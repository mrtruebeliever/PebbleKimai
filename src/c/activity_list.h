#pragma once
#include <pebble.h>

// The main (and only) list window: global Kimai activities, with the running
// one marked and start/stop/switch driven from the Select button.
Window *activity_list_window(void);
void    activity_list_reload(void);
void    activity_list_destroy(void);
