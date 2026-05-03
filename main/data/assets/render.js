function fmtEpoch(sec) {
  if (!sec) return '—';
  return new Date(sec * 1000).toLocaleString();
}

function showToast(message, isError = false) {
  const el = document.createElement('div');
  el.className = `toast${isError ? ' error' : ''}`;
  el.textContent = message;
  document.getElementById('toastHost').appendChild(el);
  setTimeout(() => el.remove(), 2800);
}

function buildField(name, label, type = 'text', step = null, value = '') {
  const wrapper = document.createElement('div');
  wrapper.className = 'field';
  const id = `field_${name}`;
  wrapper.innerHTML = `
    <label for="${id}">${label}</label>
    <input id="${id}" data-field="${name}" type="${type}" ${step ? `step="${step}"` : ''} value="${value ?? ''}" />
  `;
  return wrapper;
}

function renderRuntimeForm() {
  const host = document.getElementById('runtimeForm');
  host.innerHTML = '';
  runtimeFields.forEach(([name, label, type, step]) => {
    host.appendChild(buildField(name, label, type, step, state.config?.[name] ?? ''));
  });

  (state.config?.zones || []).forEach(zone => {
    host.appendChild(buildField(`zone_name_${zone.index}`, `Zone ${zone.index + 1} Name`, 'text', null, zone.name));
    const enabledField = buildField(`zone_enabled_${zone.index}`, `Zone ${zone.index + 1} Enabled (1/0)`, 'number', 1, zone.enabled ? 1 : 0);
    host.appendChild(enabledField);
  });
}

function renderNetworkForm() {
  const host = document.getElementById('networkForm');
  host.innerHTML = '';
  networkFields.forEach(([name, label, type]) => {
    host.appendChild(buildField(name, label, type, null, state.network?.[name] ?? ''));
  });
}

function zoneBadge(zone) {
  if (!zone.enabled) return ['Disabled', 'muted'];
  if (zone.faulted) return ['Fault', 'bad'];
  if (zone.active) return ['Active', 'good'];
  if (zone.timedOut) return ['Timed Out', 'warn'];
  if (zone.detectedWater) return ['Wet', 'good'];
  if (zone.attempted) return ['Dry', 'warn'];
  return ['Ready', 'muted'];
}

function renderStatus() {
  const s = state.status;
  if (!s) return;

  document.getElementById('systemEnabled').checked = !!s.enabled;
  document.getElementById('headerSubtext').textContent = `${s.appVersion} • ${s.cycleActive ? 'cycle active' : 'standing by'}`;
  document.getElementById('cycleState').textContent = s.cycleState || '—';
  document.getElementById('cycleEvent').textContent = s.cycleEvent || '—';
  document.getElementById('currentA').textContent = `${Number(s.currentA || 0).toFixed(2)} A`;
  document.getElementById('peakCurrentA').textContent = `Peak ${Number(s.peakCurrentA || 0).toFixed(2)} A`;
  document.getElementById('nextRun').textContent = fmtEpoch(s.nextRun);
  document.getElementById('lastRun').textContent = `Last ${fmtEpoch(s.lastRun)}`;

  const fault = document.getElementById('faultBanner');
  if (s.faultMessage) {
    fault.textContent = s.faultMessage;
    fault.classList.remove('hidden');
  } else {
    fault.classList.add('hidden');
  }

  const zoneHost = document.getElementById('zoneCards');
  zoneHost.innerHTML = '';
  (s.zones || []).forEach(zone => {
    const [badgeText, badgeClass] = zoneBadge(zone);
    const card = document.createElement('div');
    card.className = `zone-card${zone.active ? ' active' : ''}${zone.enabled ? '' : ' disabled'}`;
    card.innerHTML = `
      <div class="zone-title">
        <strong>${zone.name}</strong>
        <span class="badge ${badgeClass}">${badgeText}</span>
      </div>
      <div class="metric-list">
        <div>Duration: ${(Number(zone.durationMs || 0) / 1000).toFixed(1)} s</div>
        <div>Peak current: ${Number(zone.peakCurrentA || 0).toFixed(2)} A</div>
        <div>End current: ${Number(zone.endCurrentA || 0).toFixed(2)} A</div>
        <div>Last success: ${fmtEpoch(zone.lastSuccessEpoch)}</div>
      </div>
    `;
    zoneHost.appendChild(card);
  });
}

function renderNetwork() {
  const n = state.network;
  if (!n) return;
  document.getElementById('networkMode').textContent = n.mode || '—';
  document.getElementById('networkIp').textContent = n.ip || '—';
  document.getElementById('networkSsid').textContent = n.currentSSID || '—';
  document.getElementById('networkMdns').textContent = n.mdnsHost ? `${n.mdnsHost}.local` : '—';
}

function renderLogs() {
  const host = document.getElementById('logList');
  host.innerHTML = '';
  state.logs.forEach(entry => {
    const row = document.createElement('div');
    row.className = 'log-entry';
    row.innerHTML = `
      <div>${entry.message || ''}</div>
      <div class="log-meta">${entry.level || 'INFO'} • ${fmtEpoch(entry.epoch)}</div>
    `;
    host.appendChild(row);
  });
}
