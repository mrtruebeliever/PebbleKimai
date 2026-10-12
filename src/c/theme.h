#pragma once
#include <pebble.h>

// Kimai-ish green accent, used for the header bar and highlight rows.
#define ACCENT_HEX 0x55AA00

static inline GColor theme_accent(void) { return GColorFromHEX(ACCENT_HEX); }

// On black-and-white screens the green would dither to a grey mesh under the
// text, so bars are white there and the selected row is black with white text.
static inline GColor theme_bar_bg(void) { return PBL_IF_COLOR_ELSE(theme_accent(), GColorWhite); }
static inline GColor theme_highlight_bg(void) { return PBL_IF_COLOR_ELSE(theme_accent(), GColorBlack); }
static inline GColor theme_row_fg(bool hl) {
  return PBL_IF_COLOR_ELSE(GColorBlack, hl ? GColorWhite : GColorBlack);
}
// The context line above a row's name: grey when not selected (colour only).
static inline GColor theme_row_sub(bool hl) {
  return PBL_IF_COLOR_ELSE(hl ? GColorBlack : GColorDarkGray, theme_row_fg(hl));
}
// The left-edge marker of the active project / running activity.
static inline GColor theme_marker(bool hl) {
  return PBL_IF_COLOR_ELSE(hl ? GColorBlack : theme_accent(), hl ? GColorWhite : GColorBlack);
}

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

// Inset of a row that sits on the screen's middle line -- the focused one, since
// round lists keep the selection centred.
#define THEME_FOCUSED_ROW_INSET PBL_IF_ROUND_ELSE(6, 0)

// The part of a list row its content is drawn in. The lists span the full
// width, so on a round screen the highlight runs out to the bezel; the content
// is inset to the circle's width at the row's current height on screen, so the
// rows slide along the edge as they scroll, like the system launcher.
static inline GRect theme_row_bounds(const Layer *cell) {
  GRect b = layer_get_bounds(cell);
#if defined(PBL_ROUND)
  GRect s = layer_convert_rect_to_screen(cell, b);
  int32_t r = PBL_DISPLAY_WIDTH / 2;
  int32_t dy = s.origin.y + s.size.h / 2 - r;
  if (dy < 0) dy = -dy;
  if (dy > r) dy = r;
  int32_t half = 0;
  while ((half + 1) * (half + 1) + dy * dy <= r * r) half++;   // isqrt, a few dozen steps
  int16_t inset = (int16_t)(r - half + THEME_FOCUSED_ROW_INSET);
  if (inset > r / 2) inset = r / 2;
  b.origin.x += inset;
  b.size.w -= 2 * inset;
#endif
  return b;
}

static inline int16_t theme_name_grow(void) {
  static const int16_t GROW[3] = { -4, 0, 6 };
  return GROW[theme_text_step() + 1];
}
