# Changelog

## 2.0.0

- **Project context on the watch.** A new active-project row at the top of the
  list: tap it to choose the project that time is booked on, long-press it to
  choose a customer (the project list filters to that customer). The last choice
  is persisted on the watch; the phone settings only provide the initial default.
- **Project-bound activities.** The activity list now shows the global
  activities plus any activities bound to the active project.
- **Five languages** (English, Nederlands, Français, Deutsch, Español) on the
  watch and the settings page, automatic by watch language with a manual
  override.
- Launcher / store icons (stopwatch).

## 1.0.0

- Initial release: flat list of global activities, one-press start, confirmed
  stop/switch, ticking elapsed time, optimistic UI reconciled against
  `GET /api/timesheets/active`, Clay settings (URL/token/project, token kept on
  the phone). The server timestamps timesheets to avoid timezone drift, with a
  retry around Kimai's minute rounding.
