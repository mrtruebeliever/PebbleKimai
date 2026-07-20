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

#define ROW_H         40
#define ROW_RUNNING_H 58
#define ROW_STATUS_H  44
#define ROW_PROJECT_H 44
#define ROW_RECT_MAX  40          // touch hit-test slots (rows 0..N)

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static AppTimer  *s_tick = NULL;              // 1 s repaint while a timer runs
static bool       s_shown = false;            // the list is the visible window
static GRect      s_row_rect[ROW_RECT_MAX];   // last on-screen rect per row (touch)

// The list is drawable once loaded, or while a (re)load streams rows in.
static bool list_ready(void) {
  int st = data_load_state();
  return st == LOAD_OK || (st == LOAD_LOADING && data_activity_count() > 0);
}

// Row 0 shows the active project (tap = pick project, long-press = pick customer)
// once there is a project to show. Activity rows are then offset by one.
static bool has_project_row(void) {
  return data_project_count() > 0 || data_active_project_name()[0];
}
static int act_offset(void) { return has_project_row() ? 1 : 0; }

// True once at least one loaded activity is bound to the active project. Used
// to dim the remaining global activities so the project's own ones stand out
// as the primary group — but only once there is a primary group to contrast
// against; an all-global list stays full color.
static bool any_project_bound(void) {
  for (int i = 0; i < data_activity_count(); i++) {
    Activity *a = data_activity(i);
    if (a && a->project_id != 0) return true;
  }
  return false;
}

// --- MenuLayer callbacks ------------------------------------------------------

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  int acts = list_ready() && data_activity_count() > 0 ? data_activity_count() : 1;
  return act_offset() + acts;
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *idx, void *context) {
  if (has_project_row() && idx->row == 0) return ROW_PROJECT_H;
  int ai = idx->row - act_offset();
  if (!list_ready() || data_activity_count() == 0) return ROW_STATUS_H;
  Activity *a = data_activity(ai);
  TimerState *tm = data_timer();
  bool running = a && tm->status == TIMER_RUNNING && tm->act_id == a->id;
  return running ? ROW_RUNNING_H : ROW_H;
}

static const char *status_text(void) {
  switch (data_load_state()) {
    case LOAD_LOADING:      return i18n(STR_LOADING);
    case LOAD_ERROR:        return i18n(STR_NO_CONNECTION);
    case LOAD_UNCONFIGURED: return i18n(STR_SETUP);
    default:                return i18n(STR_NO_ACTIVITIES);
  }
}

