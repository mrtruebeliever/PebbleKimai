#include "config.h"
#include "data.h"
#include "i18n.h"

static void (*s_change_cb)(void) = NULL;
static void (*s_error_cb)(int) = NULL;

static int  s_lang = LANG_EN;
static bool s_touch_enabled = true;

// Announced size of the project stream in flight, so the active project is
// reconciled exactly once when the whole list has arrived (-1 = no stream).
static int s_proj_total = -1;

void config_set_change_callback(void (*cb)(void)) { s_change_cb = cb; }
void config_set_error_callback(void (*cb)(int))   { s_error_cb = cb; }

int  config_lang(void)          { return s_lang; }
bool config_touch_enabled(void) { return s_touch_enabled; }

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

// Maps the watch's system locale (e.g. "en_US", "nl_NL") to a UI language.
static int detect_locale_lang(void) {
  const char *loc = i18n_get_system_locale();
  if (loc && loc[0] && loc[1]) {
    if (loc[0] == 'n' && loc[1] == 'l') return LANG_NL;
    if (loc[0] == 'f' && loc[1] == 'r') return LANG_FR;
    if (loc[0] == 'd' && loc[1] == 'e') return LANG_DE;
    if (loc[0] == 'e' && loc[1] == 's') return LANG_ES;
  }
  return LANG_EN;
}

// LANG_AUTO (or any out-of-range value) follows the system locale.
static int resolve_lang(int stored) {
  if (stored >= 0 && stored < LANG_COUNT) return stored;
  return detect_locale_lang();
}

void config_load(void) {
  int stored = persist_exists(PERSIST_LANG) ? persist_read_int(PERSIST_LANG) : LANG_AUTO;
  s_lang = resolve_lang(stored);

  s_touch_enabled = persist_exists(PERSIST_TOUCH) ? (persist_read_int(PERSIST_TOUCH) != 0) : true;

  int  ap_id = persist_exists(PERSIST_AP_ID) ? persist_read_int(PERSIST_AP_ID) : 0;
  int  ap_cid = persist_exists(PERSIST_AP_CUST_ID) ? persist_read_int(PERSIST_AP_CUST_ID) : 0;
  char ap_name[PROJ_NAME_LEN] = "";
  char ap_cust[CUST_NAME_LEN] = "";
  if (persist_exists(PERSIST_AP_NAME)) {
    persist_read_string(PERSIST_AP_NAME, ap_name, sizeof(ap_name));
  }
  if (persist_exists(PERSIST_AP_CUST_NAME)) {
    persist_read_string(PERSIST_AP_CUST_NAME, ap_cust, sizeof(ap_cust));
  }
  if (ap_id < 0) ap_id = 0;  // range-check the persisted path too
  data_set_active_project(ap_id, ap_name, ap_cid < 0 ? 0 : ap_cid, ap_cust);

  int  ac_id = persist_exists(PERSIST_AC_ID) ? persist_read_int(PERSIST_AC_ID) : 0;
  char ac_name[CUST_NAME_LEN] = "";
  if (persist_exists(PERSIST_AC_NAME)) {
    persist_read_string(PERSIST_AC_NAME, ac_name, sizeof(ac_name));
  }
  data_set_active_customer(ac_id < 0 ? 0 : ac_id, ac_name);

  int  la_id = persist_exists(PERSIST_LAST_ACT_ID) ? persist_read_int(PERSIST_LAST_ACT_ID) : 0;
  char la_name[ACT_NAME_LEN] = "";
  if (persist_exists(PERSIST_LAST_ACT_NAME)) {
    persist_read_string(PERSIST_LAST_ACT_NAME, la_name, sizeof(la_name));
  }
  data_set_last_activity(la_id < 0 ? 0 : la_id, la_name);
}

