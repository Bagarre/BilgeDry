
fetch('/api/v1/config')
  .then(res => res.json())
  .then(config => {
    document.getElementById('ssid').value = config.ssid || '';
    document.getElementById('password').value = config.password || '';
    document.getElementById('interval').value = config.interval || 60;

    const zoneDiv = document.getElementById('zone-calibration');
    zoneDiv.innerHTML = '';
    const zones = config.zones || [];

    zones.forEach((zone, i) => {
      const label = document.createElement('div');
      label.className = 'card';
      label.innerHTML = `
        <label>Zone ${i + 1} Name:
          <input type="text" name="zone-name-${i}" value="${zone.name || ''}">
        </label>
        <label>Pin:
          <input type="number" name="zone-pin-${i}" value="${zone.pin}" min="0" max="39">
        </label>
        <label>Amps:
          <input type="number" step="0.1" name="zone-amp-${i}" value="${zone.amp}">
        </label>
        <label>Enabled:
          <label class="switch">
            <input type="checkbox" name="zone-enabled-${i}" ${zone.enabled ? 'checked' : ''}>
            <span class="slider"></span>
          </label>
        </label>
      `;
      zoneDiv.appendChild(label);
    });
  });

document.getElementById('config-form').addEventListener('submit', e => {
  e.preventDefault();
  const body = {
    ssid: document.getElementById('ssid').value,
    password: document.getElementById('password').value,
    interval: parseInt(document.getElementById('interval').value, 10),
    zones: []
  };

  let i = 0;
  while (document.querySelector(`[name="zone-pin-${i}"]`)) {
    const name = document.querySelector(`[name="zone-name-${i}"]`).value;
    const pin = parseInt(document.querySelector(`[name="zone-pin-${i}"]`).value, 10);
    const amp = parseFloat(document.querySelector(`[name="zone-amp-${i}"]`).value);
    const enabled = document.querySelector(`[name="zone-enabled-${i}"]`).checked;
    if (isNaN(pin) || pin < 0 || pin > 39) {
      alert(`Zone ${i + 1} pin is invalid (must be 0-39)`);
      return;
    }
    body.zones.push({ name, pin, amp, enabled });
    i++;
  }

  fetch('/api/v1/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(res => alert('Configuration saved'));
});
