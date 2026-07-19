#include "timer_actions.h"
#include "activity_list.h"
#include "confirm_menu.h"
#include "config.h"
#include "data.h"
#include "hero.h"
#include "i18n.h"

static int s_target_act_id = 0;   // activity the pending switch confirmation acts on

// Repaint whichever of the two windows is on screen (both reloads are null-safe
// against their own layer pointers, so the off-screen one is a no-op).
static void reload_views(void) {
  hero_reload();
  activity_list_reload();
}

static Activity *activity_by_id(int id) {
  for (int i = 0; i < data_activity_count(); i++) {
    Activity *a = data_activity(i);
    if (a && a->id == id) return a;
  }
  return NULL;
}

// The UI flips immediately; config.c clears `pending` on every confirmed state.
static void begin_local_run(Activity *a) {
  TimerState *tm = data_timer();
  tm->status = TIMER_RUNNING;
  tm->ts_id = 0;
  tm->act_id = a->id;
  strncpy(tm->act_name, a->name, ACT_NAME_LEN - 1);
  tm->act_name[ACT_NAME_LEN - 1] = '\0';
  tm->begin = time(NULL);
  tm->pending = true;
  config_note_last_activity(a->id, a->name);
}

void timer_start(Activity *a) {
  if (!a) return;
  begin_local_run(a);
  config_start_activity(a->id);
  vibes_short_pulse();
  reload_views();
}

void timer_switch(Activity *a) {
  if (!a) return;
  begin_local_run(a);
  config_switch_activity(a->id);
  vibes_short_pulse();
  reload_views();
}

void timer_stop(void) {
  TimerState *tm = data_timer();
  tm->status = TIMER_IDLE;
  tm->act_id = 0;
  tm->begin = 0;
  tm->pending = true;
  config_stop_activity();
  vibes_short_pulse();
  reload_views();
}

void timer_refresh(void) {
  data_set_load_state(LOAD_LOADING);
  config_request_projects();
  config_request_activities();
  reload_views();
}

// --- Confirmations (touch- and button-friendly menu) -------------------------

static void switch_to_target(void) {
  Activity *a = activity_by_id(s_target_act_id);
  if (a) { timer_switch(a); }
}

void timer_open_stop_menu(void) {
  ConfirmItem items[] = { { i18n(STR_STOP_TIMER), timer_stop } };
  confirm_menu_show(items, 1);
}

void timer_open_switch_menu(Activity *a) {
  if (!a) return;
  static char label[ACT_NAME_LEN + 20];
  snprintf(label, sizeof(label), i18n(STR_SWITCH_TO), a->name);
  s_target_act_id = a->id;
  ConfirmItem items[] = { { label, switch_to_target } };
  confirm_menu_show(items, 1);
}

void timer_open_long_press_menu(void) {
  bool running = data_timer()->status == TIMER_RUNNING;
  ConfirmItem items[2];
  int n = 0;
  items[n++] = (ConfirmItem){ i18n(STR_REFRESH), timer_refresh };
  if (running) { items[n++] = (ConfirmItem){ i18n(STR_STOP_TIMER), timer_stop }; }
  confirm_menu_show(items, n);
}
