#pragma once
#include <pebble.h>

// Recent-activity shortcut: the last activities that actually ran, newest
// first, across every project. Reached with a long press of Down (or a tap on
// the hero's status icon). Picking a row books on that row's own project.
void recents_push(void);
void recents_reload(void);                       // repaint if open (new data arrived)
bool recents_is_top(void);
void recents_handle_touch(const TouchEvent *event);
