async function loadAll() {
  const [status, config, network, logs] = await Promise.all([
    api.getStatus(),
    api.getConfig(),
    api.getNetwork(),
    api.getLogs()
  ]);

  state.status = status;
  state.config = config;
  state.network = network;
  state.logs = logs;

  renderStatus();
  renderRuntimeForm();
  renderNetwork();
  renderNetworkForm();
  renderLogs();
}

function readRuntimeForm() {
  const payload = {
    systemEnabled: document.getElementById('systemEnabled').checked,
    demoMode: !!state.config?.demoMode,
    zones: []
  };

  runtimeFields.forEach(([name, _label, type]) => {
    const el = document.getElementById(`field_${name}`);
    payload[name] = type === 'number' ? Number(el.value) : el.value;
  });

  (state.config?.zones || []).forEach(zone => {
    payload.zones.push({
      index: zone.index,
      name: document.getElementById(`field_zone_name_${zone.index}`).value,
      enabled: Number(document.getElementById(`field_zone_enabled_${zone.index}`).value) === 1
    });
  });

  return payload;
}

function readNetworkForm() {
  const payload = {};
  networkFields.forEach(([name]) => {
    payload[name] = document.getElementById(`field_${name}`).value;
  });
  return payload;
}

async function refreshStatusOnly() {
  state.status = await api.getStatus();
  renderStatus();
}

window.addEventListener('load', async () => {
  try {
    await loadAll();
  } catch (err) {
    console.error(err);
    showToast('Initial load failed', true);
  }

  setInterval(async () => {
    try {
      await refreshStatusOnly();
    } catch (err) {
      console.error(err);
    }
  }, 2000);

  setInterval(async () => {
    try {
      state.logs = await api.getLogs();
      renderLogs();
    } catch (err) {
      console.error(err);
    }
  }, 5000);

  document.getElementById('systemEnabled').addEventListener('change', async (e) => {
    try {
      await api.setEnabled(e.target.checked);
      showToast(e.target.checked ? 'System enabled' : 'System disabled');
      await refreshStatusOnly();
    } catch (err) {
      console.error(err);
      showToast('Failed to update system state', true);
    }
  });

  document.getElementById('runNowBtn').addEventListener('click', async () => {
    try {
      await api.runNow();
      showToast('Run queued');
      await refreshStatusOnly();
    } catch (err) {
      console.error(err);
      showToast('Failed to queue run', true);
    }
  });

  document.getElementById('abortBtn').addEventListener('click', async () => {
    try {
      await api.abort();
      showToast('Abort requested');
    } catch (err) {
      console.error(err);
      showToast('Abort failed', true);
    }
  });

  document.getElementById('saveRuntimeBtn').addEventListener('click', async () => {
    try {
      await api.saveConfig(readRuntimeForm());
      state.config = await api.getConfig();
      renderRuntimeForm();
      showToast('Runtime saved');
    } catch (err) {
      console.error(err);
      showToast('Failed to save runtime', true);
    }
  });

  document.getElementById('saveNetworkBtn').addEventListener('click', async () => {
    try {
      await api.saveNetwork(readNetworkForm());
      state.network = await api.getNetwork();
      renderNetwork();
      renderNetworkForm();
      showToast('Network saved. Reboot required.');
    } catch (err) {
      console.error(err);
      showToast('Failed to save network', true);
    }
  });

  document.getElementById('rebootBtn').addEventListener('click', async () => {
    try {
      await api.reboot();
      showToast('Rebooting');
    } catch (err) {
      console.error(err);
      showToast('Failed to reboot', true);
    }
  });

  document.getElementById('refreshLogsBtn').addEventListener('click', async () => {
    try {
      state.logs = await api.getLogs();
      renderLogs();
    } catch (err) {
      console.error(err);
      showToast('Failed to refresh logs', true);
    }
  });

  document.getElementById('clearLogsBtn').addEventListener('click', async () => {
    try {
      await api.clearLogs();
      state.logs = await api.getLogs();
      renderLogs();
      showToast('Logs cleared');
    } catch (err) {
      console.error(err);
      showToast('Failed to clear logs', true);
    }
  });
});
