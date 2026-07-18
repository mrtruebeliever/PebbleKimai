#pragma once
#include <pebble.h>

// Kimai-ish green accent, used for the header bar and highlight rows.
#define ACCENT_HEX 0x55AA00

static inline GColor theme_accent(void) { return GColorFromHEX(ACCENT_HEX); }
