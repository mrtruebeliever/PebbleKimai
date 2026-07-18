// Clay settings page, built at open time so the project select can be filled
// with the live project list (fetched by index.js when a URL + token are set).
// `projects` is [{id,name}] or null; `snap` pre-fills the current values.
function buildConfig(projects, snap) {
  snap = snap || {};

  var projectItems;
  if (projects && projects.length) {
    var options = [{ label: '(first project)', value: '' }].concat(
      projects.map(function (p) {
        return { label: p.name, value: String(p.id) };
      }));
    projectItems = [{
      type: 'select',
      messageKey: 'DEFAULT_PROJECT',
      label: 'Project to book time on',
      defaultValue: snap.defProject || '',
      options: options
    }];
  } else {
    projectItems = [{
      type: 'text',
      defaultValue: 'Save a server URL and API token first, then reopen this page to pick the project to book time on.'
    }];
  }

  return [
    { type: 'heading', defaultValue: 'PebbleKimai' },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: 'Kimai server' },
        {
          type: 'input',
          messageKey: 'KIMAI_URL',
          label: 'Server URL',
          defaultValue: snap.url || '',
          attributes: { placeholder: 'https://kimai.example.com', type: 'url' }
        },
        {
          type: 'input',
          messageKey: 'KIMAI_TOKEN',
          label: 'API token',
          defaultValue: snap.token || '',
          attributes: { type: 'password' },
          description: 'Create one in Kimai under your user profile > API Access.'
        }
      ]
    },
    { type: 'section', items: [{ type: 'heading', defaultValue: 'Booking' }].concat(projectItems) },
    { type: 'submit', defaultValue: 'Save' }
  ];
}

module.exports = { buildConfig: buildConfig };
