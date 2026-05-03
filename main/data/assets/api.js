const api = {
  getStatus: () => fetch('/api/v1/status').then(r => r.json()),
  getConfig: () => fetch('/api/v1/config').then(r => r.json()),
  saveConfig: (body) => fetch('/api/v1/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(r => r.json()),
  getNetwork: () => fetch('/api/v1/network').then(r => r.json()),
  saveNetwork: (body) => fetch('/api/v1/network', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(r => r.json()),
  setEnabled: (enabled) => fetch('/api/v1/control/enable', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ enabled })
  }).then(r => r.json()),
  runNow: () => fetch('/api/v1/control/run', { method: 'POST' }).then(r => r.json()),
  abort: () => fetch('/api/v1/control/abort', { method: 'POST' }).then(r => r.json()),
  reboot: () => fetch('/api/v1/reboot', { method: 'POST' }).then(r => r.json()),
  getLogs: () => fetch('/api/v1/logs').then(r => r.json()),
  clearLogs: () => fetch('/api/v1/logs/clear', { method: 'POST' }).then(r => r.json())
};
