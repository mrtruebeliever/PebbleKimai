#include "timer_actions.h"
#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "hero.h"
#include "i18n.h"
#include "theme.h"

static ActionMenu *s_action_menu = NULL;
static int         s_target_act_id = 0;   // activity the open switch menu acts on

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

// --- ActionMenu (confirmations) ----------------------------------------------

static void action_stop(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  timer_stop();
}

static void action_switch(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  Activity *a = activity_by_id(s_target_act_id);
  if (a) { timer_switch(a); }
}

static void action_refresh(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  timer_refresh();
}

static void action_menu_closed(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(menu), NULL, NULL);
  s_action_menu = NULL;
}

static void open_action_menu(ActionMenuLevel *root) {
  ActionMenuConfig cfg = (ActionMenuConfig) {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignCenter,
    .did_close = action_menu_closed,
  };
  s_action_menu = action_menu_open(&cfg);
}

void timer_open_stop_menu(void) {
  ActionMenuLevel *root = action_menu_level_create(1);
  action_menu_level_add_action(root, i18n(STR_STOP_TIMER), action_stop, NULL);
  open_action_menu(root);
}

void timer_open_switch_menu(Activity *a) {
  if (!a) return;
  static char label[ACT_NAME_LEN + 20];
  snprintf(label, sizeof(label), i18n(STR_SWITCH_TO), a->name);
  s_target_act_id = a->id;
  ActionMenuLevel *root = action_menu_level_create(1);
  action_menu_level_add_action(root, label, action_switch, NULL);
  open_action_menu(root);
}

void timer_open_long_press_menu(void) {
  bool running = data_timer()->status == TIMER_RUNNING;
  ActionMenuLevel *root = action_menu_level_create(running ? 2 : 1);
  action_menu_level_add_action(root, i18n(STR_REFRESH), action_refresh, NULL);
  if (running) {
    action_menu_level_add_action(root, i18n(STR_STOP_TIMER), action_stop, NULL);
  }
  open_action_menu(root);
}
