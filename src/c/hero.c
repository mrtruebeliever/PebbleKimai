#include "hero.h"
#include "touch_nav.h"
#include "activity_list.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "project_picker.h"
#include "recents.h"
#include "theme.h"
#include "timer_actions.h"
#include <stdlib.h>

// Local touch-gesture thresholds (pixels): the hero is a card, not a MenuLayer,
// so the system touch-navigation bridge does not reach it — it stays on the raw
// TouchService, as before 4.33.

static Window   *s_window = NULL;
static Layer    *s_body = NULL;
static Layer    *s_header = NULL;
static AppTimer *s_tick = NULL;
static bool      s_shown = false;   // hero is the visible window

// Rajdhani (the Demi watchface display font) for the large hero text.
static GFont s_f_name   = NULL;   // activity name / CTA
static GFont s_f_big    = NULL;   // elapsed timer (wide screens)
static GFont s_f_big_sm = NULL;   // elapsed timer (narrow screens)

static GBitmap *s_ico_running = NULL;   // a timer is running
static GBitmap *s_ico_last = NULL;      // this is what ran last

// Card metrics, in body-layer coordinates.
#define HERO_ICON_SIZE  20
#define HERO_PROJ_Y     2    // project line: context above the activity
#define HERO_NAME_Y     26
#define HERO_NAME_LINE  26   // line spacing we draw the name at
#define HERO_NAME_GAP   8

// Vertical bands of the card, in body-layer coordinates, recorded on each draw
// so a tap acts on whatever it visually points at (see hero_handle_touch).
static int  s_band_proj = 0;
static int  s_band_name = 0;
static int  s_band_block = 0;
static GRect s_zone_icon;      // the status icon, bottom right

static GFont name_font(void) {
  return s_f_name ? s_f_name : fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}

