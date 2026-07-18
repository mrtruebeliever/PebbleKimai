# PebbleKimai

Time tracking for [Kimai](https://www.kimai.org/) (self-hosted) on your Pebble:
pick an activity on the watch to start a timer, tap it again to stop, or pick
another one to switch. The running timer ticks on the watch and in the header
bar; the Kimai server is always the source of truth.

## Features

- Flat list of your **global activities** (activities without a project).
- **Start** with one press while idle; **stop** and **switch** ask for a single
  confirmation tap so a running timer is never killed by accident.
- Optimistic UI: the timer starts ticking immediately, then reconciles with the
  server's `GET /api/timesheets/active` state (elapsed time is always recomputed
  from the server's begin time, so clock drift never accumulates).
- The watch never sees your API token; only the phone-side JS talks to Kimai.
- Long-press Select for Refresh / Stop.

## Setup

1. In Kimai, create your activities **without a project** (that makes them
   global), e.g. Development, Hardware, Software, Network, Support.
2. Create an API token: Kimai → your user profile → **API Access**.
3. In the Pebble phone app, open the PebbleKimai settings and fill in your
   server URL and API token. Reopen the settings to pick the project that new
   timesheets are booked on (Kimai requires a project on every timesheet; if you
   leave it on "(first project)" the first visible project is used).
4. Recommended: disable "Allow overlapping timesheets" in Kimai's settings so
   only one timer can run at a time.

Note: the phone must be able to reach your Kimai instance (same network or
exposed via reverse proxy/VPN). Plain `http://` may be blocked by the phone OS;
prefer HTTPS with a real certificate.

## Build

```bash
npm install
pebble build
pebble install --emulator emery   # or basalt/chalk/diorite/aplite
```

## Architecture

- `src/c/` — watch app: `activity_list` (MenuLayer + ActionMenu confirmations),
  `header_bar` (accent bar with clock + ticking elapsed time), `data` (fixed
  arrays, timer state), `config` (AppMessage protocol), `toast` (error popups).
- `src/pkjs/` — phone companion: Kimai REST calls, activity streaming,
  timer reconciliation, Clay settings page (`config.js`).

## License

MIT
