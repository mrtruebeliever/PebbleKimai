#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "header_bar.h"
#include "theme.h"

#define ROW_H         40
#define ROW_RUNNING_H 58
#define ROW_STATUS_H  44

static Window     *s_window = NULL;
static MenuLayer  *s_menu = NULL;
static Layer      *s_header = NULL;
static AppTimer   *s_tick = NULL;         // 1 s repaint while a timer runs
static ActionMenu *s_action_menu = NULL;
static int         s_target_act_id = 0;   // activity the open ActionMenu acts on

// The list is drawable once loaded, or while a (re)load streams rows in.
static bool list_ready(void) {
  int st = data_load_state();
  return st == LOAD_OK || (st == LOAD_LOADING && data_activity_count() > 0);
}

static Activity *activity_by_id(int id) {
  for (int i = 0; i < data_activity_count(); i++) {
    Activity *a = data_activity(i);
    if (a && a->id == id) return a;
  }
  return NULL;
}

// --- Optimistic mutations ----------------------------------------------------
// The UI flips immediately and the phone's authoritative TIMER_STATUS reply
// corrects it afterwards (config.c clears `pending` on every confirmed state).

static void begin_local_run(Activity *a) {
  TimerState *tm = data_timer();
  tm->status = TIMER_RUNNING;
  tm->ts_id = 0;
  tm->act_id = a->id;
  strncpy(tm->act_name, a->name, ACT_NAME_LEN - 1);
  tm->act_name[ACT_NAME_LEN - 1] = '\0';
  tm->begin = time(NULL);
  tm->pending = true;
}

static void do_start(Activity *a) {
  begin_local_run(a);
  config_start_activity(a->id);
  vibes_short_pulse();
  activity_list_reload();
}

static void do_switch(Activity *a) {
  begin_local_run(a);
  config_switch_activity(a->id);
  vibes_short_pulse();
  activity_list_reload();
}

static void do_stop(void) {
  TimerState *tm = data_timer();
  tm->status = TIMER_IDLE;
  tm->act_id = 0;
  tm->begin = 0;
  tm->pending = true;
  config_stop_activity();
  vibes_short_pulse();
  activity_list_reload();
}

// --- ActionMenu (confirmations) ----------------------------------------------

static void action_stop(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  do_stop();
}

static void action_switch(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  Activity *a = activity_by_id(s_target_act_id);
  if (a) { do_switch(a); }
}

static void action_refresh(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  data_set_load_state(LOAD_LOADING);
  config_request_refresh();
  activity_list_reload();
}

static void action_menu_closed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(menu), NULL, NULL);
  s_action_menu = NULL;
}

static void open_action_menu(ActionMenuLevel *root) {
  ActionMenuConfig cfg = (ActionMenuConfig) {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = action_menu_closed,
  };
  s_action_menu = action_menu_open(&cfg);
}

static void open_stop_menu(void) {
  ActionMenuLevel *root = action_menu_level_create(1);
  action_menu_level_add_action(root, "Stop timer", action_stop, NULL);
  open_action_menu(root);
}

static void open_switch_menu(Activity *a) {
  static char label[ACT_NAME_LEN + 12];
  snprintf(label, sizeof(label), "Switch to %s", a->name);
  s_target_act_id = a->id;
  ActionMenuLevel *root = action_menu_level_create(1);
  action_menu_level_add_action(root, label, action_switch, NULL);
  open_action_menu(root);
}

static void open_long_press_menu(void) {
  bool running = data_timer()->status == TIMER_RUNNING;
  ActionMenuLevel *root = action_menu_level_create(running ? 2 : 1);
  action_menu_level_add_action(root, "Refresh", action_refresh, NULL);
  if (running) {
    action_menu_level_add_action(root, "Stop timer", action_stop, NULL);
  }
  open_action_menu(root);
}

// --- MenuLayer callbacks ------------------------------------------------------

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  return list_ready() && data_activity_count() > 0 ? data_activity_count() : 1;
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *idx, void *context) {
  if (!list_ready() || data_activity_count() == 0) return ROW_STATUS_H;
  Activity *a = data_activity(idx->row);
  TimerState *tm = data_timer();
  bool running = a && tm->status == TIMER_RUNNING && tm->act_id == a->id;
  return running ? ROW_RUNNING_H : ROW_H;
}

static const char *status_text(void) {
  switch (data_load_state()) {
    case LOAD_LOADING:      return "Loading...";
    case LOAD_ERROR:        return "No connection";
    case LOAD_UNCONFIGURED: return "Set up in the\nphone app";
    default:                return "No activities\nin Kimai";
  }
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *context) {
  GRect b = layer_get_bounds(cell);

  if (!list_ready() || data_activity_count() == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, status_text(), fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(4, 2, b.size.w - 8, b.size.h - 4),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  Activity *a = data_activity(idx->row);
  if (!a) return;
  TimerState *tm = data_timer();
  bool running = tm->status == TIMER_RUNNING && tm->act_id == a->id;
  bool selected = menu_layer_get_selected_index(s_menu).row == idx->row;

  graphics_context_set_text_color(ctx, GColorBlack);
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

static void select_click(MenuLayer *menu, MenuIndex *idx, void *context) {
  if (!list_ready() || data_activity_count() == 0) {
    // The status row doubles as a retry button (except while already loading).
    if (data_load_state() != LOAD_LOADING) {
      data_set_load_state(LOAD_LOADING);
      config_request_refresh();
      activity_list_reload();
    }
    return;
  }

  Activity *a = data_activity(idx->row);
  if (!a) return;
  TimerState *tm = data_timer();

  if (tm->status == TIMER_RUNNING) {
    if (tm->act_id == a->id) {
      open_stop_menu();          // stopping needs a confirmation tap
    } else {
      open_switch_menu(a);       // switching stops the running timer: confirm
    }
  } else {
    do_start(a);                 // idle (or state still unknown): start directly
  }
}

static void select_long_click(MenuLayer *menu, MenuIndex *idx, void *context) {
  open_long_press_menu();
}

// --- Second tick (elapsed time repaint) --------------------------------------

static void second_tick(void *data) {
  s_tick = app_timer_register(1000, second_tick, NULL);
  if (data_timer()->status == TIMER_RUNNING) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
    header_bar_refresh();
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
  header_bar_set_active(s_header);
  if (!s_tick) {
    s_tick = app_timer_register(1000, second_tick, NULL);
  }
}

static void window_disappear(Window *window) {
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
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
}

void activity_list_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