// The active-project row: project name (bold) with the customer as a sublabel,
// and an accent stripe down the left edge to mark it as the active context.
static void draw_project_row(GContext *ctx, const Layer *cell) {
  GRect b = layer_get_bounds(cell);
  bool selected = menu_layer_get_selected_index(s_menu).row == 0;
  const char *name = data_active_project_name();
  const char *cust = data_active_project_customer();

  graphics_context_set_fill_color(ctx, selected ? GColorBlack : theme_accent());
  graphics_fill_rect(ctx, GRect(0, 0, 4, b.size.h), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, name[0] ? name : i18n(STR_NO_PROJECT_SET),
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(10, cust[0] ? 0 : 6, b.size.w - 14, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (cust[0]) {
    graphics_context_set_text_color(ctx, selected ? GColorBlack : GColorDarkGray);
    graphics_draw_text(ctx, cust, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(10, 25, b.size.w - 14, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *context) {
  GRect b = layer_get_bounds(cell);

  // Remember this row's on-screen rectangle so a touch can hit-test it.
  if (idx->row < ROW_RECT_MAX) {
    s_row_rect[idx->row] = layer_convert_rect_to_screen((Layer *)cell, b);
  }

  if (has_project_row() && idx->row == 0) {
    draw_project_row(ctx, cell);
    return;
  }
  int ai = idx->row - act_offset();

  if (!list_ready() || data_activity_count() == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, status_text(), fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(4, 2, b.size.w - 8, b.size.h - 4),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  Activity *a = data_activity(ai);
  if (!a) return;
  TimerState *tm = data_timer();
  bool running = tm->status == TIMER_RUNNING && tm->act_id == a->id;
  bool selected = menu_layer_get_selected_index(s_menu).row == idx->row;
  bool dim = !running && a->project_id == 0 && any_project_bound();

  graphics_context_set_text_color(ctx, dim ? GColorDarkGray : GColorBlack);
  graphics_draw_text(ctx, a->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(10, running ? 2 : (b.size.h - 32) / 2, b.size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (running) {
    // Left edge marker + ticking elapsed line under the name.
    graphics_context_set_fill_color(ctx, selected ? GColorBlack : theme_accent());
    graphics_fill_rect(ctx, GRect(0, 0, 4, b.size.h), 0, GCornerNone);
    char elapsed[16];
    data_timer_elapsed_str(elapsed, sizeof(elapsed));
    static char line[24];
    snprintf(line, sizeof(line), "%s%s", elapsed, tm->pending ? " ..." : "");
    graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(10, 30, b.size.w - 16, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

// Acts on a row as if it was selected. Shared by the Select button and touch.
static void list_activate(int row) {
  if (has_project_row() && row == 0) {
    project_picker_push_projects();   // tap the project row = choose a project
    return;
  }
  int ai = row - act_offset();

  if (!list_ready() || data_activity_count() == 0) {
    // The status row doubles as a retry button (except while already loading).
    if (data_load_state() != LOAD_LOADING) { timer_refresh(); }
    return;
  }

  Activity *a = data_activity(ai);
  if (!a) return;
  TimerState *tm = data_timer();

  if (tm->status == TIMER_RUNNING) {
    if (tm->act_id == a->id) {
      timer_open_stop_menu();          // stopping needs a confirmation tap
    } else {
      timer_open_switch_menu(a);       // switching stops the running timer: confirm
    }
  } else {
    timer_start(a);                    // idle (or state still unknown): start directly
  }
}

static void select_click(MenuLayer *menu, MenuIndex *idx, void *context) {
  list_activate(idx->row);
}

static void select_long_click(MenuLayer *menu, MenuIndex *idx, void *context) {
  if (has_project_row() && idx->row == 0) {
    project_picker_push_customers();  // long-press the project row = choose a customer
    return;
  }
  timer_open_long_press_menu();
}

// --- Touch (Pebble Time 2) ----------------------------------------------------
// Tap a row to select+activate it; drag up/down to scroll; swipe right to go
// back. The physical buttons keep working as the always-available fallback.

static int16_t s_tx0, s_ty0;     // touchdown point
static int16_t s_last_y;         // last y a scroll step was taken at
static bool    s_scrolled;       // this gesture already scrolled (=> not a tap)

static void tap_row(int16_t x, int16_t y) {
  GPoint p = GPoint(x, y);
  // Ignore stored rects that fall outside the menu's current viewport: rows that
  // scrolled off keep a stale rect, and only the visible rows are redrawn.
  Layer *ml = menu_layer_get_layer(s_menu);
  GRect vis = layer_convert_rect_to_screen(ml, layer_get_bounds(ml));
  int vtop = vis.origin.y, vbot = vis.origin.y + vis.size.h;

  int n = get_num_rows(s_menu, 0, NULL);
  for (int r = 0; r < n && r < ROW_RECT_MAX; r++) {
    GRect rr = s_row_rect[r];
    int cy = rr.origin.y + rr.size.h / 2;
    if (cy < vtop || cy >= vbot) { continue; }
    if (grect_contains_point(&rr, &p)) {
      menu_layer_set_selected_index(s_menu, (MenuIndex){ .section = 0, .row = r },
                                    MenuRowAlignNone, false);
      list_activate(r);
      return;
    }
  }
}

static void touch_handler(const TouchEvent *event, void *context) {
  if (!s_menu || !config_touch_enabled()) return;

  switch (event->type) {
    case TouchEvent_Touchdown:
      s_tx0 = event->x; s_ty0 = event->y; s_last_y = event->y; s_scrolled = false;
      break;

    case TouchEvent_PositionUpdate: {
      // Scroll only while the drag stays mostly vertical (a horizontal drag is a
      // back-swipe, handled on liftoff). One selection step per row of travel.
      if (abs(event->x - s_tx0) > abs(event->y - s_ty0)) break;
      int dy = event->y - s_last_y;
      while (dy >= TOUCH_SCROLL_STEP) {          // finger down -> previous row
        menu_layer_set_selected_next(s_menu, true, MenuRowAlignCenter, false);
        s_last_y += TOUCH_SCROLL_STEP; dy -= TOUCH_SCROLL_STEP; s_scrolled = true;
      }
      while (dy <= -TOUCH_SCROLL_STEP) {         // finger up -> next row
        menu_layer_set_selected_next(s_menu, false, MenuRowAlignCenter, false);
        s_last_y -= TOUCH_SCROLL_STEP; dy += TOUCH_SCROLL_STEP; s_scrolled = true;
      }
      break;
    }

    case TouchEvent_Liftoff: {
      if (s_scrolled) break;                     // was a scroll drag: nothing to do
      int adx = abs(event->x - s_tx0), ady = abs(event->y - s_ty0);
      if (adx >= TOUCH_SWIPE_MIN && adx > ady) {
        if (event->x > s_tx0) { window_stack_pop(true); }   // swipe right = back
      } else if (adx <= TOUCH_TAP_MAX && ady <= TOUCH_TAP_MAX) {
        tap_row(event->x, event->y);             // tap = select + activate
      }
      break;
    }
  }
}

// --- Second tick (elapsed time repaint) --------------------------------------
// Runs only while a timer is running and the list is on screen, so an idle open
// list does not wake the CPU once a second for nothing.

static void second_tick(void *data) {
  s_tick = NULL;
  if (!s_shown || data_timer()->status != TIMER_RUNNING) { return; }
  layer_mark_dirty(menu_layer_get_layer(s_menu));
  header_bar_refresh();
  s_tick = app_timer_register(1000, second_tick, NULL);
}

static void ensure_tick(void) {
  if (s_shown && !s_tick && data_timer()->status == TIMER_RUNNING) {
    s_tick = app_timer_register(1000, second_tick, NULL);
  }
}

// --- Window lifecycle ---------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_menu = menu_layer_create(
      GRect(0, HEADER_BAR_HEIGHT, b.size.w, b.size.h - HEADER_BAR_HEIGHT));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
  });
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_appear(Window *window) {
  header_bar_set_active(s_header, true);   // list header shows the ticking elapsed
  s_shown = true;
  ensure_tick();
  if (touch_service_is_enabled() && config_touch_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
}

static void window_disappear(Window *window) {
  s_shown = false;
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
  touch_service_unsubscribe();
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  layer_destroy(s_header);
  s_header = NULL;
}

Window *activity_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .appear = window_appear,
      .disappear = window_disappear,
      .unload = window_unload,
    });
  }
  return s_window;
}

void activity_list_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
  header_bar_refresh();
  ensure_tick();   // a timer may have just started while the list is up
}

void activity_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