// Rendered width of a string, measured without wrapping.
static int text_width(const char *s, GFont f) {
  return graphics_text_layout_get_content_size(
      s, f, GRect(0, 0, 1000, 40), GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
}

// Byte offset of the space to break a too-wide name at, or 0 when it fits on one
// line. Rajdhani carries a baked line height of ~82px — more than three times its
// glyphs — so the layout engine's own wrapping leaves a gaping hole between the
// lines. The name is split by hand instead and drawn as two single-line runs.
// Only ASCII spaces are considered, which never occur inside a UTF-8 sequence.
static int name_break(const char *s, GFont f, int max_w) {
  if (text_width(s, f) <= max_w) { return 0; }
  char buf[ACT_NAME_LEN];
  int best = 0;
  for (int i = 0; s[i] && i < (int)sizeof(buf); i++) {
    if (s[i] != ' ') { continue; }
    memcpy(buf, s, i);
    buf[i] = '\0';
    if (text_width(buf, f) > max_w) { break; }
    best = i;
  }
  return best;
}
static GFont big_font(bool wide) {
  GFont f = wide ? s_f_big : s_f_big_sm;
  if (f) return f;
  return fonts_get_system_font(wide ? FONT_KEY_LECO_38_BOLD_NUMBERS
                                    : FONT_KEY_LECO_32_BOLD_NUMBERS);
}

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
  GFont f_name  = name_font();
  GFont f_big   = big_font(wide);
  GFont f_proj  = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Status icon: a clock while a timer runs, a history arc when the card shows
  // what ran last. Nothing at all before anything has ever run — there is no
  // state to depict, and the call to action says what to do. It sits in the
  // bottom-right corner, which is otherwise empty, leaving the top of the card
  // for the project name.
  GBitmap *ico = running ? s_ico_running : (data_last_activity_id() > 0 ? s_ico_last : NULL);
  // Round: no corner to put it in, so it sits centred at the bottom, under
  // the customer.
  s_zone_icon = round ? GRect((b.size.w - HERO_ICON_SIZE) / 2, H - HERO_ICON_SIZE - 8,
                              HERO_ICON_SIZE, HERO_ICON_SIZE)
                      : GRect(b.size.w - HERO_ICON_SIZE - 6, H - HERO_ICON_SIZE - 4,
                              HERO_ICON_SIZE, HERO_ICON_SIZE);
  if (ico) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, ico, s_zone_icon);
  }
  // Bottom lines stay clear of it.
  int bw = (ico && !round) ? lw - (HERO_ICON_SIZE + 10) : lw;
  int bx = lx;
  int icon_h = (round && ico) ? HERO_ICON_SIZE + 10 : 0;
  if (round) { bw = 150; bx = (b.size.w - bw) / 2; }   // inside the circle's lower chord

  // Project the card is about, as small context directly above the activity:
  // while a timer runs that is the project the timesheet is booked on, which is
  // not necessarily the one selected on the watch.
  const char *proj = data_active_project_name();
  const char *cust = data_active_project_customer();
  const char *next = NULL;
  if (running && tm->proj_id > 0) {
    Project *p = data_project_by_id(tm->proj_id);
    if (p) {
      // A project picked mid-run only applies to the next entry; showing it as
      // a separate line keeps that from happening invisibly.
      if (data_active_project_id() != tm->proj_id && proj[0]) { next = proj; }
      proj = p->name;
      cust = p->customer;
    }
  }
  if (proj[0]) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_draw_text(ctx, proj, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(lx, HERO_PROJ_Y, lw, 22),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  }

  // Headline: the running activity, or the last activity, or a prompt. Long
  // names wrap onto a second line where the screen is tall enough for one.
  const char *name = running ? tm->act_name
                   : (data_last_activity_id() > 0 ? data_last_activity_name() : "");
  bool tall = H >= 180;
  int name_max_h = HERO_NAME_LINE * (tall ? 2 : 1);
  int name_h = HERO_NAME_LINE;
  if (name[0]) {
    graphics_context_set_text_color(ctx, GColorBlack);
    int cut = tall ? name_break(name, f_name, lw) : 0;
    if (cut > 0) {
      char first[ACT_NAME_LEN];
      memcpy(first, name, cut);
      first[cut] = '\0';
      graphics_draw_text(ctx, first, f_name, GRect(lx, HERO_NAME_Y, lw, HERO_NAME_LINE),
                         GTextOverflowModeTrailingEllipsis, al, NULL);
      graphics_draw_text(ctx, name + cut + 1, f_name,
                         GRect(lx, HERO_NAME_Y + HERO_NAME_LINE, lw, HERO_NAME_LINE),
                         GTextOverflowModeTrailingEllipsis, al, NULL);
      name_h = HERO_NAME_LINE * 2;
    } else {
      graphics_draw_text(ctx, name, f_name, GRect(lx, HERO_NAME_Y, lw, HERO_NAME_LINE),
                         GTextOverflowModeTrailingEllipsis, al, NULL);
    }
  }

  // Bottom lines: the customer in bold, and the pending project when it differs.
  // Short screens drop the customer — the project/activity pair above earns the
  // room more than a name that rarely changes.
  bool show_cust = tall && cust[0];
  int next_h = next ? 20 : 0;
  int cust_h = show_cust ? 24 : 0;
  int bottom = H - icon_h - next_h - cust_h - 4;   // a little air above the bottom lines

  int block_h = running ? (wide ? 48 : 40) : name_max_h;
  int block_y = HERO_NAME_Y + name_h + HERO_NAME_GAP;
  if (block_y + block_h > bottom) { block_y = bottom - block_h; }
  if (block_y < HERO_NAME_Y + name_h) { block_y = HERO_NAME_Y + name_h; }

  graphics_context_set_text_color(ctx, GColorBlack);
  if (running) {
    // Big ticking elapsed time (numeric font: digits/colon only).
    char elapsed[16];
    data_timer_elapsed_str(elapsed, sizeof(elapsed));
    graphics_draw_text(ctx, elapsed, f_big, GRect(lx, block_y, lw, block_h),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  } else {
    // Idle call to action.
    const char *cta = data_last_activity_id() > 0 ? i18n(STR_START_AGAIN)
                                                  : i18n(STR_CHOOSE_ACTIVITY);
    graphics_draw_text(ctx, cta, f_name, GRect(lx, block_y, lw, block_h),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  }

  if (show_cust) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, cust, f_proj, GRect(bx, H - icon_h - next_h - 24, bw, 22),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  }
  if (next) {
    char line[PROJ_NAME_LEN + 16];
    snprintf(line, sizeof(line), "%s: %s", i18n(STR_HERO_NEXT), next);
    graphics_context_set_text_color(ctx, accent);
    graphics_draw_text(ctx, line, f_label, GRect(bx, H - icon_h - 20, bw, 18),
                       GTextOverflowModeTrailingEllipsis, al, NULL);
  }

  // Hand the touch handler the bands this draw produced.
  s_band_proj  = HERO_NAME_Y - 2;
  s_band_name  = block_y;
  s_band_block = block_y + block_h;
}

// --- Input -------------------------------------------------------------------

static void go_to_list(void) {
  window_stack_push(activity_list_window(), true);
}

// Restart the last activity (the idle call to action).
static void start_again(void) {
  if (data_last_activity_id() <= 0) { go_to_list(); return; }
  Activity a = { .id = data_last_activity_id() };
  strncpy(a.name, data_last_activity_name(), ACT_NAME_LEN - 1);
  a.name[ACT_NAME_LEN - 1] = '\0';
  timer_start(&a);
}

