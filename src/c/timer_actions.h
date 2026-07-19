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

// ActionMenu confirmations (accent-themed).
void timer_open_stop_menu(void);          // "Stop timer"
void timer_open_switch_menu(Activity *a); // "Switch to <name>"
void timer_open_long_press_menu(void);    // Refresh (+ Stop while running)
