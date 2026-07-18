#pragma once
#include <pebble.h>

// Fixed capacities — no malloc on the watch.
#define MAX_ACTIVITIES 32
#define ACT_NAME_LEN   32

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

// One global Kimai activity shown on the watch.
typedef struct {
  int  id;
  char name[ACT_NAME_LEN];
} Activity;

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

// --- Timer -------------------------------------------------------------------
TimerState *data_timer(void);
// Formats the running timer's elapsed time as "h:mm:ss" into buf.
void data_timer_elapsed_str(char *buf, size_t len);
