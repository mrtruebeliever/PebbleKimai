#include <pebble.h>
#include "config.h"
#include "data.h"
#include "activity_list.h"
#include "confirm_menu.h"
#include "header_bar.h"
#include "hero.h"
#include "i18n.h"
#include "project_picker.h"
#include "toast.h"

static void on_data_changed(void) {
  hero_reload();
  activity_list_reload();
  project_picker_reload();
}

// Error codes mirror index.js; the phone follows every error with a fresh
// authoritative TIMER_STATUS, so the UI only needs to tell the user.
static void on_error(int code) {
  switch (code) {
    case ERR_NETWORK:    toast_show(i18n(STR_TOAST_NETWORK));    break;
    case ERR_AUTH:       toast_show(i18n(STR_TOAST_AUTH));       break;
    case ERR_CONFLICT:   toast_show(i18n(STR_TOAST_CONFLICT));   break;
    case ERR_NO_PROJECT: toast_show(i18n(STR_TOAST_NOPROJECT));  break;
    default:             toast_show(i18n(STR_TOAST_ERROR));      break;
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
  data_set_load_state(LOAD_ERROR);
  on_data_changed();
}

// One app-wide subscription powers the touch sensor; events go to the window on
// top. The system navigation bridge crashes the app on firmware 4.33.1 — see the
// note in init(). Only the Time 2 has a touchscreen; on the other platforms the
// touch API is a no-op macro, which would leave this handler unreferenced.
#ifdef PBL_PLATFORM_EMERY
static void touch_dispatch(const TouchEvent *event, void *context) {
  if (confirm_menu_is_top())        { confirm_menu_handle_touch(event); }
  else if (project_picker_is_top()) { project_picker_handle_touch(event); }
  else if (activity_list_is_top())  { activity_list_handle_touch(event); }
  else if (hero_is_top())           { hero_handle_touch(event); }
}
#endif

static void init(void) {
  config_load();            // language + persisted active project/customer
  header_bar_init();
  // No app_touch_navigation_enable() here: on firmware 4.33.1 the system touch
  // bridge faults inside firmware on the first touch and kills the app. Buttons
  // only until a later firmware fixes it.

  data_set_stall_callback(on_data_changed);
  config_set_change_callback(on_data_changed);
  config_set_error_callback(on_error);
  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(1024, 256);

  window_stack_push(hero_window(), true);
#ifdef PBL_PLATFORM_EMERY
  touch_service_subscribe(touch_dispatch, NULL);
#endif

  // Drive the phone from the watch's persisted context: load the project list
  // and the active project's activities. (The phone's 'ready' handler also does
  // an initial load as a fallback if these are dropped before JS starts.)
  config_request_projects();
  config_request_activities();
}

static void deinit(void) {
  activity_list_destroy();
  hero_destroy();
  header_bar_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
