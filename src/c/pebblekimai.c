#include <pebble.h>
#include "config.h"
#include "data.h"
#include "activity_list.h"
#include "header_bar.h"
#include "toast.h"

static void on_data_changed(void) {
  activity_list_reload();
}

// Error codes mirror index.js; the phone follows every error with a fresh
// authoritative TIMER_STATUS, so the UI only needs to tell the user.
static void on_error(int code) {
  switch (code) {
    case ERR_NETWORK:    toast_show("No connection");        break;
    case ERR_AUTH:       toast_show("Check API token");      break;
    case ERR_CONFLICT:   toast_show("Timer conflict");       break;
    case ERR_NO_PROJECT: toast_show("No project in Kimai");  break;
    default:             toast_show("Kimai error");          break;
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

static void init(void) {
  header_bar_init();

  config_set_change_callback(on_data_changed);
  config_set_error_callback(on_error);
  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(1024, 256);

  window_stack_push(activity_list_window(), true);
}

static void deinit(void) {
  activity_list_destroy();
  header_bar_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
