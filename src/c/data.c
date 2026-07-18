#include "data.h"

static int        s_load_state = LOAD_LOADING;
static Activity   s_activities[MAX_ACTIVITIES];
static int        s_activity_count = 0;
static TimerState s_timer = { .status = TIMER_UNKNOWN };

int  data_load_state(void)      { return s_load_state; }
void data_set_load_state(int s) { s_load_state = s; }

int data_activity_count(void) { return s_activity_count; }

void data_set_activity_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_ACTIVITIES) n = MAX_ACTIVITIES;
  s_activity_count = n;
}

void data_clear_activities(void) {
  s_activity_count = 0;
  memset(s_activities, 0, sizeof(s_activities));
}

Activity *data_activity(int i) {
  if (i < 0 || i >= MAX_ACTIVITIES) return NULL;
  return &s_activities[i];
}

TimerState *data_timer(void) { return &s_timer; }

void data_timer_elapsed_str(char *buf, size_t len) {
  time_t now = time(NULL);
  int e = (s_timer.begin > 0 && now > s_timer.begin) ? (int)(now - s_timer.begin) : 0;
  snprintf(buf, len, "%d:%02d:%02d", e / 3600, (e / 60) % 60, e % 60);
}
