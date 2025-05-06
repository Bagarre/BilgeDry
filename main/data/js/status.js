
fetch('/api/v1/status')
  .then(res => res.json())
  .then(data => {
    const container = document.querySelector('.container');
    const statusPre = document.getElementById('status');
    if (statusPre) statusPre.remove();

    data.zones.forEach(z => {
      const zoneDiv = document.createElement('div');
      zoneDiv.className = 'card';
      zoneDiv.innerHTML = `
        <h2>${z.name || `Zone ${z.id}`}</h2>
        <p>Status: <strong class="${z.enabled ? 'enabled' : 'disabled'}">${z.status}</strong></p>
        <p>Pin: ${z.pin}</p>
        <p>Last Run: ${new Date(z.lastRun).toLocaleString()}</p>
        <label class="switch">
          <input type="checkbox" disabled ${z.enabled ? 'checked' : ''}>
          <span class="slider"></span>
        </label>
      `;
      container.appendChild(zoneDiv);
    });

    const pumpDiv = document.createElement('div');
    pumpDiv.className = 'card';
    pumpDiv.innerHTML = `
      <h2>Pump</h2>
      <p>Amps: ${data.pumpAmps}</p>
    `;
    container.appendChild(pumpDiv);
  })
  .catch(err => {
    const statusPre = document.getElementById('status');
    if (statusPre) statusPre.textContent = 'Failed to load status';
  });
