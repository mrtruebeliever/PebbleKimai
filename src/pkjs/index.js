var Clay = require('pebble-clay');
var buildConfig = require('./config').buildConfig;
var keys = require('message_keys');

// Clay is driven manually so the settings page can be rebuilt with the live
// project list each time it opens (autoHandleEvents:false).
var clay = new Clay(buildConfig(null, settingsSnapshot()), null, { autoHandleEvents: false });

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

// --- Persisted phone-side state (never sent to the watch) --------------------

function getBase() {
  var u = (localStorage.getItem('KIMAI_URL') || '').trim();
  return u.replace(/\/+$/, '');
}
function getToken() { return (localStorage.getItem('KIMAI_TOKEN') || '').trim(); }
function getDefProject() { return parseInt(localStorage.getItem('DEF_PROJECT'), 10) || 0; }
function getAllProjects() {
  try { return JSON.parse(localStorage.getItem('ALL_PROJECTS') || '[]'); }
  catch (e) { return []; }
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
      Pebble.sendAppMessage(msg, function () { i++; next(); },
        function () { console.log('act ' + i + ' send failed'); i++; next(); });
    }
    next();
  }, function () { console.log('act head send failed'); });
}

// --- Loads -------------------------------------------------------------------

// Fetches the global activities (project == null). Tries the server-side
// `globals` filter first; falls back to fetching all and filtering here.
function loadActivities() {
  if (!configured()) { sendLoad(LOAD_UNCONFIGURED); return; }
  sendLoad(LOAD_LOADING);
  request('GET', '/api/activities?globals=true&visible=1', null, function (ok, status, data) {
    if (ok && Array.isArray(data)) { streamActivities(data); return; }
    request('GET', '/api/activities?visible=1', null, function (ok2, s2, d2) {
      if (!ok2 || !Array.isArray(d2)) { sendLoad(LOAD_ERROR); return; }
      streamActivities(d2.filter(function (a) { return !a.project; }));
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

// Runs cb(projectId) with the project to book on: the configured default, or
// the first visible project as a fallback. cb(0) when none can be found.
function withProject(cb) {
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

function startActivity(actId, begin) {
  withProject(function (projectId) {
    if (!projectId) { sendError(ERR_NO_PROJECT); loadTimer(); return; }
    // No `begin` normally: the server stamps "now" in the Kimai user's own
    // timezone, which sidesteps phone-vs-server timezone mismatches entirely.
    var body = { project: projectId, activity: actId };
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
            startActivity(actId, stripOffset(end));
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

// Stops every active timesheet, then calls cb(okAll).
function stopAllActive(cb) {
  request('GET', '/api/timesheets/active', null, function (ok, status, data) {
    if (!ok || !Array.isArray(data)) { cb(false, status); return; }
    var i = 0;
    var failed = 0;
    function next() {
      if (i >= data.length) { cb(failed === 0, 0); return; }
      var id = data[i].id;
      i++;
      request('PATCH', '/api/timesheets/' + id + '/stop', null, function (ok2, s2) {
        if (!ok2) { failed++; }
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

function switchActivity(actId) {
  stopAllActive(function (ok, status) {
    if (!ok) { sendError(errorCode(status)); loadTimer(); return; }
    startActivity(actId);
  });
}

// --- Lifecycle ---------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  loadActivities();
  loadTimer();
});

Pebble.addEventListener('appmessage', function (e) {
  var d = (e && e.payload) || {};
  if (d.REQUEST_ACTIVITIES !== undefined) {
    loadActivities();
    loadTimer();
  } else if (d.REQUEST_TIMER !== undefined) {
    loadTimer();
  } else if (d.START_ACTIVITY !== undefined) {
    startActivity(parseInt(d.START_ACTIVITY, 10));
  } else if (d.STOP_ACTIVITY !== undefined) {
    stopActivity();
  } else if (d.SWITCH_ACTIVITY !== undefined) {
    switchActivity(parseInt(d.SWITCH_ACTIVITY, 10));
  }
});

// Current values used to pre-fill the settings form.
function settingsSnapshot() {
  return {
    url: getBase(),
    token: getToken(),
    defProject: String(getDefProject() || '')
  };
}

// Rebuild the settings page with the live project list, then open it.
Pebble.addEventListener('showConfiguration', function () {
  if (!configured()) {
    clay = new Clay(buildConfig(null, settingsSnapshot()), null, { autoHandleEvents: false });
    Pebble.openURL(clay.generateUrl());
    return;
  }
  request('GET', '/api/projects?visible=1', null, function (ok, status, data) {
    if (ok && Array.isArray(data)) {
      localStorage.setItem('ALL_PROJECTS', JSON.stringify(data.map(function (p) {
        return { id: p.id, name: String(p.name) };
      })));
    } else {
      console.log('showConfig: GET /api/projects failed (' + status + ')');
    }
    clay = new Clay(buildConfig(getAllProjects(), settingsSnapshot()), null,
      { autoHandleEvents: false });
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
  // Token value is never logged.
  console.log('webviewclosed: urlSet=' + !!url + ' tokenLen=' + token.length +
              ' defProject=' + defProject);

  localStorage.setItem('KIMAI_URL', url);
  localStorage.setItem('KIMAI_TOKEN', token);
  localStorage.setItem('DEF_PROJECT', String(defProject || ''));

  loadActivities();
  loadTimer();
});
