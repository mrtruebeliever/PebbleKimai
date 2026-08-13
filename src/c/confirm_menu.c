#include "confirm_menu.h"
#include "touch_nav.h"
#include "theme.h"

#define MAX_ITEMS 3
#define ROW_H     50

static Window     *s_window = NULL;
static MenuLayer  *s_menu = NULL;
static ConfirmItem s_items[MAX_ITEMS];
static int         s_count = 0;

// Runs a chosen item: dismiss the menu first, then perform its action (so the
// action's redraw lands on the window underneath).
static void choose(int row) {
  if (row < 0 || row >= s_count) return;
  ConfirmAction cb = s_items[row].cb;
  window_stack_pop(true);
  if (cb) { cb(); }
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  return s_count;
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  return ROW_H;
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *c) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  // Two lines, wrapped. At Gothic 24 Bold one line cut off around 16 characters,
  // so DE "Wechseln zu %s" lost the activity name -- on a confirmation whose
  // whole job is to say what you are about to stop -- and the FR/ES "stop
  // timer" labels clipped before any name was even substituted.
  graphics_draw_text(ctx, s_items[idx->row].label,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(10, 4, b.size.w - 16, 42),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  choose(idx->row);
}

// --- Window ------------------------------------------------------------------

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

bool confirm_menu_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void confirm_menu_handle_touch(const TouchEvent *event) {
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

  // Vertically centre the (few) rows.
  int h = s_count * ROW_H;
  int top = (b.size.h - h) / 2;
  if (top < 0) top = 0;

  s_menu = menu_layer_create(GRect(0, top, b.size.w, b.size.h - top));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_accent(), GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  window_set_touch_bridge_disabled(window, true);
  touch_nav_set_settled_handler(touch_settled);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  touch_nav_reset();
  if (s_menu) { menu_layer_destroy(s_menu); s_menu = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

void confirm_menu_show(const ConfirmItem *items, int count) {
  if (count > MAX_ITEMS) count = MAX_ITEMS;
  s_count = count;
  for (int i = 0; i < count; i++) { s_items[i] = items[i]; }

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
