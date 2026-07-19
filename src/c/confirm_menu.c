#include "confirm_menu.h"
#include "config.h"
#include "theme.h"
#include "touch_nav.h"
#include <stdlib.h>

#define MAX_ITEMS 3
#define ROW_H     50

static Window     *s_window = NULL;
static MenuLayer  *s_menu = NULL;
static ConfirmItem s_items[MAX_ITEMS];
static int         s_count = 0;
static GRect       s_row_rect[MAX_ITEMS];
static int16_t     s_tx0, s_ty0, s_last_y;
static bool        s_scrolled;

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
  if (idx->row < MAX_ITEMS) {
    s_row_rect[idx->row] = layer_convert_rect_to_screen((Layer *)cell, layer_get_bounds(cell));
  }
  GRect b = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_items[idx->row].label,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(10, (b.size.h - 28) / 2, b.size.w - 16, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  choose(idx->row);
}

// --- Touch -------------------------------------------------------------------

static void tap(int16_t x, int16_t y) {
  GPoint p = GPoint(x, y);
  for (int r = 0; r < s_count && r < MAX_ITEMS; r++) {
    if (grect_contains_point(&s_row_rect[r], &p)) {
      menu_layer_set_selected_index(s_menu, (MenuIndex){ .section = 0, .row = r },
                                    MenuRowAlignNone, false);
      choose(r);
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
      if (abs(event->x - s_tx0) > abs(event->y - s_ty0)) break;
      int dy = event->y - s_last_y;
      while (dy >= TOUCH_SCROLL_STEP) {
        menu_layer_set_selected_next(s_menu, true, MenuRowAlignCenter, false);
        s_last_y += TOUCH_SCROLL_STEP; dy -= TOUCH_SCROLL_STEP; s_scrolled = true;
      }
      while (dy <= -TOUCH_SCROLL_STEP) {
        menu_layer_set_selected_next(s_menu, false, MenuRowAlignCenter, false);
        s_last_y -= TOUCH_SCROLL_STEP; dy += TOUCH_SCROLL_STEP; s_scrolled = true;
      }
      break;
    }
    case TouchEvent_Liftoff: {
      if (s_scrolled) break;
      int adx = abs(event->x - s_tx0), ady = abs(event->y - s_ty0);
      if (adx >= TOUCH_SWIPE_MIN && adx > ady) {
        if (event->x > s_tx0) { window_stack_pop(true); }   // swipe right = cancel
      } else if (adx <= TOUCH_TAP_MAX && ady <= TOUCH_TAP_MAX) {
        tap(event->x, event->y);
      }
      break;
    }
  }
}

// --- Window ------------------------------------------------------------------

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
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_appear(Window *window) {
  if (touch_service_is_enabled() && config_touch_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
}

static void window_disappear(Window *window) {
  touch_service_unsubscribe();
}

static void window_unload(Window *window) {
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
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
