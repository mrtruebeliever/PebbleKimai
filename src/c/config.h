#pragma once
#include <pebble.h>

// Persist storage keys.
#define PERSIST_LANG          1
#define PERSIST_AP_ID         2
#define PERSIST_AP_NAME       3
#define PERSIST_AP_CUST_ID    4
#define PERSIST_AP_CUST_NAME  5
#define PERSIST_AC_ID         6
#define PERSIST_AC_NAME       7
#define PERSIST_LAST_ACT_ID   8
#define PERSIST_LAST_ACT_NAME 9
// 10 was PERSIST_TOUCH (Clay touch toggle, removed once SDK 4.33 made touch a
// system-wide watch setting). Do not reuse.
// (next free persist key: 11)

// AppMessage in/outbox for the Kimai companion protocol.

// Loads persisted settings (language + active project/customer) into `data`.
void config_load(void);

// Current UI language index (Lang from i18n.h). Valid after config_load.
int  config_lang(void);

// Fired whenever incoming data changed (activities, projects, load state, timer).
void config_set_change_callback(void (*cb)(void));
// Fired when the phone reports an operation error (ERR_* code from data.h).
void config_set_error_callback(void (*cb)(int code));

void config_inbox_received(DictionaryIterator *iter, void *context);

// --- Active-context mutators (persist + keep the phone in sync) ---------------
// Picks a project to book on: persists it, remembers its customer as the active
// customer filter, and asks the phone to (re)load that project's activities.
void config_set_active_project(int id, const char *name, int customer_id, const char *customer);
// Picks the customer filter for the project picker (persisted).
void config_set_active_customer(int id, const char *name);

// Records the running activity as the "last activity" (persisted). No-op for an
// empty/unchanged value.
void config_note_last_activity(int id, const char *name);

// --- Outbox helpers ----------------------------------------------------------
void config_request_projects(void);                    // reload the project list
void config_request_activities(void);                  // reload the active project's activities + timer
void config_start_activity(int act_id);                // start; books on the active project
void config_stop_activity(void);
void config_switch_activity(int act_id);               // stop whatever runs, then start act_id
