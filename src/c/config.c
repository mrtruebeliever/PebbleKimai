#include "config.h"
#include "data.h"

static void (*s_change_cb)(void) = NULL;
static void (*s_error_cb)(int) = NULL;

void config_set_change_callback(void (*cb)(void)) { s_change_cb = cb; }
void config_set_error_callback(void (*cb)(int))   { s_error_cb = cb; }

// Decode an integer tuple by its actual type and width. The real phone app
// packs integers at their smallest width (a 1 arrives as a 1-byte tuple), so
// reading value->int32 blindly picks up neighbouring dict bytes; the emulator
// always sends 4 bytes and never shows the bug. Clay may also send strings.
static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_CSTRING: return (int32_t)atoi(t->value->cstring);
    case TUPLE_INT:  if (t->length == 1) return t->value->int8;
                     if (t->length == 2) return t->value->int16;
                     return t->value->int32;
    case TUPLE_UINT: if (t->length == 1) return t->value->uint8;
                     if (t->length == 2) return t->value->uint16;
                     return (int32_t)t->value->uint32;
    default: return 0;
  }
}

// Copies one activity's fields out of a per-activity AppMessage.
static void apply_activity(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_ACT_INDEX);
  if (!t_idx) return;
  int idx = tuple_int(t_idx);
  Activity *a = data_activity(idx);
  if (!a) return;

  if (idx >= data_activity_count()) {
    data_set_activity_count(idx + 1);  // reveal rows progressively
  }

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_ACT_ID))) {
    a->id = tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ACT_NAME))) {
    strncpy(a->name, t->value->cstring, ACT_NAME_LEN - 1);
    a->name[ACT_NAME_LEN - 1] = '\0';
  }
}

// Applies an authoritative timer-state message from the phone. Any confirmed
// state (idle or running) clears the optimistic `pending` flag.
static void apply_timer(DictionaryIterator *iter) {
  Tuple *t_status = dict_find(iter, MESSAGE_KEY_TIMER_STATUS);
  if (!t_status) return;
  TimerState *tm = data_timer();

  tm->status = tuple_int(t_status);
  tm->pending = false;
  tm->ts_id = 0;
  tm->act_id = 0;
  tm->begin = 0;
  tm->act_name[0] = '\0';

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_TS_ID)))  tm->ts_id  = tuple_int(t);
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_ACT_ID))) tm->act_id = tuple_int(t);
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_BEGIN)))  tm->begin  = (time_t)tuple_int(t);
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_ACT_NAME))) {
    strncpy(tm->act_name, t->value->cstring, ACT_NAME_LEN - 1);
    tm->act_name[ACT_NAME_LEN - 1] = '\0';
  }
}

void config_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_LOAD_STATE))) {
    data_set_load_state(tuple_int(t));
  }

  // Head message resets the list; per-activity items arrive in later messages.
  if ((t = dict_find(iter, MESSAGE_KEY_ACT_COUNT))) {
    data_clear_activities();
  }
  apply_activity(iter);
  apply_timer(iter);

  if ((t = dict_find(iter, MESSAGE_KEY_ERROR_CODE)) && s_error_cb) {
    s_error_cb(tuple_int(t));
  }

  if (s_change_cb) {
    s_change_cb();
  }
}

// --- Outbox helpers ----------------------------------------------------------

static void send_flag(uint32_t key) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, key, &one, sizeof(int), true);
  app_message_outbox_send();
}

static void send_int(uint32_t key, int value) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int(out, key, &value, sizeof(int), true);
  app_message_outbox_send();
}

void config_request_refresh(void)         { send_flag(MESSAGE_KEY_REQUEST_ACTIVITIES); }
void config_start_activity(int act_id)    { send_int(MESSAGE_KEY_START_ACTIVITY, act_id); }
void config_stop_activity(void)           { send_flag(MESSAGE_KEY_STOP_ACTIVITY); }
void config_switch_activity(int act_id)   { send_int(MESSAGE_KEY_SWITCH_ACTIVITY, act_id); }
