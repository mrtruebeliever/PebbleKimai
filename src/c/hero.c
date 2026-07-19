#include "hero.h"
#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "project_picker.h"
#include "theme.h"
#include "timer_actions.h"
#include "touch_nav.h"
#include <stdlib.h>

static Window   *s_window = NULL;
static Layer    *s_body = NULL;
static Layer    *s_header = NULL;
static AppTimer *s_tick = NULL;

static bool has_project_row(void) {
  return data_project_count() > 0 || data_active_project_name()[0];
}

// --- Card drawing ------------------------------------------------------------

static void draw_card(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int H = b.size.h;
  bool round = PBL_IF_ROUND_ELSE(true, false);
  bool wide  = b.size.w >= 170;
  GTextAlignment al = round ? GTextAlignmentCenter : GTextAlignmentLeft;
  int lx = round ? 0 : 12;
  int lw = round ? b.size.w : b.size.w - 16;

  // Accent color, but never invisible on the black/white platforms.
  GColor accent = PBL_IF_COLOR_ELSE(theme_accent(), GColorBlack);

  // Left accent stripe marks the card (rectangular screens only).
  if (!round) {
    graphics_context_set_fill_color(ctx, accent);
    graphics_fill_rect(ctx, GRect(0, 0, 4, H), 0, GCornerNone);
  }

  TimerState *tm = data_timer();
  bool running = tm->status == TIMER_RUNNING;

  GFont f_label = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f_name  = fonts_get_system_font(wide ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  GFont f_big   = fonts_get_system_font(wide ? FONT_KEY_LECO_38_BOLD_NUMBERS
                                             : FONT_KEY_LECO_32_BOLD_NUMBERS);
  GFont f_proj  = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Status label (top).
  const char *label = running ? i18n(STR_HERO_RUNNING)
                     : (data_last_activity_id() > 0 ? i18n(STR_HERO_LAST) : i18n(STR_HERO_READY));
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, label, f_label, GRect(lx, 6, lw, 16),
                     GTextOverflowModeTrailingEllipsis, al, NULL);

  // Headline: the running activity, or the last activity, or a prompt.
  const char *name = running ? tm->act_name
                   : (data_last_activity_id() > 0 ? data_last_activity_name() : "");
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, name, f_name, GRect(lx, 22, lw, 34),
                     GTextOverflowModeTrailingEllipsis, al, NULL);

  if (running) {
    // Big ticking elapsed time (numeric font: digits/colon only).
    char elapsed[16];
    data_timer_elapsed_str(elapsed, sizeof(elapsed));
    graphics_draw_text(ctx, elapsed, f_big, GRect(lx, wide ? 64 : 54, lw, wide ? 48 : 40),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  } else {
    // Idle call to action.
    const char *cta = data_last_activity_id() > 0 ? i18n(STR_START_AGAIN)
                                                  : i18n(STR_CHOOSE_ACTIVITY);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, cta, f_name, GRect(lx, wide ? 64 : 56, lw, 32),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  }

  // Project + customer (bottom).
  const char *proj = data_active_project_name();
  const char *cust = data_active_project_customer();
  if (proj[0]) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, proj, f_proj, GRect(lx, H - (cust[0] ? 46 : 30), lw, 22),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
    if (cust[0]) {
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_draw_text(ctx, cust, f_label, GRect(lx, H - 24, lw, 18),
                         GTextOverflowModeTrailingEllipsis, al, NULL);
    }
  }
}

// --- Input -------------------------------------------------------------------

static void go_to_list(void) {
  window_stack_push(activity_list_window(), true);
}

static void select_click(ClickRecognizerRef recognizer, void *ctx) {
  if (data_timer()->status == TIMER_RUNNING) {
    go_to_list();                    // running: jump in to switch/stop
  } else if (data_last_activity_id() > 0) {
    // Idle with a known last activity: start it again straight away.
    Activity a = { .id = data_last_activity_id() };
    strncpy(a.name, data_last_activity_name(), ACT_NAME_LEN - 1);
    a.name[ACT_NAME_LEN - 1] = '\0';
    timer_start(&a);
  } else {
    go_to_list();                    // nothing to repeat: open the list
  }
}

static void select_long_click(ClickRecognizerRef recognizer, void *ctx) {
  if (data_timer()->status == TIMER_RUNNING) {
    timer_open_stop_menu();
  } else {
    timer_open_long_press_menu();
  }
}

static void down_click(ClickRecognizerRef recognizer, void *ctx) {
  go_to_list();
}

static void up_click(ClickRecognizerRef recognizer, void *ctx) {
  if (has_project_row()) { project_picker_push_projects(); }
}

static void up_long_click(ClickRecognizerRef recognizer, void *ctx) {
  if (has_project_row()) { project_picker_push_customers(); }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_long_click_subscribe(BUTTON_ID_UP, 0, up_long_click, NULL);
}

// Touch (Pebble Time 2): tap the card to open the activity list; swipe right to
// go back (exits the app, since the hero is the root).
static int16_t s_tx0, s_ty0;

static void touch_handler(const TouchEvent *event, void *context) {
  if (!config_touch_enabled()) return;
  if (event->type == TouchEvent_Touchdown) {
    s_tx0 = event->x; s_ty0 = event->y;
  } else if (event->type == TouchEvent_Liftoff) {
    int adx = abs(event->x - s_tx0), ady = abs(event->y - s_ty0);
    if (adx >= TOUCH_SWIPE_MIN && adx > ady) {
      if (event->x > s_tx0) { window_stack_pop(true); }   // swipe right = back (exit)
    } else if (adx <= TOUCH_TAP_MAX && ady <= TOUCH_TAP_MAX) {
      go_to_list();                                        // tap = open the list
    }
  }
}

// --- Second tick (elapsed repaint) -------------------------------------------

static void second_tick(void *data) {
  s_tick = app_timer_register(1000, second_tick, NULL);
  if (data_timer()->status == TIMER_RUNNING) {
    if (s_body) { layer_mark_dirty(s_body); }
    header_bar_refresh();
  }
}

// --- Window lifecycle --------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_body = layer_create(GRect(0, HEADER_BAR_HEIGHT, b.size.w, b.size.h - HEADER_BAR_HEIGHT));
  layer_set_update_proc(s_body, draw_card);
  layer_add_child(root, s_body);

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);

  window_set_click_config_provider(window, click_config);
}

static void window_appear(Window *window) {
  header_bar_set_active(s_header, false);   // the card shows elapsed; header keeps the title
  if (!s_tick) { s_tick = app_timer_register(1000, second_tick, NULL); }
  if (touch_service_is_enabled() && config_touch_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
  if (s_body) { layer_mark_dirty(s_body); }
}

static void window_disappear(Window *window) {
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
  touch_service_unsubscribe();
}

static void window_unload(Window *window) {
  if (s_body) { layer_destroy(s_body); s_body = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
}

Window *hero_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorWhite);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .appear = window_appear,
      .disappear = window_disappear,
      .unload = window_unload,
    });
  }
  return s_window;
}

void hero_reload(void) {
  if (s_body) { layer_mark_dirty(s_body); }
  header_bar_refresh();
}

void hero_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
