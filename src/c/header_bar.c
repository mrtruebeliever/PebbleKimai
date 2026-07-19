#include "header_bar.h"
#include "data.h"
#include "theme.h"

static Layer *s_active = NULL;   // the header currently on screen

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // Accent fill + a thin black bottom separator.
  graphics_context_set_fill_color(ctx, theme_accent());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, b.size.h - 1), GPoint(b.size.w, b.size.h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Time (right-aligned).
  char time_buf[16];
  clock_copy_time_string(time_buf, sizeof(time_buf));
  const int TIME_W = 56;
  int time_left = b.size.w - 4 - TIME_W;
  graphics_draw_text(ctx, time_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(time_left, 4, TIME_W, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Left: the ticking elapsed time while a timer runs, the app title otherwise.
  // The "running" dot is drawn (not a glyph) so it renders in every system font.
  TimerState *tm = data_timer();
  if (tm->status == TIMER_RUNNING) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(11, 13), 4);
    char elapsed[16];
    data_timer_elapsed_str(elapsed, sizeof(elapsed));
    graphics_draw_text(ctx, elapsed, f18, GRect(20, 4, time_left - 24, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, "Kimai", f18, GRect(6, 4, time_left - 12, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_init(void) {
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void header_bar_deinit(void) {
  tick_timer_service_unsubscribe();
}

Layer *header_bar_create(int width) {
  Layer *layer = layer_create(GRect(0, 0, width, HEADER_BAR_HEIGHT));
  layer_set_update_proc(layer, header_update);
  return layer;
}

void header_bar_set_active(Layer *hb) {
  s_active = hb;
  if (s_active) { layer_mark_dirty(s_active); }
}

void header_bar_refresh(void) {
  if (s_active) { layer_mark_dirty(s_active); }
}
