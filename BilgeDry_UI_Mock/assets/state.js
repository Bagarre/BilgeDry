const state = {
  status: null,
  config: null,
  network: null,
  logs: []
};

const runtimeFields = [
  ['runIntervalHours', 'Run Interval (hours)', 'number', 1],
  ['primeDelayMs', 'Prime Delay (ms)', 'number', 100],
  ['zoneMaxRunMs', 'Zone Max Run (ms)', 'number', 100],
  ['sampleIntervalMs', 'Sample Interval (ms)', 'number', 10],
  ['settleDelayMs', 'Settle Delay (ms)', 'number', 10],
  ['wetCurrentA', 'Wet Threshold (A)', 'number', 0.1],
  ['dryCurrentA', 'Dry Threshold (A)', 'number', 0.1],
  ['faultCurrentA', 'Fault Threshold (A)', 'number', 0.1]
];

const networkFields = [
  ['wifiSSID', 'Wi‑Fi SSID', 'text'],
  ['wifiPassword', 'Wi‑Fi Password', 'password'],
  ['apSSID', 'AP SSID', 'text'],
  ['apPassword', 'AP Password', 'password'],
  ['mdnsHost', 'mDNS Host', 'text']
];
