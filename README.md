# PebbleKimai

Time tracking for [Kimai](https://www.kimai.org/) (self-hosted) on your Pebble:
choose the project on the watch, tap an activity to start a timer, tap it again
to stop, or pick another to switch. The running timer ticks on the watch and in
the header bar; the Kimai server is always the source of truth.

## Features

- **Hero start screen**: the project in bold above the activity name (over two lines when it is long), a big ticking elapsed time
  and the customer in bold at the bottom, with a status icon in the bottom-right
  corner — or, when idle, the last activity with a one-press **Start again**.
  Tapping the elapsed time stops the timer (after confirming). The project shown while a timer runs is the one
  that timesheet is booked on; picking another project mid-run adds a **Next:**
  line, since it only applies to the next entry. Down/Select jump into the
  activity list; long-press Select stops (or refreshes).
- **Recent activities**: long-press Down (or tap the status icon) for the last
  five activities that ran, across every project, each with its own project.
  Picking one books on that project and selects it on the watch.
- **Touchscreen** on Pebble Time 2: the lists, pickers and confirmations use the
  watch's own touch navigation, so scrolling, tapping a row and swiping right to
  go back feel like every other menu. The start screen is a card rather than a
  menu, so it maps taps to what they point at: status icon = recent activities,
  project line = project picker, elapsed time = stop (with confirmation), call
  to action = start again, anywhere else = the activity list. The physical
  buttons keep working everywhere; on firmware older than 4.33.2 the app falls
  back to its own touch handling, since the system bridge crashes there.
- **Active-project row** at the top of the list: tap it to pick the project new timesheets
  are booked on; long-press it to pick a **customer** (the project list then
  filters to that customer). Your latest choice is remembered on the watch.
- List of the **global activities** plus any activities bound to the active
  project — the project's own activities surface on top (globals dimmed
  below), each group ordered by most time tracked in the last 90 days.
- **Start** with one press while idle; **stop** and **switch** ask for a single
  confirmation tap so a running timer is never killed by accident.
- **Sound on start/stop** *(optional, Pebble Time 2)* — a short tone, rising to
  start and falling to stop, on top of the vibration. Silent during Quiet Time
  or when the watch speaker is muted.
- **Running timer in the app list** — while a timer runs, Kimai's line in the
  launcher shows the elapsed time and the activity ("0:43:32 · Development"),
  ticking on after the app closes (not on aplite).
- **Green backlight while a timer runs** *(Pebble Time 2)* — the colour
  backlight turns green until the timer stops or the app closes.
- List rows follow the watch's **text size** (Settings → Display → Text size).
- Optimistic UI: the timer starts ticking immediately, then reconciles with the
  server's `GET /api/timesheets/active` state (elapsed time is always recomputed
  from the server's begin time, so clock drift never accumulates).
- **Five languages** (English, Nederlands, Français, Deutsch, Español), on the
  watch and the settings page, following the watch language automatically or set
  manually.
- The watch never sees your API token; only the phone-side JS talks to Kimai.
- Long-press an activity for Refresh / Stop.

## Setup

1. Create an API token: Kimai → your user profile → **API Access**.
2. In the Pebble phone app, open the PebbleKimai settings and fill in your
   server URL and API token. Reopen the settings to pick the default project and
   (if your instance has more than one customer) the default customer. You can
   always change both on the watch afterwards.
3. Recommended: disable "Allow overlapping timesheets" in Kimai's settings so
   only one timer can run at a time.

Kimai requires a project on every timesheet, including for global activities, so
a project is always selected (on the watch or as the settings default).

Note: the phone must be able to reach your Kimai instance (same network or
exposed via reverse proxy/VPN). Plain `http://` may be blocked by the phone OS;
prefer HTTPS with a real certificate.

## Build

```bash
npm install
pebble build
pebble install --emulator emery   # or basalt/chalk/diorite/aplite
```

Regenerate icons (needs the icon venv):

```bash
uv venv .art-venv && source .art-venv/bin/activate && uv pip install pillow
python3 tools/gen_icon.py
```

Build a size-optimized store `.pbw`: `python3 tools/release.py`.

## Architecture

- `src/c/` — watch app: `hero` (landing card: running / last activity + touch),
  `activity_list` (MenuLayer with the project row + touch hit-testing),
  `timer_actions` (optimistic start/stop/switch + confirmation ActionMenus shared
  by both screens), `project_picker` (project / customer picker), `header_bar`
  (accent bar with the app icon, clock + ticking elapsed time), `data` (fixed
  arrays, project list, active + last-activity context, timer state), `config`
  (AppMessage protocol + persisted settings), `i18n` (five-language string
  table), `toast` (error popups).
- `src/pkjs/` — phone companion: Kimai REST calls, project + activity streaming,
  timer reconciliation, Clay settings page (`i18n.js`).

## License

MIT
