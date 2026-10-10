#include "timer_actions.h"
#include "activity_list.h"
#include "config.h"
#include "data.h"
#include "hero.h"
#include "i18n.h"
#include "theme.h"

// What a pending switch confirmation acts on. A full copy, not an id: a recent
// activity can belong to a project whose activities are not currently loaded,
// so it cannot be looked up in the list when the confirmation fires.
static Activity s_target;
static int      s_target_pid = 0;
static char     s_target_pname[PROJ_NAME_LEN] = "";

// Repaint whichever of the two windows is on screen (both reloads are null-safe
// against their own layer pointers, so the off-screen one is a no-op).
static void reload_views(void) {
  timer_apply_tint();
  hero_reload();
  activity_list_reload();
}

#define TINT_RUNNING 0x50FF78u

void timer_apply_tint(void) {
  static int s_tinted = -1;   // -1 unknown, 0 system colour, 1 green
  int want = data_timer()->status == TIMER_RUNNING ? 1 : 0;
  if (want == s_tinted) return;
  s_tinted = want;
  // (void): on platforms without a colour backlight these are `(0)` macros.
  if (want) (void)light_set_color_rgb888(TINT_RUNNING);
  else      (void)light_set_system_color();
}

// --- Confirmation tones (optional, phone settings) ---------------------------
// Start rises, stop falls, so the two can be told apart without looking. Only
// the Time 2 (emery) has a speaker; elsewhere the speaker calls do not exist.
#if defined(PBL_PLATFORM_EMERY)
static const SpeakerNote TONE_START[] = {
  {79, SpeakerWaveformSine, 70, 80, 0}, {84, SpeakerWaveformSine, 90, 85, 0},
  {84, SpeakerWaveformSine, 120, 22, 0},
};
static const SpeakerNote TONE_STOP[] = {
  {84, SpeakerWaveformSine, 70, 80, 0}, {79, SpeakerWaveformSine, 90, 80, 0},
  {79, SpeakerWaveformSine, 120, 22, 0},
};
#define TONE_VOLUME 60
#endif

// The vibration the app always gave, plus the tone when switched on. Both stay
// quiet during Quiet Time; the tone also follows the watch's speaker mute.
static void confirm(bool start) {
  if (quiet_time_is_active()) { return; }
  vibes_short_pulse();
#if defined(PBL_PLATFORM_EMERY)
  if (!config_timer_sound() || speaker_is_muted()) { return; }
  if (start) speaker_play_notes(TONE_START, ARRAY_LENGTH(TONE_START), TONE_VOLUME);
  else       speaker_play_notes(TONE_STOP, ARRAY_LENGTH(TONE_STOP), TONE_VOLUME);
#else
  (void)start;
#endif
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
  confirm(true);
  reload_views();
}

void timer_switch(Activity *a) {
  if (!a) return;
  begin_local_run(a);
  config_switch_activity(a->id);
  confirm(true);
  reload_views();
}

void timer_stop(void) {
  TimerState *tm = data_timer();
  tm->status = TIMER_IDLE;
  tm->act_id = 0;
  tm->begin = 0;
  tm->pending = true;
  config_stop_activity();
  confirm(false);
  reload_views();
}

void timer_refresh(void) {
  data_set_load_state(LOAD_LOADING);
  config_request_projects();
  config_request_activities();
  reload_views();
}

void timer_start_in_project(const Activity *a, int project_id, const char *project_name) {
  if (!a) return;
  if (project_id > 0 && project_id != data_active_project_id()) {
    Project *p = data_project_by_id(project_id);
    config_set_active_project(project_id,
                              p ? p->name : (project_name ? project_name : ""),
                              p ? p->customer_id : 0,
                              p ? p->customer : "");
  }
  Activity copy = *a;
  if (data_timer()->status == TIMER_RUNNING) { timer_switch(&copy); }
  else                                       { timer_start(&copy); }
}

// --- Confirmations (the watch's own ActionMenu) ------------------------------

static void switch_to_target(void) {
  timer_start_in_project(&s_target, s_target_pid, s_target_pname);
}

// Runs the chosen item's function, which was stored as the item's action data.
static void perform(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  void (*fn)(void) = action_menu_item_get_action_data(item);
  if (fn) { fn(); }
}

// The level is owned by the menu for as long as it is open.
static void did_close(ActionMenu *menu, const ActionMenuItem *performed, void *context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(menu), NULL, NULL);
}

static void show_menu(const char *labels[], void (*fns[])(void), int n) {
  ActionMenuLevel *root = action_menu_level_create(n);
  if (!root) return;
  for (int i = 0; i < n; i++) {
    action_menu_level_add_action(root, labels[i], perform, (void *)(uintptr_t)fns[i]);
  }
  ActionMenuConfig config = {
    .root_level = root,
    .colors = { .background = theme_accent(), .foreground = GColorBlack },
    .align = ActionMenuAlignTop,
    .did_close = did_close,
  };
  action_menu_open(&config);
}

void timer_open_stop_menu(void) {
  const char *labels[] = { i18n(STR_STOP_TIMER) };
  void (*fns[])(void) = { timer_stop };
  show_menu(labels, fns, 1);
}

static void open_switch_menu(const Activity *a) {
  // The label must outlive this call: the menu reads it while it is open.
  static char label[ACT_NAME_LEN + 20];
  snprintf(label, sizeof(label), i18n(STR_SWITCH_TO), a->name);
  s_target = *a;
  const char *labels[] = { label };
  void (*fns[])(void) = { switch_to_target };
  show_menu(labels, fns, 1);
}

void timer_open_switch_menu(Activity *a) {
  if (!a) return;
  s_target_pid = 0;
  s_target_pname[0] = '\0';
  open_switch_menu(a);
}

void timer_open_switch_menu_in(const Activity *a, int project_id, const char *project_name) {
  if (!a) return;
  s_target_pid = project_id;
  strncpy(s_target_pname, project_name ? project_name : "", PROJ_NAME_LEN - 1);
  s_target_pname[PROJ_NAME_LEN - 1] = '\0';
  open_switch_menu(a);
}

void timer_open_long_press_menu(void) {
  bool running = data_timer()->status == TIMER_RUNNING;
  const char *labels[2];
  void (*fns[2])(void);
  int n = 0;
  labels[n] = i18n(STR_REFRESH); fns[n] = timer_refresh; n++;
  if (running) { labels[n] = i18n(STR_STOP_TIMER); fns[n] = timer_stop; n++; }
  show_menu(labels, fns, n);
}
