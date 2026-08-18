#include "recents.h"
#include "touch_nav.h"
#include "config.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "theme.h"
#include "timer_actions.h"

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;

static int row_count(void) {
  int n = data_recent_count();
  return n > 0 ? n : 1;                  // 1 = empty-state row
}

// Text for the single empty-state row. Mirrors project_picker.c: a phone that
// cannot be reached must not claim that nothing has run yet.
static const char *empty_text(void) {
  switch (data_recent_load_state()) {
    case LOAD_LOADING:      return i18n(STR_LOADING);
    case LOAD_ERROR:        return i18n(STR_NO_CONNECTION);
    case LOAD_UNCONFIGURED: return i18n(STR_SETUP);
    default: break;
  }
  return i18n(STR_NO_RECENTS);
}

// Performs a row's action (shared by the Select button and touch).
static void activate(int row) {
  if (data_recent_count() == 0) return;
  Recent *r = data_recent(row);
  if (!r || r->id <= 0) return;

  Activity a = { .id = r->id, .project_id = r->project_id };
  strncpy(a.name, r->name, ACT_NAME_LEN - 1);
  a.name[ACT_NAME_LEN - 1] = '\0';

  window_stack_pop(true);                // back to the hero either way
  if (data_timer()->status == TIMER_RUNNING) {
    timer_open_switch_menu_in(&a, r->project_id, r->project);   // never kill a timer silently
  } else {
    timer_start_in_project(&a, r->project_id, r->project);
  }
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  return row_count();
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 40;
}

// Project above the activity: first where it is booked, then what it is. The
// project carries the weight (bold) and the activity the size, so neither line
// wins outright — a row is identified by the pair, not by one of them.
static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  if (data_recent_count() == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, empty_text(), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, 10, b.size.w - 12, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }
  Recent *r = data_recent(ci->row);
  if (!r) return;

  if (r->project[0]) {
    graphics_context_set_text_color(ctx, hl ? GColorBlack
                                            : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_draw_text(ctx, r->project, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, -1, b.size.w - 12, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, r->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, r->project[0] ? 17 : 9, b.size.w - 12, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  activate(ci->row);
}

// --- Touch (see touch_nav.h; same handling as the project picker) ------------

static void touch_settled(void) {
  if (!s_menu) { return; }
  GRect frame = layer_get_frame(menu_layer_get_layer(s_menu));
  int row = touch_nav_row_at(s_menu, GPoint(frame.size.w / 2, frame.origin.y + frame.size.h / 2),
                             row_count(), get_cell_height);
  if (row >= 0) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

bool recents_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void recents_handle_touch(const TouchEvent *event) {
  if (!s_menu) { return; }
  GPoint point;
  switch (touch_nav_feed(menu_layer_get_scroll_layer(s_menu), event, &point)) {
    case TOUCH_NAV_TAP: {
      int row = touch_nav_row_at(s_menu, point, row_count(), get_cell_height);
      if (row < 0) { return; }
      menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
      activate(row);
      break;
    }
    case TOUCH_NAV_BACK:
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

// --- Window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_menu = menu_layer_create(
      GRect(0, HEADER_BAR_HEIGHT, b.size.w, b.size.h - HEADER_BAR_HEIGHT));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  // Let the system drive this menu when the bridge is available.
  window_set_touch_bridge_disabled(window, !touch_nav_bridge_active());
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_appear(Window *window) {
  touch_nav_set_settled_handler(touch_settled);
  header_bar_set_active(s_header, false);
  header_bar_set_title(i18n(STR_RECENT));
}

static void window_unload(Window *window) {
  touch_nav_reset();
  if (s_menu) { menu_layer_destroy(s_menu); s_menu = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

void recents_push(void) {
  data_set_recent_load_state(data_recent_count() > 0 ? LOAD_OK : LOAD_LOADING);
  config_request_recents();              // always refresh; the list shows what it has
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .appear = window_appear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void recents_reload(void) {
  if (!s_window || !s_menu) return;      // only when open
  menu_layer_reload_data(s_menu);
}