// Records the activity a timer is running on as the "last activity", persisted
// so the idle hero can offer to start it again. Called on every confirmed or
// optimistic RUNNING state.
void config_note_last_activity(int id, const char *name) {
  if (id <= 0 || !name || !name[0]) return;
  if (id == data_last_activity_id()) return;   // unchanged; skip the persist write
  data_set_last_activity(id, name);
  persist_write_int(PERSIST_LAST_ACT_ID, id);
  persist_write_string(PERSIST_LAST_ACT_NAME, name);
}

// --- Incoming: activity stream -----------------------------------------------

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
  if ((t = dict_find(iter, MESSAGE_KEY_ACT_PROJECT_ID))) {
    a->project_id = tuple_int(t);
  }
}

// --- Incoming: project stream ------------------------------------------------

static void apply_project(DictionaryIterator *iter) {
  Tuple *t_idx = dict_find(iter, MESSAGE_KEY_PROJ_INDEX);
  if (!t_idx) return;
  int idx = tuple_int(t_idx);
  Project *p = data_project(idx);
  if (!p) return;

  if (idx >= data_project_count()) {
    data_set_project_count(idx + 1);  // reveal rows progressively
  }

  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_ID)))          p->id = tuple_int(t);
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_CUSTOMER_ID))) p->customer_id = tuple_int(t);
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_NAME))) {
    strncpy(p->name, t->value->cstring, PROJ_NAME_LEN - 1);
    p->name[PROJ_NAME_LEN - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_CUSTOMER_NAME))) {
    strncpy(p->customer, t->value->cstring, CUST_NAME_LEN - 1);
    p->customer[CUST_NAME_LEN - 1] = '\0';
  }
}

// Once the whole project list has arrived, refresh the active project's cached
// name and customer from the authoritative list (the id is the source of truth;
// Clay's DEFAULT_PROJECT only carries an id). Only when the active project is
// genuinely absent (never set, or deleted server-side) do we adopt the first
// project. This MUST run only on a complete list: reconciling mid-stream would
// wrongly fall back to the first project before the active one has streamed in,
// clobbering the persisted choice.
static void reconcile_active_project(void) {
  if (data_project_count() == 0) return;

  int ap_id = data_active_project_id();
  Project *p = ap_id ? data_project_by_id(ap_id) : NULL;
  if (!p) { p = data_project(0); }   // unset or stale -> first project
  if (!p) return;

  bool changed = (p->id != ap_id);
  config_set_active_project(p->id, p->name, p->customer_id, p->customer);
  // If we just adopted a different project, its activities must be (re)loaded.
  if (changed) { config_request_activities(); }
}

// --- Incoming: timer state ---------------------------------------------------

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
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_BEGIN))) {
    tm->begin = (time_t)tuple_int(t);
    // Kimai rounds the begin down to the whole minute, and the server clock can
    // run slightly ahead, so the reported begin is sometimes in the future. Count
    // from now in that case instead of freezing the elapsed at 0:00:00.
    time_t now = time(NULL);
    if (tm->begin > now) { tm->begin = now; }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TIMER_ACT_NAME))) {
    strncpy(tm->act_name, t->value->cstring, ACT_NAME_LEN - 1);
    tm->act_name[ACT_NAME_LEN - 1] = '\0';
  }

  // Remember what ran, so the idle hero can offer to start it again.
  if (tm->status == TIMER_RUNNING) {
    config_note_last_activity(tm->act_id, tm->act_name);
  }
}

