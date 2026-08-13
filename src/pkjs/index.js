var Clay = require('pebble-clay');
var i18n = require('./i18n');
var buildConfig = i18n.buildConfig;
var keys = require('message_keys');

// LANG stores the raw setting: '255' = Automatic (follow watch locale), or '0'..'4'.
function getLangRaw() { var v = localStorage.getItem('LANG'); return (v == null || v === '') ? '255' : v; }
// Language to render the settings page in (Automatic falls back to English).
function getPageLang() { var r = parseInt(getLangRaw(), 10); return (r >= 0 && r < 5) ? r : 0; }

// Clay is driven manually so the settings page can be rebuilt with the live
// project/customer list (and language) each time it opens (autoHandleEvents:false).
var clay = new Clay(buildConfig(getPageLang(), null, null, { langRaw: getLangRaw() }), null,
  { autoHandleEvents: false });

// Load states (mirror data.h).
var LOAD_LOADING = 0;
var LOAD_OK = 1;
var LOAD_ERROR = 2;
var LOAD_UNCONFIGURED = 3;

// Timer status (mirror data.h).
var TIMER_IDLE = 1;
var TIMER_RUNNING = 2;

// Error codes (mirror data.h).
var ERR_NETWORK = 1;
var ERR_AUTH = 2;
var ERR_CONFLICT = 3;
var ERR_SERVER = 4;
var ERR_NO_PROJECT = 5;

var MAX_ACTIVITIES = 32;
var MAX_PROJECTS = 16;

// --- Persisted phone-side state (never sent to the watch) --------------------

function getBase() {
  var u = (localStorage.getItem('KIMAI_URL') || '').trim();
  u = u.replace(/\/+$/, '');       // drop trailing slashes
  u = u.replace(/\/api$/i, '');    // tolerate a trailing /api (we add it ourselves)
  return u;
}
function getToken() { return (localStorage.getItem('KIMAI_TOKEN') || '').trim(); }
function getDefProject() { return parseInt(localStorage.getItem('DEF_PROJECT'), 10) || 0; }
function getDefCustomer() { return parseInt(localStorage.getItem('DEF_CUSTOMER'), 10) || 0; }
function getAllProjects() {
  try { return JSON.parse(localStorage.getItem('ALL_PROJECTS') || '[]'); }
  catch (e) { return []; }
}
// Distinct customers derived from the cached project list, order preserved.
function getAllCustomers() {
  var seen = {};
  var out = [];
  getAllProjects().forEach(function (p) {
    if (p.customerId && !seen[p.customerId]) {
      seen[p.customerId] = true;
      out.push({ id: p.customerId, name: p.customer || String(p.customerId) });
    }
  });
  return out;
}
function configured() { return !!(getBase() && getToken()); }

// --- HTTP (Kimai REST API, Bearer auth) --------------------------------------

// Generic request; cb(ok, status, data).
function request(method, path, body, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open(method, getBase() + path);
  xhr.setRequestHeader('Authorization', 'Bearer ' + getToken());
  if (body) { xhr.setRequestHeader('Content-Type', 'application/json'); }
  xhr.timeout = 15000;
  xhr.onload = function () {
    var ok = (xhr.status >= 200 && xhr.status < 300);
    var data = null;
    try { data = xhr.responseText ? JSON.parse(xhr.responseText) : null; } catch (e) {}
    if (!ok) { console.log(method + ' ' + path + ' -> ' + xhr.status); }
    cb(ok, xhr.status, data);
  };
  xhr.onerror = function () { console.log(method + ' failed ' + path); cb(false, 0, null); };
  xhr.ontimeout = function () { console.log(method + ' timeout ' + path); cb(false, 0, null); };
  xhr.send(body ? JSON.stringify(body) : null);
}

