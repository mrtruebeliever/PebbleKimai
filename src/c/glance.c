#include "glance.h"
#include "data.h"

// Aplite has no app glances; there the call is a no-op.
#ifndef PBL_PLATFORM_APLITE

// The firmware caps a subtitle template at 150 bytes.
static char s_subtitle[150];

// Appends `src` to `dst` as literal text: the subtitle is a template string,
// where '{' opens an expression and '\' escapes the next character.
static void append_literal(char *dst, size_t len, const char *src) {
  size_t n = strlen(dst);
  for (; *src && n + 2 < len; src++) {
    if (*src == '{' || *src == '\\') { dst[n++] = '\\'; }
    dst[n++] = *src;
  }
  dst[n] = '\0';
}

static void reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) { return; }
  AppGlanceSlice slice = {
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = s_subtitle,
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  AppGlanceResult r = app_glance_add_slice(session, slice);
  if (r != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "glance: add_slice %d", (int)r);
  }
}

void glance_update(void) {
  TimerState *t = data_timer();
  // The phone never answered this session: keep whatever the launcher shows.
  if (t->status == TIMER_UNKNOWN) { return; }
  if (t->status != TIMER_RUNNING || t->begin <= 0) {
    app_glance_reload(NULL, NULL);
    return;
  }
  // The firmware evaluates time_since() itself, so the launcher keeps counting
  // after the app has closed. Same h:mm:ss as the hero; 'f' lets the hours run
  // past 24 instead of wrapping.
  snprintf(s_subtitle, sizeof(s_subtitle),
           "{time_since(%ld)|format('%%fH:%%0M:%%0S')}", (long)t->begin);
  if (t->act_name[0]) {
    append_literal(s_subtitle, sizeof(s_subtitle), " · ");
    append_literal(s_subtitle, sizeof(s_subtitle), t->act_name);
  }
  app_glance_reload(reload, NULL);
}

#else

void glance_update(void) {}

#endif
