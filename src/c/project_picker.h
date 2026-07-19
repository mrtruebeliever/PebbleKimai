#pragma once
#include <pebble.h>

// A single MenuLayer window with two modes over the streamed project list:
//  - PROJECTS: the projects of the active customer (customer sublabel when more
//    than one customer exists). Picking one books time on it from then on.
//  - CUSTOMERS: the distinct customers; picking one filters the project list and
//    drops straight into PROJECTS mode.
void project_picker_push_projects(void);   // opened from Select on the project row
void project_picker_push_customers(void);  // opened from a long-press on the project row
