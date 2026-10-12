# Changelog

## 2.10.0

- **Pebble Round 2 (gabbro) and Pebble 2 Duo (flint).** On round screens the
  header puts the clock on top and the title under it, list rows follow the
  edge of the circle, and the start screen moves the customer and status icon
  inside it. On black-and-white screens the bars are white and the selected row
  black, instead of a grey dither under the text. The header clock is a size
  smaller on 144 px screens.
- Settings page moved to @rebble/clay (needed for the new platforms).
- Store screenshots for both.

## 2.9.0

- **Sound on start/stop** (phone settings, off by default, Pebble Time 2):
  starting or switching the timer plays a short rising tone, stopping it a
  falling one, alongside the vibration. Silent during Quiet Time and when the
  watch speaker is muted (new `TIMER_SOUND` message key).
- **List text follows the watch's text size.** The main line of every list row
  (activity, project, recent) is Gothic 18 bold at the watch's default size,
  and steps to 14 or 24 bold with Settings → Display → Text size; rows grow to
  match.
- **Green backlight while a timer runs (Pebble Time 2).** The colour backlight
  turns green while a timer is running and returns to the watch's own colour
  when it stops or the app closes.
- **Running timer in the launcher.** While a timer runs, Kimai's line in the
  app list shows the elapsed time and the activity ("0:43:32 · Development").
  The firmware keeps it ticking after the app closes; it clears once the timer
  is stopped (not on aplite, which has no app glances).
- Silenced the "statement with no effect" warnings on aplite, basalt and chalk,
  where the SDK stubs `window_set_touch_bridge_disabled()` out as a bare `(0)`.

## 2.8.1

Everything below shipped as one round of work on the start screen and the lists;
none of the intermediate versions were released.

- **Recent activities, across every project.** A long press of Down on the start
  screen (or a tap on its status icon) opens the last five activities that
  actually ran, newest first, each with the project it ran on. Picking one books
  on *that* project and selects it on the watch — a project-bound activity is
  rejected by Kimai under any other project, so the booking project follows the
  row. The list is derived from the timesheet history the phone already fetches,
  so it also shows what you logged in the web interface.
- **Redesigned start screen.** The project sits in bold above the activity name
  instead of at the bottom, the status text became an icon in the bottom-right
  corner, and the customer moved to the bottom in bold. Long activity names wrap
  onto a second line rather than ending in an ellipsis, and the call to action
  fits on screen in every language.
- **The project shown is the one the running timesheet is booked on**, which is
  not the same as the project selected on the watch when a timer was started
  elsewhere or the selection changed mid-run. Picking another project while a
  timer runs adds a "Next:" line, so that change no longer happens invisibly.
- **Tap the running time to stop it**, through the same confirmation the buttons
  use, so a stray tap costs nothing. (Kimai has no pause: stopping closes the
  entry and starting again opens a new one.)
- **Every part of the start screen is a target**: status icon = recent
  activities, project line = project picker, elapsed time = stop, call to action
  = start again, anywhere else = the activity list.
- **The lists share one shape and are denser**: a bold grey context line on top
  (the project in the recent list, the customer in the pickers, shown only when
  the instance has more than one) with the row's own subject in bold underneath.
  Rows are 34px in the activity list, 40px in the recent list, and 32px for a
  project row without a customer above it — all five recent activities fit on
  one screen.
- **Confirmations use the watch's own ActionMenu again**, and the menus are back
  on the system's touch navigation. PebbleKimai's hand-built confirmation menu
  existed only because the system menu never received touch events; that is
  fixed from firmware 4.33.2. `app_touch_navigation_enable()` faults inside
  firmware 4.33.1 ([PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865)),
  so the bridge is switched on above that version only — an older watch keeps
  the hand-rolled touch layer instead of crashing.

## 2.4.0

- **Touch navigation, done in the app.** The activity list, project picker and
  confirm menu scroll with your finger (and keep gliding after you let go), open
  the row you tap, and go back on a left-to-right swipe. The hero card keeps its
  tap-to-open-the-list and swipe-right-to-leave gestures; it now shares the same
  gesture decoding as the lists.
- **The 2.3.0 switch to the watch's built-in touch navigation is reverted.** On
  firmware 4.33.1 `app_touch_navigation_enable(true)` faults inside firmware on
  the first touch and kills the app, whatever the app does. Touch now comes
  straight from the raw touch stream (`src/c/touch_nav.c`). Reported as
  [PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865).

## 2.3.0

- **Touch now uses the watch's built-in navigation** (Pebble SDK 4.33) instead
  of PebbleKimai's own tap/scroll/swipe handling — tapping, scrolling and
  swiping right to go back on the activity list and the project/customer
  pickers now feel exactly like every other menu on the watch. The separate
  "Touchscreen" toggle in settings is gone; touch follows the watch's own
  system-wide touch setting.

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
