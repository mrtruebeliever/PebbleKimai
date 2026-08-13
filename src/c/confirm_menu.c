#include "confirm_menu.h"
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
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