void config_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    int v = tuple_int(t);                 // may be LANG_AUTO
    persist_write_int(PERSIST_LANG, v);
    s_lang = resolve_lang(v);
  }

  if ((t = dict_find(iter, MESSAGE_KEY_TOUCH_ENABLED))) {
    s_touch_enabled = tuple_int(t) != 0;
    persist_write_int(PERSIST_TOUCH, s_touch_enabled ? 1 : 0);
  }

  // Clay's default project / customer arrive as ids only; names get resolved
  // from the project list once it is loaded (reconcile_active_project).
  if ((t = dict_find(iter, MESSAGE_KEY_DEFAULT_PROJECT))) {
    int id = tuple_int(t);
    if (id > 0) {
      Project *p = data_project_by_id(id);
      config_set_active_project(id, p ? p->name : "", p ? p->customer_id : 0,
                                p ? p->customer : "");
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DEFAULT_CUSTOMER))) {
    int id = tuple_int(t);
    if (id > 0) {
      Project *p = NULL;
      for (int i = 0; i < data_project_count(); i++) {
        Project *q = data_project(i);
        if (q && q->customer_id == id) { p = q; break; }
      }
      config_set_active_customer(id, p ? p->customer : "");
    }
  }

  if ((t = dict_find(iter, MESSAGE_KEY_LOAD_STATE))) {
    data_set_load_state(tuple_int(t));
  }

  // Head messages reset a list; per-record items arrive in later messages.
  if ((t = dict_find(iter, MESSAGE_KEY_ACT_COUNT))) {
    data_clear_activities();
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROJ_COUNT))) {
    data_clear_projects();
    data_set_project_load_state(LOAD_OK);
    s_proj_total = tuple_int(t);
  }

  apply_activity(iter);
  if (dict_find(iter, MESSAGE_KEY_PROJ_INDEX)) { apply_project(iter); }
  apply_timer(iter);

  // Reconcile the active project only once the whole list has arrived (see the
  // warning on reconcile_active_project). An empty list (total 0) completes here
  // at the head message.
  if (s_proj_total >= 0 && data_project_count() >= s_proj_total) {
    reconcile_active_project();
    s_proj_total = -1;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_ERROR_CODE)) && s_error_cb) {
    s_error_cb(tuple_int(t));
  }

  if (s_change_cb) {
    s_change_cb();
  }
}

// --- Active-context mutators -------------------------------------------------

void config_set_active_project(int id, const char *name, int customer_id, const char *customer) {
  data_set_active_project(id, name, customer_id, customer);
  persist_write_int(PERSIST_AP_ID, id);
  persist_write_string(PERSIST_AP_NAME, name ? name : "");
  persist_write_int(PERSIST_AP_CUST_ID, customer_id);
  persist_write_string(PERSIST_AP_CUST_NAME, customer ? customer : "");
  // The active project also drives the customer filter for the picker.
  data_set_active_customer(customer_id, customer);
  persist_write_int(PERSIST_AC_ID, customer_id);
  persist_write_string(PERSIST_AC_NAME, customer ? customer : "");
}

void config_set_active_customer(int id, const char *name) {
  data_set_active_customer(id, name);
  persist_write_int(PERSIST_AC_ID, id);
  persist_write_string(PERSIST_AC_NAME, name ? name : "");
}

// --- Outbox helpers ----------------------------------------------------------

static void send_flag(uint32_t key) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int one = 1;
  dict_write_int(out, key, &one, sizeof(int), true);
  app_message_outbox_send();
}

// Sends a request flag together with the active project id, so the phone books
// / lists against the project currently selected on the watch.
static void send_with_project(uint32_t key, int value) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int(out, key, &value, sizeof(int), true);
  int pid = data_active_project_id();
  dict_write_int(out, MESSAGE_KEY_PROJECT_ID, &pid, sizeof(int), true);
  app_message_outbox_send();
}

void config_request_projects(void)      { send_flag(MESSAGE_KEY_REQUEST_PROJECTS); }
void config_request_activities(void)    { send_with_project(MESSAGE_KEY_REQUEST_ACTIVITIES, 1); }
void config_start_activity(int act_id)  { send_with_project(MESSAGE_KEY_START_ACTIVITY, act_id); }
void config_stop_activity(void)         { send_flag(MESSAGE_KEY_STOP_ACTIVITY); }
void config_switch_activity(int act_id) { send_with_project(MESSAGE_KEY_SWITCH_ACTIVITY, act_id); }
