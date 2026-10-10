// Settings-page translations + Clay config builder. Language index matches the
// watch side (src/c/i18n.h): 0=en, 1=nl, 2=fr, 3=de, 4=es.
var LANGS = ['en', 'nl', 'fr', 'de', 'es'];

var L = {
  en: {
    server_heading: 'Kimai server', url_label: 'Server URL',
    token_label: 'API token',
    token_desc: 'Create one in Kimai under your user profile > API Access. It stays on your phone only and is never sent to the watch.',
    general_heading: 'General', language: 'Language', automatic: 'Automatic (watch language)',
    sound_label: 'Sound on start/stop', sound_desc: 'A short tone when you start or stop the timer (rising to start, falling to stop). Pebble Time 2 only; silent during Quiet Time or when the speaker is muted.',
    booking_heading: 'Booking',
    project_label: 'Default project', project_first: '(first project)',
    customer_label: 'Default customer', customer_any: '(all customers)',
    booking_desc: 'The starting project and customer. You can also switch them on the watch (tap or long-press the project row); your latest choice is remembered there.',
    notoken: 'Save a server URL and API token first, then reopen this page to pick the default project and customer.',
    save: 'Save'
  },
  nl: {
    server_heading: 'Kimai-server', url_label: 'Server-URL',
    token_label: 'API-token',
    token_desc: 'Maak er een aan in Kimai onder je gebruikersprofiel > API-toegang. De token blijft alleen op je telefoon en wordt nooit naar het horloge gestuurd.',
    general_heading: 'Algemeen', language: 'Taal', automatic: 'Automatisch (horlogetaal)',
    sound_label: 'Geluid bij starten/stoppen', sound_desc: 'Een kort toontje als je de timer start of stopt (stijgend bij start, dalend bij stop). Alleen Pebble Time 2; stil tijdens Quiet Time of als de speaker gedempt is.',
    booking_heading: 'Boeken',
    project_label: 'Standaardproject', project_first: '(eerste project)',
    customer_label: 'Standaardklant', customer_any: '(alle klanten)',
    booking_desc: 'Het beginproject en de beginklant. Je kunt ze ook op het horloge wisselen (tik of houd de projectrij ingedrukt); je laatste keuze wordt daar onthouden.',
    notoken: 'Sla eerst een server-URL en API-token op, heropen daarna deze pagina om het standaardproject en de standaardklant te kiezen.',
    save: 'Opslaan'
  },
  fr: {
    server_heading: 'Serveur Kimai', url_label: 'URL du serveur',
    token_label: 'Jeton API',
    token_desc: 'Créez-en un dans Kimai sous votre profil > Accès API. Il reste uniquement sur votre téléphone et n\'est jamais envoyé à la montre.',
    general_heading: 'Général', language: 'Langue', automatic: 'Automatique (langue de la montre)',
    sound_label: 'Son au démarrage/arrêt', sound_desc: 'Un court son quand vous démarrez ou arrêtez le chronomètre (montant au démarrage, descendant à l\'arrêt). Pebble Time 2 uniquement ; muet pendant Quiet Time ou si le haut-parleur est coupé.',
    booking_heading: 'Saisie',
    project_label: 'Projet par défaut', project_first: '(premier projet)',
    customer_label: 'Client par défaut', customer_any: '(tous les clients)',
    booking_desc: 'Le projet et le client de départ. Vous pouvez aussi les changer sur la montre (touchez ou appui long sur la ligne du projet) ; votre dernier choix y est mémorisé.',
    notoken: 'Enregistrez d\'abord une URL de serveur et un jeton API, puis rouvrez cette page pour choisir le projet et le client par défaut.',
    save: 'Enregistrer'
  },
  de: {
    server_heading: 'Kimai-Server', url_label: 'Server-URL',
    token_label: 'API-Token',
    token_desc: 'Erstelle einen in Kimai unter deinem Benutzerprofil > API-Zugriff. Er bleibt nur auf deinem Telefon und wird nie an die Uhr gesendet.',
    general_heading: 'Allgemein', language: 'Sprache', automatic: 'Automatisch (Uhrsprache)',
    sound_label: 'Ton bei Start/Stopp', sound_desc: 'Ein kurzer Ton beim Starten oder Stoppen des Timers (steigend beim Start, fallend beim Stopp). Nur Pebble Time 2; stumm während Quiet Time oder bei stummgeschaltetem Lautsprecher.',
    booking_heading: 'Buchung',
    project_label: 'Standardprojekt', project_first: '(erstes Projekt)',
    customer_label: 'Standardkunde', customer_any: '(alle Kunden)',
    booking_desc: 'Das Start-Projekt und der Start-Kunde. Du kannst sie auch auf der Uhr wechseln (Projektzeile tippen oder lang drücken); deine letzte Wahl wird dort gemerkt.',
    notoken: 'Speichere zuerst eine Server-URL und einen API-Token, öffne diese Seite dann erneut, um Standardprojekt und Standardkunde zu wählen.',
    save: 'Speichern'
  },
  es: {
    server_heading: 'Servidor Kimai', url_label: 'URL del servidor',
    token_label: 'Token de API',
    token_desc: 'Crea uno en Kimai en tu perfil de usuario > Acceso API. Permanece solo en tu teléfono y nunca se envía al reloj.',
    general_heading: 'General', language: 'Idioma', automatic: 'Automático (idioma del reloj)',
    sound_label: 'Sonido al iniciar/detener', sound_desc: 'Un tono breve al iniciar o detener el temporizador (ascendente al iniciar, descendente al detener). Solo Pebble Time 2; silencio durante Quiet Time o con el altavoz silenciado.',
    booking_heading: 'Registro',
    project_label: 'Proyecto predeterminado', project_first: '(primer proyecto)',
    customer_label: 'Cliente predeterminado', customer_any: '(todos los clientes)',
    booking_desc: 'El proyecto y el cliente iniciales. También puedes cambiarlos en el reloj (toca o mantén pulsada la fila del proyecto); tu última elección se recuerda allí.',
    notoken: 'Guarda primero una URL de servidor y un token de API, luego vuelve a abrir esta página para elegir el proyecto y el cliente predeterminados.',
    save: 'Guardar'
  }
};

