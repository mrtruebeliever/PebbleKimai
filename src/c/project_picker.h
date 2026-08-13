#pragma once
#include <pebble.h>

// A single MenuLayer window with two modes over the streamed project list:
//  - PROJECTS: the projects of the active customer (customer sublabel when more
//    than one customer exists). Picking one books time on it from then on.
//  - CUSTOMERS: the distinct customers; picking one filters the project list and
//    drops straight into PROJECTS mode.
void project_picker_push_projects(void);   // opened from Select on the project row
void project_picker_push_customers(void);  // opened from a long-press on the project row

// Rebuilds the picker from the live project list if it is open (e.g. projects
// arrived from the phone while it was showing "loading"). No-op when closed.
void project_picker_reload(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool project_picker_is_top(void);
void project_picker_handle_touch(const TouchEvent *event);
