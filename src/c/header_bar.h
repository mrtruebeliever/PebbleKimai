#pragma once
#include <pebble.h>

// A branded top bar: accent fill, app title (or the ticking elapsed time while
// a timer runs) on the left, the clock on the right. Custom-drawn; the clock is
// kept fresh via a single app-wide minute tick, the elapsed time via the
// activity list's second timer calling header_bar_refresh().

#define HEADER_BAR_HEIGHT 28

// App lifecycle: subscribe/unsubscribe the minute tick.
void header_bar_init(void);
void header_bar_deinit(void);

// Creates a header layer (0,0,width,HEADER_BAR_HEIGHT) with the shared draw proc.
// The caller adds it to its window root and destroys it on unload.
Layer *header_bar_create(int width);

// Registers which header layer is currently visible. `show_elapsed` = true shows
// the ticking elapsed time on the left while a timer runs (the activity list);
// false always shows the app title (the hero, whose card already shows elapsed).
void header_bar_set_active(Layer *hb, bool show_elapsed);

// Marks the active header dirty (clock/elapsed changed). Safe if none.
void header_bar_refresh(void);
