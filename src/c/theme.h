#pragma once
#include <pebble.h>

// Kimai-ish green accent, used for the header bar and highlight rows.
#define ACCENT_HEX 0x55AA00

static inline GColor theme_accent(void) { return GColorFromHEX(ACCENT_HEX); }

// The watch's text size (Settings → Display), as a step from this screen's
// default: -1 smaller, 0 default, +1 larger. The firmware defaults to Large on
// screens 200px and taller and to Medium below, and that default is what the
// layouts here were drawn for.
static inline int theme_text_step(void) {
  int def = PBL_DISPLAY_HEIGHT >= 200 ? PreferredContentSizeLarge : PreferredContentSizeMedium;
  int d = (int)preferred_content_size() - def;
  return d < -1 ? -1 : (d > 1 ? 1 : d);
}

// The main line of a list row (activity, project, recent): Gothic 18 bold at
// the default size, one step down or up with the watch's setting. Rows grow by
// theme_name_grow() pixels to match; the small context line above stays put.
static inline GFont theme_name_font(void) {
  static const char *const KEYS[3] = {
    FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD,
  };
  return fonts_get_system_font(KEYS[theme_text_step() + 1]);
}

static inline int16_t theme_name_grow(void) {
  static const int16_t GROW[3] = { -4, 0, 6 };
  return GROW[theme_text_step() + 1];
}