static void select_click(ClickRecognizerRef recognizer, void *ctx) {
  if (data_timer()->status == TIMER_RUNNING) {
    go_to_list();                    // running: jump in to switch/stop
  } else {
    start_again();                   // idle: repeat, or open the list
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

// Long Down: the recent activities, across every project.
static void down_long_click(ClickRecognizerRef recognizer, void *ctx) {
  recents_push();
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
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, down_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_long_click_subscribe(BUTTON_ID_UP, 0, up_long_click, NULL);
}

// Touch (Pebble Time 2): tap the card to open the activity list. Gesture
// decoding lives in touch_nav.h, shared with the list windows, and the app-wide
// subscription in pebblekimai.c forwards the events here.

bool hero_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void hero_handle_touch(const TouchEvent *event) {
  GPoint point;
  switch (touch_nav_feed(NULL, event, &point)) {
    case TOUCH_NAV_TAP: {
      // Zones are in body-layer coordinates; touch events are in screen ones.
      GPoint p = GPoint(point.x, point.y - HEADER_BAR_HEIGHT);
      bool running = data_timer()->status == TIMER_RUNNING;
      // A little slack around the icon: 20px is a small target for a finger.
      GRect icon = grect_inset(s_zone_icon, GEdgeInsets(-8));
      if (grect_contains_point(&icon, &p) && (running || data_last_activity_id() > 0)) {
        recents_push();                       // status icon -> recent activities
      } else if (p.y < s_band_proj && has_project_row()) {
        project_picker_push_projects();       // project line -> project picker
      } else if (p.y >= s_band_name && p.y < s_band_block) {
        // The elapsed time is the timer: tapping it offers to stop it, through
        // the confirmation menu so a stray tap cannot end a running entry.
        if (running) { timer_open_stop_menu(); } else { start_again(); }
      } else {
        go_to_list();
      }
      break;
    }
    case TOUCH_NAV_BACK: window_stack_pop(true); break;   // leaves the app, as BACK does
    default: break;
  }
}

// --- Second tick (elapsed repaint) -------------------------------------------
// Only runs while a timer is actually running and the hero is on screen, so an
// idle open app does not wake the CPU once a second for nothing.

static void second_tick(void *data) {
  s_tick = NULL;
  if (!s_shown || data_timer()->status != TIMER_RUNNING) { return; }
  if (s_body) { layer_mark_dirty(s_body); }
  header_bar_refresh();
  s_tick = app_timer_register(1000, second_tick, NULL);
}

// Starts the ticker if a timer is running and the hero is visible (idempotent).
static void ensure_tick(void) {
  if (s_shown && !s_tick && data_timer()->status == TIMER_RUNNING) {
    s_tick = app_timer_register(1000, second_tick, NULL);
  }
}

// --- Window lifecycle --------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_f_name   = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RAJDHANI_24));
  s_f_big    = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RAJDHANI_46));
  s_f_big_sm = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RAJDHANI_34));
  s_ico_running = gbitmap_create_with_resource(RESOURCE_ID_IMG_STATUS_RUNNING);
  s_ico_last    = gbitmap_create_with_resource(RESOURCE_ID_IMG_STATUS_LAST);

  s_body = layer_create(GRect(0, HEADER_BAR_HEIGHT, b.size.w, b.size.h - HEADER_BAR_HEIGHT));
  layer_set_update_proc(s_body, draw_card);
  layer_add_child(root, s_body);

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);

  window_set_click_config_provider(window, click_config);
  (void)window_set_touch_bridge_disabled(window, true);   // a card, not a menu: own tap zones
}

static void window_appear(Window *window) {
  header_bar_set_active(s_header, false);   // the card shows elapsed; header keeps the title
  s_shown = true;
  ensure_tick();
  if (s_body) { layer_mark_dirty(s_body); }
}

static void window_disappear(Window *window) {
  s_shown = false;
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
}

static void window_unload(Window *window) {
  if (s_body) { layer_destroy(s_body); s_body = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
  if (s_f_name)   { fonts_unload_custom_font(s_f_name);   s_f_name = NULL; }
  if (s_f_big)    { fonts_unload_custom_font(s_f_big);    s_f_big = NULL; }
  if (s_f_big_sm) { fonts_unload_custom_font(s_f_big_sm); s_f_big_sm = NULL; }
  if (s_ico_running) { gbitmap_destroy(s_ico_running); s_ico_running = NULL; }
  if (s_ico_last)    { gbitmap_destroy(s_ico_last);    s_ico_last = NULL; }
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
  ensure_tick();   // a timer may have just started while the hero is up
}

void hero_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
