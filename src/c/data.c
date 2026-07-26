#include "data.h"

static int        s_load_state = LOAD_LOADING;
static Activity   s_activities[MAX_ACTIVITIES];
static int        s_activity_count = 0;
static TimerState s_timer = { .status = TIMER_UNKNOWN };

static int      s_project_load_state = LOAD_LOADING;
static Project  s_projects[MAX_PROJECTS];
static int      s_project_count = 0;

// Active context (loaded from persist by config_load, kept in sync on picks).
static int  s_ap_id = 0;
static char s_ap_name[PROJ_NAME_LEN] = "";
static int  s_ap_customer_id = 0;
static char s_ap_customer[CUST_NAME_LEN] = "";
static int  s_ac_id = 0;
static char s_ac_name[CUST_NAME_LEN] = "";

static int  s_last_act_id = 0;
static char s_last_act_name[ACT_NAME_LEN] = "";

// Loading watchdog. A request the phone accepts but never answers used to leave
// the list on its spinner indefinitely, with exiting the app as the only way
// out. After this long without a reply, fall to the error state -- which does
// have a retry on it.
#define LOAD_TIMEOUT_MS 15000
static AppTimer *s_load_timer;
static void (*s_stall_cb)(void) = NULL;

void data_set_stall_callback(void (*cb)(void)) { s_stall_cb = cb; }

static void load_stalled(void *ctx) {
  s_load_timer = NULL;
  if (s_load_state != LOAD_LOADING) return;
  s_load_state = LOAD_ERROR;
  if (s_stall_cb) s_stall_cb();
}

int  data_load_state(void)      { return s_load_state; }

void data_set_load_state(int s) {
  s_load_state = s;
  if (s_load_timer) {
    app_timer_cancel(s_load_timer);
    s_load_timer = NULL;
  }
  if (s == LOAD_LOADING) {
    s_load_timer = app_timer_register(LOAD_TIMEOUT_MS, load_stalled, NULL);
  }
}

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

// --- Projects ----------------------------------------------------------------

int  data_project_load_state(void)      { return s_project_load_state; }
void data_set_project_load_state(int s) { s_project_load_state = s; }

int data_project_count(void) { return s_project_count; }

void data_set_project_count(int n) {
  if (n < 0) n = 0;
  if (n > MAX_PROJECTS) n = MAX_PROJECTS;
  s_project_count = n;
}

void data_clear_projects(void) {
  s_project_count = 0;
  memset(s_projects, 0, sizeof(s_projects));
}

Project *data_project(int i) {
  if (i < 0 || i >= MAX_PROJECTS) return NULL;
  return &s_projects[i];
}

Project *data_project_by_id(int id) {
  for (int i = 0; i < s_project_count; i++) {
    if (s_projects[i].id == id) return &s_projects[i];
  }
  return NULL;
}

int data_customer_count(void) {
  int n = 0;
  for (int i = 0; i < s_project_count; i++) {
    bool seen = false;
    for (int j = 0; j < i; j++) {
      if (s_projects[j].customer_id == s_projects[i].customer_id) { seen = true; break; }
    }
    if (!seen) n++;
  }
  return n;
}

// --- Active context ----------------------------------------------------------

int         data_active_project_id(void)          { return s_ap_id; }
const char *data_active_project_name(void)        { return s_ap_name; }
int         data_active_project_customer_id(void) { return s_ap_customer_id; }
const char *data_active_project_customer(void)    { return s_ap_customer; }
int         data_active_customer_id(void)         { return s_ac_id; }
const char *data_active_customer_name(void)       { return s_ac_name; }

void data_set_active_project(int id, const char *name, int customer_id, const char *customer) {
  s_ap_id = id;
  strncpy(s_ap_name, name ? name : "", PROJ_NAME_LEN - 1);
  s_ap_name[PROJ_NAME_LEN - 1] = '\0';
  s_ap_customer_id = customer_id;
  strncpy(s_ap_customer, customer ? customer : "", CUST_NAME_LEN - 1);
  s_ap_customer[CUST_NAME_LEN - 1] = '\0';
}

void data_set_active_customer(int id, const char *name) {
  s_ac_id = id;
  strncpy(s_ac_name, name ? name : "", CUST_NAME_LEN - 1);
  s_ac_name[CUST_NAME_LEN - 1] = '\0';
}

int         data_last_activity_id(void)   { return s_last_act_id; }
const char *data_last_activity_name(void) { return s_last_act_name; }

void data_set_last_activity(int id, const char *name) {
  s_last_act_id = id;
  strncpy(s_last_act_name, name ? name : "", ACT_NAME_LEN - 1);
  s_last_act_name[ACT_NAME_LEN - 1] = '\0';
}

TimerState *data_timer(void) { return &s_timer; }

void data_timer_elapsed_str(char *buf, size_t len) {
  time_t now = time(NULL);
  int e = (s_timer.begin > 0 && now > s_timer.begin) ? (int)(now - s_timer.begin) : 0;
  snprintf(buf, len, "%d:%02d:%02d", e / 3600, (e / 60) % 60, e % 60);
}
