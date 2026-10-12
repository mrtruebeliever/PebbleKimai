#include "project_picker.h"
#include "touch_nav.h"
#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "header_bar.h"
#include "i18n.h"
#include "theme.h"

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

// A row without a customer line above it needs a lot less height.
static int16_t get_cell_height(MenuLayer *ml, MenuIndex *ci, void *ctx) {
  return ((s_mode == MODE_PROJECTS && multi_customer()) ? 40 : 32) + theme_name_grow();
}

// Same shape as the recent list: the context line on top in bold, the row's own
// subject underneath at full size. Here the context is the customer, shown only
// when the instance has more than one.
static void draw_text_row(GContext *ctx, const Layer *cell, const char *title, const char *sub) {
  GRect b = theme_row_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  bool has_sub = sub && sub[0];
  if (has_sub) {
    graphics_context_set_text_color(ctx, theme_row_sub(hl));
    graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(b.origin.x + 8, -1, b.size.w - 12, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  graphics_context_set_text_color(ctx, theme_row_fg(hl));
  graphics_draw_text(ctx, title, theme_name_font(),
                     GRect(b.origin.x + 8, has_sub ? 17 : 5, b.size.w - 12, 22 + theme_name_grow()),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *ci, void *c) {
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

bool project_picker_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void project_picker_handle_touch(const TouchEvent *event) {
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
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, theme_highlight_bg(), theme_row_fg(true));
  menu_layer_set_click_config_onto_window(s_menu, window);
  // Let the system drive this menu when the bridge is available.
  (void)window_set_touch_bridge_disabled(window, !touch_nav_bridge_active());
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_header = header_bar_create(b.size.w);
  layer_add_child(root, s_header);
}

static void window_appear(Window *window) {
  touch_nav_set_settled_handler(touch_settled);
  header_bar_set_active(s_header, false);
  refresh_title();
}

static void window_unload(Window *window) {
  touch_nav_reset();
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
