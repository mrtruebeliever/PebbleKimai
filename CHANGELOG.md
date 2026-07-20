# Changelog

## 2.2.0

- **Activities bound to the active project now surface at the top of the
  list**; the remaining global activities stay reachable below, dimmed so the
  project's own activities stand out (a project with no bound activities
  still shows everything at full color).
- **Sorted by recent use.** Within each group, activities are ordered by how
  much time was logged on them in the last 90 days (most-used first), so the
  activities you actually track land within easy reach.
- **Project / customer picker title bar.** The picker windows now show
  "Project" or "Customer" at the top so it's clear which list is on screen.

## 2.1.0

- **Hero start screen**: a landing card showing the running activity (name, big
  ticking elapsed, project/customer) or, when idle, the last activity with a
  one-press "Start again". Down/Select open the activity list.
- **Touchscreen navigation** on Pebble Time 2: tap a row to select/start/stop/
  switch, tap the hero to open the list; physical buttons remain the fallback
  and touch self-disables on non-touch watches.
- The last activity is remembered across a stop (persisted on the watch).
- Stopwatch icon in the header bar.
- The hero's large text (activity name + elapsed) uses the Rajdhani display font
  (as on the Demi watchface).

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
