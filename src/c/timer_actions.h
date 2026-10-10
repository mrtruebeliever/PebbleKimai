#pragma once
#include <pebble.h>
#include "data.h"

// Optimistic timer mutations + confirmation menus, shared by the hero screen and
// the activity list. Each mutation flips local state immediately, tells the
// phone, buzzes, and repaints both windows; the phone's authoritative
// TIMER_STATUS reply reconciles afterwards.
void timer_start(Activity *a);            // start a timer on `a`
void timer_switch(Activity *a);           // stop the running one, start `a`
void timer_stop(void);                    // stop the running timer
void timer_refresh(void);                 // reload projects + activities

// Colour backlight (Time 2; a no-op elsewhere): green while a timer runs, the
// user's own colour otherwise. Called on every timer state change.
void timer_apply_tint(void);

// Starts `a` on a specific project, selecting that project first. Used by the
// recent-activity shortcut, which crosses projects: a project-bound activity is
// rejected by Kimai under any other project, so the booking project has to
// follow the row. Switches instead of starting when a timer is already running.
void timer_start_in_project(const Activity *a, int project_id, const char *project_name);

// ActionMenu confirmations (accent-themed, the watch's own menu).
void timer_open_stop_menu(void);          // "Stop timer"
void timer_open_switch_menu(Activity *a); // "Switch to <name>"
void timer_open_long_press_menu(void);    // Refresh (+ Stop while running)
// "Switch to <name>", where confirming also moves to `project_id`.
void timer_open_switch_menu_in(const Activity *a, int project_id, const char *project_name);