// Maps an HTTP failure to the watch's ERR_* codes.
function errorCode(status) {
  if (status === 0) { return ERR_NETWORK; }
  if (status === 401 || status === 403) { return ERR_AUTH; }
  if (status >= 500) { return ERR_SERVER; }
  return ERR_CONFLICT;  // 4xx: validation / overlap / bad state
}

// --- AppMessage senders ------------------------------------------------------

function sendLoad(state) {
  var msg = {};
  msg[keys.LOAD_STATE] = state;
  Pebble.sendAppMessage(msg);
}

function sendError(code) {
  var msg = {};
  msg[keys.ERROR_CODE] = code;
  Pebble.sendAppMessage(msg);
}

// Sends the authoritative timer state. `active` is one entry from
// GET /api/timesheets/active, or null for idle.
function sendTimer(active) {
  var msg = {};
  if (active) {
    var act = active.activity || {};
    msg[keys.TIMER_STATUS] = TIMER_RUNNING;
    msg[keys.TIMER_TS_ID] = active.id || 0;
    msg[keys.TIMER_ACT_ID] = (typeof act === 'object' ? act.id : act) || 0;
    msg[keys.TIMER_ACT_NAME] = String((typeof act === 'object' && act.name) || '').substring(0, 31);
    msg[keys.TIMER_BEGIN] = Math.floor(new Date(active.begin).getTime() / 1000) || 0;
  } else {
    msg[keys.TIMER_STATUS] = TIMER_IDLE;
  }
  Pebble.sendAppMessage(msg);
}