// langIdx: language to render the page in; projects: [{id,name,customerId,customer}]
// or null; customers: [{id,name}] or null; snap: prefill values.
function buildConfig(langIdx, projects, customers, snap) {
  snap = snap || {};
  var s = L[LANGS[langIdx] || 'en'];

  var items = [
    { type: 'heading', defaultValue: 'PebbleKimai' },
    { type: 'section', items: [
      { type: 'heading', defaultValue: s.server_heading },
      { type: 'input', messageKey: 'KIMAI_URL', label: s.url_label,
        defaultValue: snap.url || '',
        attributes: { placeholder: 'https://kimai.example.com', type: 'url' } },
      { type: 'input', messageKey: 'KIMAI_TOKEN', label: s.token_label,
        defaultValue: snap.token || '',
        attributes: { type: 'password' }, description: s.token_desc }
    ] },
    { type: 'section', items: [
      { type: 'heading', defaultValue: s.general_heading },
      { type: 'select', messageKey: 'LANGUAGE', label: s.language,
        defaultValue: (snap.langRaw != null && snap.langRaw !== '') ? String(snap.langRaw) : '255',
        options: [
          { label: s.automatic, value: '255' },
          { label: 'English', value: '0' },
          { label: 'Nederlands', value: '1' },
          { label: 'Français', value: '2' },
          { label: 'Deutsch', value: '3' },
          { label: 'Español', value: '4' }
        ] },
      { type: 'toggle', messageKey: 'TIMER_SOUND', label: s.sound_label,
        defaultValue: !!snap.timerSound, description: s.sound_desc }
    ] }
  ];

  if (projects && projects.length) {
    var projOptions = [{ label: s.project_first, value: '' }].concat(
      projects.map(function (p) { return { label: p.name, value: String(p.id) }; }));

    var bookingItems = [
      { type: 'heading', defaultValue: s.booking_heading },
      { type: 'text', defaultValue: s.booking_desc },
      { type: 'select', messageKey: 'DEFAULT_PROJECT', label: s.project_label,
        defaultValue: snap.defProject || '', options: projOptions }
    ];

    // Only offer a customer default when the instance has more than one customer.
    if (customers && customers.length > 1) {
      var custOptions = [{ label: s.customer_any, value: '' }].concat(
        customers.map(function (c) { return { label: c.name, value: String(c.id) }; }));
      bookingItems.push({ type: 'select', messageKey: 'DEFAULT_CUSTOMER', label: s.customer_label,
        defaultValue: snap.defCustomer || '', options: custOptions });
    }

    items.push({ type: 'section', items: bookingItems });
  } else {
    items.push({ type: 'section', items: [{ type: 'text', defaultValue: s.notoken }] });
  }

  items.push({ type: 'submit', defaultValue: s.save });
  return items;
}

module.exports = { LANGS: LANGS, buildConfig: buildConfig };
