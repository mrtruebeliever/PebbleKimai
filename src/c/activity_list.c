#include "activity_list.h"
#include "touch_nav.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "project_picker.h"
#include "theme.h"
#include "timer_actions.h"

#define ROW_H         34
#define ROW_RUNNING_H 48
#define ROW_STATUS_H  54   // two Gothic-18 lines: several status strings
                           // embed a newline and 44 clipped the second
#define ROW_PROJECT_H 40   // 32 without a customer line, see get_cell_height

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static AppTimer  *s_tick = NULL;              // 1 s repaint while a timer runs
static bool       s_shown = false;            // the list is the visible window

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
  if (has_project_row() && idx->row == 0) {
    return ((data_active_project_customer()[0] && data_customer_count() > 1) ? ROW_PROJECT_H : 32)
           + theme_name_grow();
  }
  int ai = idx->row - act_offset();
  if (!list_ready() || data_activity_count() == 0) return ROW_STATUS_H;
  Activity *a = data_activity(ai);
  TimerState *tm = data_timer();
  bool running = a && tm->status == TIMER_RUNNING && tm->act_id == a->id;
  return (running ? ROW_RUNNING_H : ROW_H) + theme_name_grow();
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
  GRect b = theme_row_bounds(cell);
  bool selected = menu_layer_get_selected_index(s_menu).row == 0;
  const char *name = data_active_project_name();
  const char *cust = data_active_project_customer();

  graphics_context_set_fill_color(ctx, theme_marker(selected));
  graphics_fill_rect(ctx, GRect(b.origin.x, 0, 4, b.size.h), 0, GCornerNone);

  // Customer on top in bold, project underneath: the shape the pickers use.
  // Only worth a line when the instance actually has more than one customer,
  // the same rule the project picker applies.
  bool show_cust = cust[0] && data_customer_count() > 1;
  if (show_cust) {
    graphics_context_set_text_color(ctx, theme_row_sub(selected));
    graphics_draw_text(ctx, cust, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(b.origin.x + 10, -1, b.size.w - 14, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  graphics_context_set_text_color(ctx, theme_row_fg(selected));
  graphics_draw_text(ctx, name[0] ? name : i18n(STR_NO_PROJECT_SET), theme_name_font(),
                     GRect(b.origin.x + 10, show_cust ? 17 : 5, b.size.w - 14, 22 + theme_name_grow()),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *context) {
  GRect b = theme_row_bounds(cell);

  if (has_project_row() && idx->row == 0) {
    draw_project_row(ctx, cell);
    return;
  }
  int ai = idx->row - act_offset();

  if (!list_ready() || data_activity_count() == 0) {
    graphics_context_set_text_color(ctx, theme_row_fg(menu_cell_layer_is_highlighted(cell)));
    graphics_draw_text(ctx, status_text(), fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(b.origin.x + 4, 2, b.size.w - 8, b.size.h - 4),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  Activity *a = data_activity(ai);
  if (!a) return;
  TimerState *tm = data_timer();
  bool running = tm->status == TIMER_RUNNING && tm->act_id == a->id;
  bool selected = menu_layer_get_selected_index(s_menu).row == idx->row;
  bool dim = !running && a->project_id == 0 && any_project_bound();

  // Never dim the selected row: DarkGray on the KellyGreen highlight is a
  // poor pair, and it lands on the row the user is about to act on.
  graphics_context_set_text_color(ctx, (dim && !selected) ? GColorDarkGray : theme_row_fg(selected));
  const int16_t name_h = 24 + theme_name_grow();
  graphics_draw_text(ctx, a->name, theme_name_font(),
                     GRect(b.origin.x + 10, running ? 2 : (b.size.h - name_h) / 2, b.size.w - 16, name_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (running) {
    // Left edge marker + ticking elapsed line under the name.
    graphics_context_set_fill_color(ctx, theme_marker(selected));
    graphics_fill_rect(ctx, GRect(b.origin.x, 0, 4, b.size.h), 0, GCornerNone);
    char elapsed[16];
    data_timer_elapsed_str(elapsed, sizeof(elapsed));
    static char line[24];
    snprintf(line, sizeof(line), "%s%s", elapsed, tm->pending ? " ..." : "");
    graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(b.origin.x + 10, name_h, b.size.w - 16, 22),
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
    // The status row doubles as a retry button, including while loading: if the
    // phone swallowed the first request, pressing the only row on screen should
    // re-issue it rather than do nothing. The watchdog in data.c covers the
    // case where the user does not press anything at all.
    timer_refresh();
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

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge (app_touch_navigation_enable) faults inside firmware
// on 4.33.1, so touch is handled from the raw stream — see touch_nav.h.

// Keeps the selection on a visible row after a drag or glide: SELECT would
// otherwise act on a row that scrolled out of sight, and the next UP/DOWN would
// yank the list back. The middle row is where button navigation puts it anyway.
static void touch_settled(void) {
  if (!s_menu) { return; }
  GRect frame = layer_get_frame(menu_layer_get_layer(s_menu));
  int row = touch_nav_row_at(s_menu, GPoint(frame.size.w / 2, frame.origin.y + frame.size.h / 2),
                             get_num_rows(s_menu, 0, NULL), get_cell_height);
  if (row >= 0) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
    layer_mark_dirty(menu_layer_get_layer(s_menu));   // no scroll means no repaint otherwise
  }
}

bool activity_list_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void activity_list_handle_touch(const TouchEvent *event) {
  if (!s_menu) { return; }
  GPoint point;
  switch (touch_nav_feed(menu_layer_get_scroll_layer(s_menu), event, &point)) {
    case TOUCH_NAV_TAP: {
      int row = touch_nav_row_at(s_menu, point, get_num_rows(s_menu, 0, NULL), get_cell_height);
      if (row < 0) { return; }
      MenuIndex index = MenuIndex(0, (uint16_t)row);
      menu_layer_set_selected_index(s_menu, index, MenuRowAlignNone, false);
      select_click(s_menu, &index, NULL);
      break;
    }
    case TOUCH_NAV_BACK:
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

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
  menu_layer_set_highlight_colors(s_menu, theme_highlight_bg(), theme_row_fg(true));
  menu_layer_set_click_config_onto_window(s_menu, window);
  // Let the system drive this menu when the bridge is available. (void): on
  // platforms without touch the SDK stubs this out as a bare (0).
  (void)window_set_touch_bridge_disabled(window, !touch_nav_bridge_active());
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_appear(Window *window) {
  touch_nav_set_settled_handler(touch_settled);
  header_bar_set_active(s_header, true);   // list header shows the ticking elapsed
  s_shown = true;
  ensure_tick();
}

static void window_disappear(Window *window) {
  touch_nav_reset();
  s_shown = false;
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
}

static void window_unload(Window *window) {
  touch_nav_reset();
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