// Streams the activity list one message at a time, advancing on each ACK.
function streamActivities(activities) {
  var count = Math.min(activities.length, MAX_ACTIVITIES);
  var head = {};
  head[keys.ACT_COUNT] = count;
  head[keys.LOAD_STATE] = LOAD_OK;
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function next() {
      if (i >= count) return;
      var a = activities[i];
      var msg = {};
      msg[keys.ACT_INDEX] = i;
      msg[keys.ACT_ID] = a.id;
      msg[keys.ACT_NAME] = String(a.name).substring(0, 31);
      msg[keys.ACT_PROJECT_ID] = a.projectId || 0;
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('act ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('act head send failed'); });
}

// Streams the project list (id, name, customer id + name) to the watch.
function streamProjects(projects) {
  var count = Math.min(projects.length, MAX_PROJECTS);
  var head = {};
  head[keys.PROJ_COUNT] = count;
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function next() {
      if (i >= count) return;
      var p = projects[i];
      var msg = {};
      msg[keys.PROJ_INDEX] = i;
      msg[keys.PROJ_ID] = p.id;
      msg[keys.PROJ_NAME] = String(p.name).substring(0, 31);
      msg[keys.PROJ_CUSTOMER_ID] = p.customerId || 0;
      msg[keys.PROJ_CUSTOMER_NAME] = String(p.customer || '').substring(0, 31);
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('proj ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('proj head send failed'); });
}

// Normalizes a Kimai project record to the fields the watch/settings need.
function normProject(p) {
  return { id: p.id, name: String(p.name), customerId: p.customer || 0,
           customer: String(p.parentTitle || '') };
}

// --- Loads -------------------------------------------------------------------

// Fetches the project list, caches it (for the settings page + customer list)
// and streams it to the watch.
function loadProjects() {
  if (!configured()) { return; }
  request('GET', '/api/projects?visible=1', null, function (ok, status, data) {
    if (!ok || !Array.isArray(data)) { console.log('loadProjects failed ' + status); return; }
    var projects = data.map(normProject);
    localStorage.setItem('ALL_PROJECTS', JSON.stringify(projects));
    streamProjects(projects);
  });
}

// The id of a Kimai activity's project (null/undefined for a global activity).
function activityProjectId(a) {
  var ap = a.project;
  if (ap == null) { return null; }
  return (typeof ap === 'object') ? ap.id : ap;
}

// --- Recent time-tracked per activity (drives the "most used first" sort) ----
// Cached in memory with a short TTL so a normal start/stop/switch cycle (which
// reloads the activity list) doesn't refetch timesheet history every time.

var DURATION_WINDOW_DAYS = 90;
var DURATION_TTL_MS = 15 * 60 * 1000;
var DURATION_PAGE_SIZE = 500;   // Kimai's documented max page size
var DURATION_MAX_PAGES = 3;     // safety cap: up to 1500 recent entries

var s_durations = {};   // activity id -> total tracked seconds in the window
var s_durationsAt = 0;  // ms timestamp of the last successful fetch

// Fetches every timesheet since the window start, paginating while a full
// page comes back (bounded by DURATION_MAX_PAGES).
function fetchRecentTimesheets(cb) {
  var begin = new Date(Date.now() - DURATION_WINDOW_DAYS * 86400000)
    .toISOString().replace(/\.\d+Z$/, '');
  var all = [];
  var page = 1;
  function next() {
    var path = '/api/timesheets?begin=' + encodeURIComponent(begin) +
      '&size=' + DURATION_PAGE_SIZE + '&page=' + page + '&orderBy=begin&order=DESC';
    request('GET', path, null, function (ok, status, data) {
      if (!ok || !Array.isArray(data)) { cb(all); return; }
      all = all.concat(data);
      if (data.length === DURATION_PAGE_SIZE && page < DURATION_MAX_PAGES) {
        page++; next();
      } else {
        cb(all);
      }
    });
  }
  next();
}

// Refreshes s_durations if the cache is stale, then calls cb(). Failures leave
// the previous (possibly empty) cache in place rather than blocking the list.
function loadDurations(cb) {
  if (Date.now() - s_durationsAt < DURATION_TTL_MS) { cb(); return; }
  fetchRecentTimesheets(function (records) {
    var sums = {};
    records.forEach(function (r) {
      var actId = (typeof r.activity === 'object') ? r.activity.id : r.activity;
      var dur = r.duration;
      if (actId == null || typeof dur !== 'number' || dur <= 0) { return; }
      sums[actId] = (sums[actId] || 0) + dur;
    });
    s_durations = sums;
    s_durationsAt = Date.now();
    cb();
  });
}

function byDurationDesc(x, y) { return y.duration - x.duration; }

// Fetches the activities available for `projectId`: the ones bound to that
// project first, then the global ones (each group ordered by most time
// tracked in the last DURATION_WINDOW_DAYS days, descending). Filtered
// client-side so it works regardless of which query-parameter combination
// the instance supports.
function loadActivities(projectId) {
  if (!configured()) { sendLoad(LOAD_UNCONFIGURED); return; }
  sendLoad(LOAD_LOADING);
  var pid = parseInt(projectId, 10) || 0;
  loadDurations(function () {
    request('GET', '/api/activities?visible=1', null, function (ok, status, data) {
      if (!ok || !Array.isArray(data)) { sendLoad(LOAD_ERROR); return; }
      var globals = [];
      var bound = [];
      data.forEach(function (a) {
        var apid = activityProjectId(a);
        var isBound = apid != null && pid && apid === pid;
        if (apid != null && !isBound) { return; }  // bound to a different project: not shown
        var item = { id: a.id, name: a.name, projectId: isBound ? apid : 0,
                     duration: s_durations[a.id] || 0 };
        (isBound ? bound : globals).push(item);
      });
      bound.sort(byDurationDesc);
      globals.sort(byDurationDesc);
      streamActivities(bound.concat(globals));
    });
  });
}

// Fetches the active timer and sends the authoritative state to the watch.
// More than one active timesheet is a conflict (should not happen with
// overlapping entries disabled in Kimai); the first one wins.
function loadTimer() {
  if (!configured()) { return; }
  request('GET', '/api/timesheets/active', null, function (ok, status, data) {
    if (!ok || !Array.isArray(data)) { sendError(errorCode(status)); return; }
    if (data.length > 1) { sendError(ERR_CONFLICT); }
    sendTimer(data.length ? data[0] : null);
  });
}

// --- Timer mutations ---------------------------------------------------------
// Every mutation ends by re-fetching /timesheets/active, so the watch always
// converges on the server's state (also after an error).

// Runs cb(projectId) with the project to book on: the one the watch sent, else
// the configured default, else the first visible project. cb(0) when none found.
function withProject(preferredId, cb) {
  var pid = parseInt(preferredId, 10) || 0;
  if (pid) { cb(pid); return; }
  var configuredId = getDefProject();
  if (configuredId) { cb(configuredId); return; }
  request('GET', '/api/projects?visible=1', null, function (ok, status, data) {
    if (ok && Array.isArray(data) && data.length) { cb(data[0].id); }
    else { cb(0); }
  });
}

// Strips the timezone offset from a Kimai datetime so it can be sent back
// as-is (the API interprets plain datetimes in the Kimai user's timezone).
function stripOffset(s) {
  return String(s || '').replace(/(Z|[+-]\d{2}:?\d{2})$/, '');
}

function startActivity(actId, projectId, begin) {
  withProject(projectId, function (pid) {
    if (!pid) { sendError(ERR_NO_PROJECT); loadTimer(); return; }
    // No `begin` normally: the server stamps "now" in the Kimai user's own
    // timezone, which sidesteps phone-vs-server timezone mismatches entirely.
    var body = { project: pid, activity: actId };
    if (begin) { body.begin = begin; }
    request('POST', '/api/timesheets', body, function (ok, status) {
      if (ok) { loadTimer(); return; }
      if (!begin && status === 400) {
        // Kimai rounds a stopped entry's end up to the next full minute, so a
        // start (or switch) within that minute is rejected as overlapping.
        // Retry once, beginning exactly where the latest entry ends — but only
        // if that end is recent, to avoid backdating after an unrelated 400.
        request('GET', '/api/timesheets?size=1', null, function (ok2, s2, data) {
          var end = ok2 && Array.isArray(data) && data[0] && data[0].end;
          if (end && new Date(end).getTime() > Date.now() - 120000) {
            startActivity(actId, pid, stripOffset(end));
          } else {
            sendError(errorCode(status));
            loadTimer();
          }
        });
        return;
      }
      sendError(errorCode(status));
      loadTimer();
    });
  });
}

// Stops every active timesheet, then calls cb(okAll, status). `status` carries
// the failing request's HTTP status (0 = network) so the watch can show the real
// error instead of a blanket "no connection".
function stopAllActive(cb) {
  request('GET', '/api/timesheets/active', null, function (ok, status, data) {
    if (!ok || !Array.isArray(data)) { cb(false, status); return; }
    var i = 0;
    var failed = 0;
    var failStatus = 0;
    function next() {
      if (i >= data.length) { cb(failed === 0, failStatus); return; }
      var id = data[i].id;
      i++;
      request('PATCH', '/api/timesheets/' + id + '/stop', null, function (ok2, s2) {
        if (!ok2) { failed++; failStatus = s2; }
        next();
      });
    }
    next();
  });
}

function stopActivity() {
  stopAllActive(function (ok, status) {
    if (!ok) { sendError(errorCode(status)); }
    loadTimer();
  });
}

function switchActivity(actId, projectId) {
  stopAllActive(function (ok, status) {
    if (!ok) { sendError(errorCode(status)); loadTimer(); return; }
    startActivity(actId, projectId);
  });
}

// --- Lifecycle ---------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  // Initial fallback load; the watch also drives these from its persisted context.
  loadProjects();
  loadActivities(getDefProject());
  loadTimer();
});

Pebble.addEventListener('appmessage', function (e) {
  var d = (e && e.payload) || {};
  var projectId = d.PROJECT_ID;
  if (d.REQUEST_PROJECTS !== undefined) {
    loadProjects();
  } else if (d.REQUEST_ACTIVITIES !== undefined) {
    loadActivities(projectId);
    loadTimer();
  } else if (d.REQUEST_TIMER !== undefined) {
    loadTimer();
  } else if (d.START_ACTIVITY !== undefined) {
    startActivity(parseInt(d.START_ACTIVITY, 10), projectId);
  } else if (d.STOP_ACTIVITY !== undefined) {
    stopActivity();
  } else if (d.SWITCH_ACTIVITY !== undefined) {
    switchActivity(parseInt(d.SWITCH_ACTIVITY, 10), projectId);
  }
});

// Current values used to pre-fill the settings form.
function settingsSnapshot() {
  return {
    url: getBase(),
    token: getToken(),
    langRaw: getLangRaw(),
    defProject: String(getDefProject() || ''),
    defCustomer: String(getDefCustomer() || '')
  };
}

// Rebuild the settings page with the live project/customer list, then open it.
Pebble.addEventListener('showConfiguration', function () {
  if (!configured()) {
    clay = new Clay(buildConfig(getPageLang(), null, null, settingsSnapshot()), null,
      { autoHandleEvents: false });
    Pebble.openURL(clay.generateUrl());
    return;
  }
  request('GET', '/api/projects?visible=1', null, function (ok, status, data) {
    if (ok && Array.isArray(data)) {
      localStorage.setItem('ALL_PROJECTS', JSON.stringify(data.map(normProject)));
    } else {
      console.log('showConfig: GET /api/projects failed (' + status + ')');
    }
    clay = new Clay(buildConfig(getPageLang(), getAllProjects(), getAllCustomers(), settingsSnapshot()),
      null, { autoHandleEvents: false });
    Pebble.openURL(clay.generateUrl());
  });
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) { return; }
  // Prefer the raw name-keyed settings; Clay wraps each value as { value: X }.
  var sRaw = clay.getSettings(e.response, false);
  var sConv = clay.getSettings(e.response);
  function unwrap(v) {
    return (v && typeof v === 'object' && !Array.isArray(v) && ('value' in v)) ? v.value : v;
  }
  function pick(name) {
    var c = [sRaw[name], sRaw[keys[name]], sConv[keys[name]]];
    for (var j = 0; j < c.length; j++) {
      if (c[j] !== undefined && c[j] !== null) return unwrap(c[j]);
    }
    return undefined;
  }

  var url = (pick('KIMAI_URL') || '').toString().trim();
  var token = (pick('KIMAI_TOKEN') || '').toString().trim();
  var defProject = parseInt(pick('DEFAULT_PROJECT'), 10) || 0;
  var defCustomer = parseInt(pick('DEFAULT_CUSTOMER'), 10) || 0;
  var lang = parseInt(pick('LANGUAGE'), 10);
  if (isNaN(lang)) { lang = 255; }  // 255 = Automatic (follow watch locale)
  // Token value is never logged.
  console.log('webviewclosed: urlSet=' + !!url + ' tokenLen=' + token.length +
              ' defProject=' + defProject + ' defCustomer=' + defCustomer + ' lang=' + lang);

  localStorage.setItem('KIMAI_URL', url);
  localStorage.setItem('KIMAI_TOKEN', token);
  localStorage.setItem('LANG', String(lang));
  localStorage.setItem('DEF_PROJECT', String(defProject || ''));
  localStorage.setItem('DEF_CUSTOMER', String(defCustomer || ''));

  // Language + booking defaults go to the watch (persisted there). A default of
  // 0 (="first/all") is intentionally sent so the watch can ignore it.
  var msg = {};
  msg[keys.LANGUAGE] = lang;
  msg[keys.DEFAULT_PROJECT] = defProject;
  msg[keys.DEFAULT_CUSTOMER] = defCustomer;
  Pebble.sendAppMessage(msg, function () {
    loadProjects();
    loadActivities(defProject || getDefProject());
    loadTimer();
  }, function () {
    loadProjects();
    loadActivities(defProject || getDefProject());
    loadTimer();
  });
});
