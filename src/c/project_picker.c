#include "project_picker.h"
#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "theme.h"
#include "touch_nav.h"
#include <stdlib.h>

typedef enum { MODE_PROJECTS, MODE_CUSTOMERS } PickMode;

static Window    *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Layer     *s_header = NULL;
static PickMode   s_mode = MODE_PROJECTS;

// The title bar reflects which list is showing (localized "Project"/"Customer").
static void refresh_title(void) {
  header_bar_set_title(i18n(s_mode == MODE_PROJECTS ? STR_PROJECT : STR_CUSTOMER));
}

// Rebuilt on each (re)load from the live project list.
static int  s_proj_idx[MAX_PROJECTS];               // data-project indices, filtered
static int  s_proj_n = 0;
static int  s_cust_id[MAX_PROJECTS];                 // distinct customer ids
static char s_cust_name[MAX_PROJECTS][CUST_NAME_LEN];
static int  s_cust_n = 0;

static GRect   s_row_rect[MAX_PROJECTS];             // on-screen rect per row (touch)
static int16_t s_tx0, s_ty0, s_last_y;
static bool    s_scrolled;

// Show a customer sublabel / offer the customer picker only when the instance
// actually has more than one customer.
static bool multi_customer(void) { return data_customer_count() > 1; }

static void rebuild_lists(void) {
  s_proj_n = 0;
  s_cust_n = 0;
  int filter = data_active_customer_id();
  bool multi = multi_customer();

  for (int i = 0; i < data_project_count(); i++) {
    Project *p = data_project(i);
    if (!p) continue;

    // Projects for the current customer (all of them when single-customer).
    if (!multi || filter == 0 || p->customer_id == filter) {
      if (s_proj_n < MAX_PROJECTS) { s_proj_idx[s_proj_n++] = i; }
    }

    // Distinct customers.
    bool seen = false;
    for (int j = 0; j < s_cust_n; j++) {
      if (s_cust_id[j] == p->customer_id) { seen = true; break; }
    }
    if (!seen && s_cust_n < MAX_PROJECTS) {
      s_cust_id[s_cust_n] = p->customer_id;
      strncpy(s_cust_name[s_cust_n], p->customer, CUST_NAME_LEN - 1);
      s_cust_name[s_cust_n][CUST_NAME_LEN - 1] = '\0';
      s_cust_n++;
    }
  }
}

// Text for the single empty-state row.
static const char *empty_text(void) {
  // Mirrors status_text() in activity_list.c. Without the error and
  // unconfigured cases, a phone that cannot be reached reported itself as
  // "No projects" -- stating as fact that the user's Kimai is empty, and
  // sending them off to fix the wrong thing.
  switch (data_project_load_state()) {
    case LOAD_LOADING:      return i18n(STR_LOADING);
    case LOAD_ERROR:        return i18n(STR_NO_CONNECTION);
    case LOAD_UNCONFIGURED: return i18n(STR_SETUP);
    default: break;
  }
  return (s_mode == MODE_PROJECTS) ? i18n(STR_NO_PROJECTS) : i18n(STR_NO_CUSTOMERS);
}

// Performs a row's action (shared by the Select button and touch).
static void activate(int row) {
  if (s_mode == MODE_PROJECTS) {
    if (s_proj_n == 0 || row < 0 || row >= s_proj_n) return;
    Project *p = data_project(s_proj_idx[row]);
    if (!p) return;
    config_set_active_project(p->id, p->name, p->customer_id, p->customer);
    config_request_activities();
    activity_list_reload();
    window_stack_pop(true);
  } else {
    if (s_cust_n == 0 || row < 0 || row >= s_cust_n) return;
    config_set_active_customer(s_cust_id[row], s_cust_name[row]);
    // Drop into the project list for the chosen customer.
    s_mode = MODE_PROJECTS;
    rebuild_lists();
    menu_layer_reload_data(s_menu);
    menu_layer_set_selected_index(s_menu, (MenuIndex){0, 0}, MenuRowAlignTop, false);
    refresh_title();
  }
}

// --- MenuLayer callbacks -----------------------------------------------------

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  int n = (s_mode == MODE_PROJECTS) ? s_proj_n : s_cust_n;
  return n > 0 ? n : 1;                              // 1 = empty-state row
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return 44;
}

static void draw_text_row(GContext *ctx, const Layer *cell, const char *title, const char *sub) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, (sub && sub[0]) ? 0 : 6, b.size.w - 12, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (sub && sub[0]) {
    graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorDarkGray);
    graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(8, 26, b.size.w - 12, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
  if (ci->row < MAX_PROJECTS) {
    s_row_rect[ci->row] = layer_convert_rect_to_screen((Layer *)cell, layer_get_bounds(cell));
  }
  if (s_mode == MODE_PROJECTS) {
    if (s_proj_n == 0) { draw_text_row(ctx, cell, empty_text(), ""); return; }
    Project *p = data_project(s_proj_idx[ci->row]);
    if (p) draw_text_row(ctx, cell, p->name, multi_customer() ? p->customer : "");
  } else {
    if (s_cust_n == 0) { draw_text_row(ctx, cell, empty_text(), ""); return; }
    draw_text_row(ctx, cell, s_cust_name[ci->row], "");
  }
}

static void select_click(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  activate(ci->row);
}

// --- Touch -------------------------------------------------------------------

static void tap(int16_t x, int16_t y) {
  GPoint p = GPoint(x, y);
  Layer *ml = menu_layer_get_layer(s_menu);
  GRect vis = layer_convert_rect_to_screen(ml, layer_get_bounds(ml));
  int vtop = vis.origin.y, vbot = vis.origin.y + vis.size.h;
  int n = get_num_rows(s_menu, 0, NULL);
  for (int r = 0; r < n && r < MAX_PROJECTS; r++) {
    GRect rr = s_row_rect[r];
    int cy = rr.origin.y + rr.size.h / 2;
    if (cy < vtop || cy >= vbot) { continue; }
    if (grect_contains_point(&rr, &p)) {
      menu_layer_set_selected_index(s_menu, (MenuIndex){ .section = 0, .row = r },
                                    MenuRowAlignNone, false);
      activate(r);
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
        if (event->x > s_tx0) { window_stack_pop(true); }   // swipe right = back
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
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_appear(Window *window) {
  header_bar_set_active(s_header, false);
  refresh_title();
  if (touch_service_is_enabled() && config_touch_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
}

static void window_disappear(Window *window) {
  touch_service_unsubscribe();
}

static void window_unload(Window *window) {
  if (s_menu) { menu_layer_destroy(s_menu); s_menu = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

static void push(PickMode mode) {
  s_mode = mode;
  rebuild_lists();
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void project_picker_push_projects(void)  { push(MODE_PROJECTS); }
void project_picker_push_customers(void) { push(MODE_CUSTOMERS); }

void project_picker_reload(void) {
  if (!s_window || !s_menu) return;   // only when open
  rebuild_lists();
  menu_layer_reload_data(s_menu);
}
