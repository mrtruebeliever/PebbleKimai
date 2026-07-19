#pragma once
#include <pebble.h>

// UI language. Index MUST match the phone side (src/pkjs/i18n.js) and the Clay
// LANGUAGE select values.
typedef enum {
  LANG_EN = 0,
  LANG_NL = 1,
  LANG_FR = 2,
  LANG_DE = 3,
  LANG_ES = 4,
  LANG_COUNT,
} Lang;

// Sentinel stored/sent for "follow the watch's system locale" (see config.c).
#define LANG_AUTO 255

// All translatable UI strings.
typedef enum {
  STR_LOADING,          // "Loading..."
  STR_NO_CONNECTION,    // "No connection"
  STR_SETUP,            // "Set up in the\nphone app"
  STR_NO_ACTIVITIES,    // "No activities\nin Kimai"
  STR_NO_PROJECTS,      // "No projects"
  STR_NO_CUSTOMERS,     // "No customers"
  STR_NO_PROJECT_SET,   // "(no project)"
  STR_STOP_TIMER,       // "Stop timer"
  STR_SWITCH_TO,        // "Switch to %s"  (printf format, one string arg)
  STR_REFRESH,          // "Refresh"
  STR_PROJECT,          // "Project"
  STR_CUSTOMER,         // "Customer"
  STR_TOAST_NETWORK,    // "No connection"
  STR_TOAST_AUTH,       // "Check API token"
  STR_TOAST_CONFLICT,   // "Timer conflict"
  STR_TOAST_NOPROJECT,  // "No project in Kimai"
  STR_TOAST_ERROR,      // "Kimai error"
  STR_COUNT,
} StrId;

// Returns the string in the currently configured language (config_lang()).
const char *i18n(StrId id);
