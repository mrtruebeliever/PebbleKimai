#pragma once
#include <pebble.h>

// Fixed capacities — no malloc on the watch.
#define MAX_ACTIVITIES 32
#define ACT_NAME_LEN   32
#define MAX_PROJECTS   16
#define PROJ_NAME_LEN  32
#define CUST_NAME_LEN  32

// Load state of the activity list.
#define LOAD_LOADING       0
#define LOAD_OK            1
#define LOAD_ERROR         2  // fetch failed (network / token)
#define LOAD_UNCONFIGURED  3  // no server URL / token configured on the phone

// Timer status. UNKNOWN until the phone answers the first state request;
// IDLE/RUNNING mirror the TIMER_STATUS values the phone sends.
#define TIMER_UNKNOWN 0
#define TIMER_IDLE    1
#define TIMER_RUNNING 2

// Error codes sent by the phone in ERROR_CODE (mirrored in index.js).
#define ERR_NETWORK    1
#define ERR_AUTH       2
#define ERR_CONFLICT   3
#define ERR_SERVER     4
#define ERR_NO_PROJECT 5

// One Kimai activity shown on the watch. `project_id` is the active project it
// is bound to, or 0 for a global activity — set from the phone's per-project
// filtered stream, so any nonzero value here always means "bound to whatever
// project is currently active" (see activity_list.c's dimming of globals).
typedef struct {
  int  id;
  char name[ACT_NAME_LEN];
  int  project_id;
} Activity;

// One Kimai project, with its owning customer (streamed from the phone). The
// customer fields let the watch group projects by customer without a second
// endpoint (Kimai's /api/projects already carries parentTitle + customer id).
typedef struct {
  int  id;
  char name[PROJ_NAME_LEN];
  int  customer_id;
  char customer[CUST_NAME_LEN];
} Project;

// The (single) running-timer state. The server is the source of truth: `begin`
// is the server-reported start as epoch seconds, so elapsed time is always
// recomputed from it and local clock drift never accumulates. `pending` marks
// an optimistic local change that the phone has not confirmed yet.
typedef struct {
  int    status;                 // TIMER_*
  int    ts_id;                  // running timesheet id on the server (0 = unknown)
  int    act_id;                 // running activity id
  char   act_name[ACT_NAME_LEN];
  time_t begin;                  // epoch seconds
  bool   pending;
} TimerState;

// --- Load state --------------------------------------------------------------
int  data_load_state(void);
void data_set_load_state(int s);

// --- Activities --------------------------------------------------------------
int       data_activity_count(void);
void      data_set_activity_count(int n);  // clamps to MAX_ACTIVITIES
void      data_clear_activities(void);
Activity *data_activity(int i);            // by index, NULL if out of range

// --- Projects ----------------------------------------------------------------
// Separate load state so the project list can (re)load without disturbing the
// activity list's own loading/error state.
int       data_project_load_state(void);
void      data_set_project_load_state(int s);
int       data_project_count(void);
void      data_set_project_count(int n);   // clamps to MAX_PROJECTS
void      data_clear_projects(void);
Project  *data_project(int i);             // by index, NULL if out of range
Project  *data_project_by_id(int id);      // NULL if not found
// Number of distinct customers across the loaded projects.
int       data_customer_count(void);

// --- Active context (persisted; see config.c) --------------------------------
// The project time is booked on, and the customer filter for the project picker.
int         data_active_project_id(void);
const char *data_active_project_name(void);
int         data_active_project_customer_id(void);
const char *data_active_project_customer(void);
int         data_active_customer_id(void);
const char *data_active_customer_name(void);
void data_set_active_project(int id, const char *name, int customer_id, const char *customer);
void data_set_active_customer(int id, const char *name);

// --- Last activity (persisted; see config.c) ---------------------------------
// The most recent activity a timer ran on. Survives a stop so the idle hero can
// offer to start it again. 0 / "" when nothing has run yet.
int         data_last_activity_id(void);
const char *data_last_activity_name(void);
void        data_set_last_activity(int id, const char *name);

// --- Timer -------------------------------------------------------------------
TimerState *data_timer(void);
// Formats the running timer's elapsed time as "h:mm:ss" into buf.
void data_timer_elapsed_str(char *buf, size_t len);
