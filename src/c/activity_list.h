#pragma once
#include <pebble.h>

// The main (and only) list window: global Kimai activities, with the running
// one marked and start/stop/switch driven from the Select button.
Window *activity_list_window(void);
void    activity_list_reload(void);
void    activity_list_destroy(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool activity_list_is_top(void);
void activity_list_handle_touch(const TouchEvent *event);
