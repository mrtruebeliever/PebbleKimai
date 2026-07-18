#include "toast.h"

#define TOAST_MS 2000

static Window    *s_window = NULL;
static TextLayer *s_text = NULL;
static AppTimer  *s_timer = NULL;
static char       s_buf[64];

static void dismiss(void *data) {
  s_timer = NULL;
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_text = text_layer_create(GRect(8, b.size.h / 2 - 30, b.size.w - 16, 60));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_text(s_text, s_buf);
  layer_add_child(root, text_layer_get_layer(s_text));
}

static void window_unload(Window *window) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  text_layer_destroy(s_text);
  s_text = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void toast_show(const char *text) {
  strncpy(s_buf, text ? text : "", sizeof(s_buf) - 1);
  s_buf[sizeof(s_buf) - 1] = '\0';

  if (s_window) {  // already showing: just update text + restart the timer
    if (s_text) { text_layer_set_text(s_text, s_buf); }
    if (s_timer) { app_timer_reschedule(s_timer, TOAST_MS); }
    return;
  }
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  vibes_short_pulse();
  s_timer = app_timer_register(TOAST_MS, dismiss, NULL);
}
