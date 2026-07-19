# PebbleKimai

Time tracking for [Kimai](https://www.kimai.org/) (self-hosted) on your Pebble:
choose the project on the watch, tap an activity to start a timer, tap it again
to stop, or pick another to switch. The running timer ticks on the watch and in
the header bar; the Kimai server is always the source of truth.

## Features

- **Active-project row** at the top: tap it to pick the project new timesheets
  are booked on; long-press it to pick a **customer** (the project list then
  filters to that customer). Your latest choice is remembered on the watch.
- List of the **global activities** plus any activities bound to the active
  project.
- **Start** with one press while idle; **stop** and **switch** ask for a single
  confirmation tap so a running timer is never killed by accident.
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

- `src/c/` — watch app: `activity_list` (MenuLayer with the project row +
  ActionMenu confirmations), `project_picker` (project / customer picker),
  `header_bar` (accent bar with clock + ticking elapsed time), `data` (fixed
  arrays, project list, active context, timer state), `config` (AppMessage
  protocol + persisted settings), `i18n` (five-language string table), `toast`
  (error popups).
- `src/pkjs/` — phone companion: Kimai REST calls, project + activity streaming,
  timer reconciliation, Clay settings page (`i18n.js`).

## License

MIT
