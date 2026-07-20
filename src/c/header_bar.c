#include "header_bar.h"
#include "data.h"
#include "theme.h"

static GBitmap   *s_icon   = NULL;   // stopwatch shown at the far left of the bar
static Layer     *s_active = NULL;   // the header currently on screen
static bool       s_show_elapsed = true;  // show ticking elapsed vs. the title
static const char *s_title = NULL;   // title override; NULL = default "Kimai"

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // Accent fill + a thin black bottom separator.
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, b.size.h - 1), GPoint(b.size.w, b.size.h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // App icon (left, vertically centered); the left text starts after it.
  int name_x = 6;
  if (s_icon) {
    GRect ib = gbitmap_get_bounds(s_icon);
    int iy = (b.size.h - ib.size.h) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_icon, GRect(3, iy, ib.size.w, ib.size.h));
    name_x = 3 + ib.size.w + 4;
  }

  // Time (right-aligned).
  char time_buf[16];
  clock_copy_time_string(time_buf, sizeof(time_buf));
  const int TIME_W = 56;
  int time_left = b.size.w - 4 - TIME_W;
  graphics_draw_text(ctx, time_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(time_left, 4, TIME_W, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Between the icon and the clock: the ticking elapsed time while a timer runs
  // (only where the window asked for it), otherwise the app title.
  TimerState *tm = data_timer();
  char left_buf[24];
  if (tm->status == TIMER_RUNNING && s_show_elapsed) {
    data_timer_elapsed_str(left_buf, sizeof(left_buf));
  } else {
    snprintf(left_buf, sizeof(left_buf), "%s", s_title ? s_title : "Kimai");
  }
  graphics_draw_text(ctx, left_buf, f18, GRect(name_x, 4, time_left - name_x - 4, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_init(void) {
  s_icon = gbitmap_create_with_resource(RESOURCE_ID_IMG_HEADER_ICON);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void header_bar_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_icon) { gbitmap_destroy(s_icon); s_icon = NULL; }
}

Layer *header_bar_create(int width) {
  Layer *layer = layer_create(GRect(0, 0, width, HEADER_BAR_HEIGHT));
  layer_set_update_proc(layer, header_update);
  return layer;
}

void header_bar_set_active(Layer *hb, bool show_elapsed) {
  s_active = hb;
  s_show_elapsed = show_elapsed;
  s_title = NULL;
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_set_title(const char *title) {
  s_title = title;
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_refresh(void) {
  if (s_active) { layer_mark_dirty(s_active); }
}
