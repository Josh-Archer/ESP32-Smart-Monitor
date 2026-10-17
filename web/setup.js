// QR setup payloads for Wi-Fi join, the device dashboard, and Home Assistant.
// The Wi-Fi password is used only in this browser to draw a QR code.
// It is not fetched, posted, stored, or written into the page text.

(function (root, factory) {
  var api = factory();
  if (typeof module === 'object' && module.exports) {
    module.exports = api;
  }
  root.esp32Setup = api;
  if (typeof document !== 'undefined') {
    var boot = function () {
      if (document.getElementById('setup-app')) {
        api.initPage();
      }
    };
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', boot);
    } else {
      boot();
    }
  }
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  var LIMIT_SSID = 32;
  var LIMIT_PASSWORD = 63;
  var LIMIT_NAME = 64;
  var LIMIT_URL = 200;
  var LIMIT_HOST = 253;
  var LIMIT_DISCOVERY = 64;
  var LIMIT_DEVICE = 64;

  var DEMO_INFO = {
    v: 1,
    device: 'poop-monitor',
    version: '0.0.0-demo',
    ip: '192.0.2.10',
    mdns: 'poop-monitor.local',
    dashboard_url: 'http://192.0.2.10/',
    mdns_url: 'http://poop-monitor.local/',
    wifi: {ssid: 'ExampleNet', security: 'WPA'},
    home_assistant: {
      mqtt_enabled: true,
      mqtt_host: '192.0.2.20',
      mqtt_port: 1883,
      discovery_prefix: 'homeassistant',
      device_id: 'esp32_poop_monitor',
      device_name: 'ESP32 Poop Monitor',
      status_topic: 'homeassistant/sensor/poop_monitor/status',
      availability_topic: 'homeassistant/sensor/poop_monitor/availability'
    }
  };

  function fail(message) {
    var err = new Error(message);
    err.code = message;
    throw err;
  }

  function asText(value) {
    if (value === undefined || value === null) {
      return '';
    }
    return String(value);
  }

  function checkText(value, maxLen, allowEmpty) {
    var s = asText(value);
    if (!s) {
      if (allowEmpty) {
        return '';
      }
      fail('invalid');
    }
    if (s.length > maxLen) {
      fail('too long');
    }
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      if (c < 0x20 || c === 0x7f) {
        fail('invalid');
      }
    }
    return s;
  }

  function eqCi(a, b) {
    return asText(a).toLowerCase() === asText(b).toLowerCase();
  }

  function normalizeAuth(auth, havePassword) {
    if (!havePassword) {
      return 'nopass';
    }
    if (eqCi(auth, 'nopass')) {
      return 'nopass';
    }
    if (eqCi(auth, 'wep')) {
      return 'WEP';
    }
    return 'WPA';
  }

  function escapeWifiField(input) {
    var out = '';
    for (var i = 0; i < input.length; i++) {
      var ch = input.charAt(i);
      if (ch === '\\' || ch === ';' || ch === ',' || ch === ':' || ch === '"') {
        out += '\\';
      }
      out += ch;
    }
    return out;
  }

  function buildWifiQrPayload(opts) {
    opts = opts || {};
    var ssid = checkText(opts.ssid, LIMIT_SSID, false);
    var password = asText(opts.password);
    var havePassword = password.length > 0;
    if (havePassword) {
      password = checkText(password, LIMIT_PASSWORD, false);
    }
    var security = normalizeAuth(opts.auth, havePassword);
    if (security === 'nopass') {
      havePassword = false;
    }
    var payload = 'WIFI:T:' + security + ';S:' + escapeWifiField(ssid);
    if (havePassword) {
      payload += ';P:' + escapeWifiField(password);
    }
    payload += opts.hidden ? ';H:true;;' : ';H:false;;';
    return payload;
  }

  function buildDashboardQrPayload(url) {
    var value = checkText(url, LIMIT_URL, false);
    var rest;
    if (value.indexOf('https://') === 0) {
      rest = value.slice(8);
    } else if (value.indexOf('http://') === 0) {
      rest = value.slice(7);
    } else {
      fail('invalid');
    }
    if (!rest || rest.charAt(0) === '/') {
      fail('invalid');
    }
    if (/[@ "'<>\\]/.test(value)) {
      fail('invalid');
    }
    return value;
  }

  function buildHaQrPayload(opts) {
    opts = opts || {};
    var name = checkText(opts.name, LIMIT_NAME, true);
    var url = asText(opts.dashboardUrl || opts.url);
    if (url) {
      url = buildDashboardQrPayload(url);
    }
    var host = checkText(opts.mqttHost || opts.host, LIMIT_HOST, true);
    if (host && /[:/@ ]/.test(host)) {
      fail('invalid');
    }
    var port = opts.mqttPort;
    if (port === undefined || port === null || port === '') {
      port = opts.port;
    }
    if (port === undefined || port === null || port === '') {
      port = 0;
    }
    port = Number(port);
    if (!isFinite(port) || Math.floor(port) !== port || port < 0 || port > 65535) {
      fail('invalid');
    }
    var discovery = checkText(opts.discovery, LIMIT_DISCOVERY, true);
    var device = checkText(opts.deviceId || opts.device, LIMIT_DEVICE, true);
    var mqttValue = '';
    if (host) {
      mqttValue = port > 0 ? (host + ':' + port) : host;
    }
    var payload = 'esp32mon://setup?v=1';
    function add(key, value) {
      if (!value) {
        return;
      }
      payload += '&' + key + '=' + encodeURIComponent(value);
    }
    add('name', name);
    add('url', url);
    add('mqtt', mqttValue);
    add('discovery', discovery);
    add('device', device);
    return payload;
  }

  function renderQrSvg(text, title) {
    if (typeof qrcode !== 'function') {
      fail('invalid');
    }
    var qr = qrcode(0, 'M');
    qr.addData(text);
    qr.make();
    return qr.createSvgTag({
      cellSize: 4,
      margin: 2,
      scalable: true,
      title: title || 'Setup QR code'
    });
  }

  function deviceBase() {
    var params = new URLSearchParams(location.search);
    var given = params.get('device');
    if (given) {
      return given.replace(/\/+$/, '');
    }
    var path = location.pathname || '';
    if (path === '/setup' || path === '/setup/') {
      return location.origin;
    }
    return 'http://poop-monitor.local';
  }

  function secretKey(name) {
    var key = String(name).toLowerCase();
    return key.indexOf('password') !== -1 ||
      key.indexOf('token') !== -1 ||
      key.indexOf('secret') !== -1 ||
      key === 'mqtt_user' ||
      key === 'user';
  }

  function assertNoSecrets(value) {
    if (!value || typeof value !== 'object') {
      return;
    }
    var keys = Object.keys(value);
    for (var i = 0; i < keys.length; i++) {
      if (secretKey(keys[i])) {
        fail('invalid');
      }
      assertNoSecrets(value[keys[i]]);
    }
  }

  function fetchInfo(base) {
    return fetch(base.replace(/\/+$/, '') + '/setup/info', {
      method: 'GET',
      cache: 'no-store'
    }).then(function (response) {
      if (!response.ok) {
        throw new Error('status ' + response.status);
      }
      return response.json();
    }).then(function (data) {
      assertNoSecrets(data);
      return data;
    });
  }

  function setStatus(text) {
    var el = document.getElementById('setup-status');
    if (el) {
      el.textContent = text;
    }
  }

  function setText(id, text) {
    var el = document.getElementById(id);
    if (el) {
      el.textContent = text || '';
    }
  }

  function setValue(id, value) {
    var el = document.getElementById(id);
    if (el && value !== undefined && value !== null) {
      el.value = value;
    }
  }

  function renderInto(id, payload, title) {
    var el = document.getElementById(id);
    if (!el) {
      return;
    }
    if (!payload) {
      el.innerHTML = '';
      return;
    }
    try {
      el.innerHTML = renderQrSvg(payload, title);
    } catch (err) {
      el.innerHTML = '';
    }
  }

  function readHaOpts() {
    return {
      name: document.getElementById('ha-name').value,
      dashboardUrl: document.getElementById('dashboard-url').value,
      mqttHost: document.getElementById('mqtt-host').value,
      mqttPort: document.getElementById('mqtt-port').value,
      discovery: document.getElementById('discovery').value,
      deviceId: document.getElementById('device-id').value
    };
  }

  function updateDashboardQr() {
    var url = document.getElementById('dashboard-url').value.trim();
    var link = document.getElementById('dashboard-link');
    if (!url) {
      renderInto('dashboard-qr', '', '');
      setText('dashboard-payload', '');
      if (link) {
        link.textContent = '';
        link.removeAttribute('href');
      }
      return;
    }
    try {
      var payload = buildDashboardQrPayload(url);
      renderInto('dashboard-qr', payload, 'Open the device');
      setText('dashboard-payload', payload);
      if (link) {
        link.textContent = payload;
        link.href = payload;
      }
    } catch (err) {
      renderInto('dashboard-qr', '', '');
      setText('dashboard-payload', '');
    }
  }

  function updateHaQr() {
    try {
      var payload = buildHaQrPayload(readHaOpts());
      renderInto('ha-qr', payload, 'Home Assistant onboarding');
      setText('ha-payload', payload);
    } catch (err) {
      renderInto('ha-qr', '', '');
      setText('ha-payload', '');
    }
  }

  function updateWifiQr() {
    var ssid = document.getElementById('ssid').value;
    var password = document.getElementById('wifi-password').value;
    var auth = document.getElementById('security').value;
    var hidden = document.getElementById('hidden').checked;
    var hint = document.getElementById('wifi-hint');
    if (!ssid) {
      renderInto('wifi-qr', '', '');
      if (hint) {
        hint.textContent = 'Enter the network name to generate a join code.';
      }
      return;
    }
    if (auth !== 'nopass' && !password) {
      renderInto('wifi-qr', '', '');
      if (hint) {
        hint.textContent = 'Enter the Wi-Fi password here to draw the join code. It stays in this browser and is not sent to the ESP32.';
      }
      return;
    }
    try {
      var payload = buildWifiQrPayload({
        ssid: ssid,
        password: password,
        auth: auth,
        hidden: hidden
      });
      renderInto('wifi-qr', payload, 'Wi-Fi join code');
      if (hint) {
        hint.textContent = auth === 'nopass'
          ? 'Open network. Scan with the phone camera to join.'
          : 'Scan with the phone camera to join. The password was used only to draw this code.';
      }
    } catch (err) {
      renderInto('wifi-qr', '', '');
      if (hint) {
        hint.textContent = 'Could not build a Wi-Fi code from those values.';
      }
    }
  }

  function applyInfo(info) {
    info = info || {};
    var ha = info.home_assistant || {};
    var wifi = info.wifi || {};
    var label = info.device || 'ESP32';
    if (info.version) {
      label += ' · ' + info.version;
    }
    setText('device-line', label);
    setValue('ssid', wifi.ssid || '');
    if (wifi.security) {
      setValue('security', wifi.security);
    }
    setValue('dashboard-url', info.dashboard_url || info.mdns_url || '');
    setText('mdns-line', info.mdns_url || '');
    setValue('ha-name', ha.device_name || info.device || '');
    setValue('mqtt-host', ha.mqtt_host || '');
    setValue('mqtt-port', ha.mqtt_port ? String(ha.mqtt_port) : '');
    setValue('discovery', ha.discovery_prefix || '');
    setValue('device-id', ha.device_id || '');
    setText('status-topic', ha.status_topic || '');
    setText('availability-topic', ha.availability_topic || '');
    var mqttNote = document.getElementById('mqtt-note');
    if (mqttNote) {
      mqttNote.textContent = ha.mqtt_enabled === false
        ? 'This firmware build does not publish Home Assistant MQTT discovery.'
        : 'MQTT username and password are not included. Put those only in the broker and in the gitignored credentials file.';
    }
    updateDashboardQr();
    updateHaQr();
    updateWifiQr();
  }

  function bindForm() {
    var form = document.getElementById('wifi-form');
    if (form) {
      form.addEventListener('submit', function (event) {
        event.preventDefault();
        updateWifiQr();
      });
    }
    ['ssid', 'wifi-password', 'security', 'hidden'].forEach(function (id) {
      var el = document.getElementById(id);
      if (el) {
        el.addEventListener('input', updateWifiQr);
        el.addEventListener('change', updateWifiQr);
      }
    });
    var clearBtn = document.getElementById('clear-password');
    if (clearBtn) {
      clearBtn.addEventListener('click', function () {
        document.getElementById('wifi-password').value = '';
        updateWifiQr();
      });
    }
    ['dashboard-url', 'ha-name', 'mqtt-host', 'mqtt-port', 'discovery', 'device-id'].forEach(function (id) {
      var el = document.getElementById(id);
      if (el) {
        el.addEventListener('input', function () {
          updateDashboardQr();
          updateHaQr();
        });
      }
    });
    var reload = document.getElementById('reload-device');
    if (reload) {
      reload.addEventListener('click', function () {
        loadFromDevice();
      });
    }
  }

  function loadFromDevice() {
    setStatus('Loading setup data from the device…');
    return fetchInfo(deviceBase()).then(function (info) {
      applyInfo(info);
      setStatus('Loaded setup data from the device. The Wi-Fi password was not included.');
    }).catch(function () {
      setStatus('Device setup data is unavailable. You can still type the network name, dashboard URL, and Home Assistant fields. Nothing you type is uploaded.');
    });
  }

  function runSelfTest() {
    var result = document.getElementById('selftest-result');
    function finish(text) {
      if (result) {
        result.hidden = false;
        result.textContent = text;
      }
    }
    try {
      applyInfo(DEMO_INFO);
      var wifi = buildWifiQrPayload({
        ssid: 'ExampleNet',
        password: 'unit-test-pass',
        auth: 'WPA',
        hidden: false
      });
      if (wifi.indexOf('WIFI:T:WPA;S:ExampleNet;P:') !== 0 || !wifi.endsWith(';H:false;;')) {
        fail('invalid');
      }
      renderInto('wifi-qr', wifi, 'Wi-Fi join code');
      var dash = buildDashboardQrPayload('http://192.0.2.10/');
      if (dash !== 'http://192.0.2.10/') {
        fail('invalid');
      }
      var ha = buildHaQrPayload({
        name: 'ESP32 Poop Monitor',
        dashboardUrl: 'http://192.0.2.10/',
        mqttHost: '192.0.2.20',
        mqttPort: 1883,
        discovery: 'homeassistant',
        deviceId: 'esp32_poop_monitor'
      });
      if (ha.indexOf('esp32mon://setup?v=1') !== 0 || ha.indexOf('discovery=homeassistant') < 0) {
        fail('invalid');
      }
      if (ha.indexOf('password') !== -1) {
        fail('invalid');
      }
      var boxes = ['wifi-qr', 'dashboard-qr', 'ha-qr'];
      for (var i = 0; i < boxes.length; i++) {
        var el = document.getElementById(boxes[i]);
        if (!el || el.innerHTML.indexOf('<svg') < 0) {
          fail('invalid');
        }
      }
      var surfaced = document.getElementById('setup-app').innerHTML;
      var typed = document.getElementById('wifi-password').value || '';
      if ((surfaced + typed).indexOf('unit-test-pass') !== -1) {
        fail('invalid');
      }
      finish('selftest: ok');
    } catch (err) {
      finish('selftest: fail');
    }
  }

  function initPage() {
    bindForm();
    var params = new URLSearchParams(location.search);
    if (params.has('selftest')) {
      runSelfTest();
      return;
    }
    if (params.has('demo')) {
      applyInfo(DEMO_INFO);
      setStatus('Showing demo data. The Wi-Fi password is not included. Type one locally to draw a join code.');
      return;
    }
    loadFromDevice();
  }

  return {
    DEMO_INFO: DEMO_INFO,
    buildWifiQrPayload: buildWifiQrPayload,
    buildDashboardQrPayload: buildDashboardQrPayload,
    buildHaQrPayload: buildHaQrPayload,
    renderQrSvg: renderQrSvg,
    assertNoSecrets: assertNoSecrets,
    initPage: initPage
  };
});
