#pragma once
#include <pebble.h>

// AppMessage in/outbox for the Kimai companion protocol.

// Fired whenever incoming data changed (activities, load state, timer state).
void config_set_change_callback(void (*cb)(void));
// Fired when the phone reports an operation error (ERR_* code from data.h).
void config_set_error_callback(void (*cb)(int code));

void config_inbox_received(DictionaryIterator *iter, void *context);

// --- Outbox helpers ----------------------------------------------------------
void config_request_refresh(void);          // reload activities + timer state
void config_start_activity(int act_id);
void config_stop_activity(void);
void config_switch_activity(int act_id);    // stop whatever runs, then start act_id
