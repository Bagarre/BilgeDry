function showBanner(message, type = "info") {
  const banner = document.createElement("div");
  banner.textContent = message;
  banner.style.position = "fixed";
  banner.style.top = "1rem";
  banner.style.left = "50%";
  banner.style.transform = "translateX(-50%)";
  banner.style.fontSize = "1rem";
  banner.style.textAlign = "center";
  banner.style.maxWidth = "90%";
  banner.style.padding = "0.5rem 1rem";
  banner.style.borderRadius = "0.5rem";
  banner.style.boxShadow = "0 2px 4px rgba(0, 0, 0, 0.2)";
  banner.style.color = "white";
  banner.style.zIndex = 9999;
  banner.style.transition = "opacity 0.3s ease";
  banner.style.opacity = 1;
  banner.style.backgroundColor = type === "error" ? "#dc2626" : "#16a34a";

  document.body.appendChild(banner);

  setTimeout(() => {
    banner.style.opacity = 0;
    setTimeout(() => banner.remove(), 1000);
  }, 3000);
};

function updateHeaderState(enabled) {
  const header = document.querySelector("header");
  if (header) {
    header.style.backgroundColor = enabled ? "#0074D9" : "#4b5563"; // blue or gray
    document.getElementById("enableStatus").textContent = enabled ? "Status: Enabled" : "Status: Disabled";
  }
}


function fetchStatus() {
  fetch("/api/v1/status")
  .then(res => res.json())
  .then(data => {
    document.getElementById("voltage").textContent = data.voltage || "—";


        document.getElementById("lastRun").textContent =
          data.lastRun
            ? new Date(data.lastRun * 1000).toLocaleString()
            : "—";
        document.getElementById("nextRun").textContent =
          data.nextRun
            ? new Date(data.nextRun * 1000).toLocaleString()
            : "—";


    document.getElementById("port1NameDisplay").textContent = data.port1Name || "—";
    document.getElementById("port2NameDisplay").textContent = data.port2Name || "—";
    document.getElementById("port3NameDisplay").textContent = data.port3Name || "—";
    document.getElementById("port4NameDisplay").textContent = data.port4Name || "—";
    document.getElementById("port1Run").textContent = data.port1Run || "—";
    document.getElementById("port2Run").textContent = data.port2Run || "—";
    document.getElementById("port3Run").textContent = data.port3Run || "—";
    document.getElementById("port4Run").textContent = data.port4Run || "—";

  })
  .catch(err => {
    console.error("Failed to fetch status:", err);
    showBanner("Failed to fetch status", "error");
  });
}



function fetchConfigs() {

  fetch("/api/v1/config")
    .then(res => res.json())
    .then(cfg => {
      document.getElementById("runFrequency").value = cfg.runFrequency || "";
      document.getElementById("runDelay").value = cfg.runDelay || "";
      document.getElementById("runCurrent").value = cfg.runCurrent || "";
      document.getElementById("runMax").value = cfg.runMax || "";
      document.getElementById("port1NameConfig").value = cfg.port1Name || "";
      document.getElementById("port2NameConfig").value = cfg.port2Name || "";
      document.getElementById("port3NameConfig").value = cfg.port3Name || "";
      document.getElementById("port4NameConfig").value = cfg.port4Name || "";
      document.getElementById("port1EnableConfig").checked = cfg.port1Enable || "";
      document.getElementById("port2EnableConfig").checked = cfg.port2Enable || "";
      document.getElementById("port3EnableConfig").checked = cfg.port3Enable || "";
      document.getElementById("port4EnableConfig").checked = cfg.port4Enable || "";
    })
    .catch(err => {
      console.error("Failed to load config:", err);
      showBanner("Failed to load configuration", "error");
    });

}

function fetchLogs() {
  fetch('/api/v1/logs')
    .then(res => res.json())
    .then(logs => {
      const out = document.getElementById('logOutput');
      // clear old entries
      out.innerHTML = '';

      logs.forEach(entry => {
        // split timestamp (seconds) from message
        const [sec, ...msgParts] = entry.split(': ');
        const date = new Date(Number(sec) * 1000);
        const time = date.toLocaleString();  // or .toLocaleTimeString()
        const msg  = msgParts.join(': ');

        const li = document.createElement('li');
        li.className = 'log-entry';
        li.innerHTML = `<span class="log-time">${time}</span> — <span class="log-msg">${msg}</span>`;
        out.appendChild(li);
      });
    })
    .catch(err => {
      console.error('Failed to load logs', err);
      document.getElementById('logOutput').textContent = 'Error loading logs';
    });
}




function fetchNetwork() {
  fetch("/api/v1/network")
    .then(res => res.json())
    .then(cfg => {
      document.getElementById("ssid").value = cfg.ssid;
      })
    .catch(err => {
      console.error("Failed to load network Data:", err);
      showBanner("Failed to load Network Data", "error");
    });
}





window.onload = () => {
  setInterval(fetchStatus, 5000);
  setInterval(fetchLogs, 5000);

  fetchStatus();
  fetchConfigs();
  fetchLogs();
  fetchNetwork();



  fetch("/api/v1/enable")
    .then(res => res.json())
    .then(data => {
      const enabled = data.enabled === true;
      document.getElementById("enableToggle").checked = enabled;
      updateHeaderState(enabled);
    })
    .catch(err => {
      console.error("Failed to fetch enable state:", err);
      showBanner("Failed to fetch enable state", "error");
    });

  document.getElementById("enableToggle").addEventListener("change", (e) => {
    const newState = e.target.checked;
    fetch("/api/v1/enable", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: newState })
    })
      .then(() => {
        updateHeaderState(newState);
        showBanner(newState ? "✅ Enabled" : "⚠️ Disabled", newState ? "success" : "error");
      })
      .catch(err => {
        console.error("Enable toggle failed", err);
        showBanner("Failed to change enable state", "error");
      });
  });



  document.getElementById("saveConfig").addEventListener("click", () => {
    const config = {
      runFrequency: document.getElementById("runFrequency").value,
      runDelay: document.getElementById("runDelay").value,
      runCurrent: document.getElementById("runCurrent").value,
      runMax: document.getElementById("runMax").value,
      port1Name: document.getElementById("port1NameConfig").value,
      port2Name: document.getElementById("port2NameConfig").value,
      port3Name: document.getElementById("port3NameConfig").value,
      port4Name: document.getElementById("port4NameConfig").value,
      port1Enable: document.getElementById("port1EnableConfig").checked,
      port2Enable: document.getElementById("port2EnableConfig").checked,
      port3Enable: document.getElementById("port3EnableConfig").checked,
      port4Enable: document.getElementById("port4EnableConfig").checked

    };
console.log( JSON.stringify(config) );
    fetch("/api/v1/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(config)
    })
      .then(res => res.text())
      .then(text => {
        console.log("Config saved:", text);
        showBanner("✅ Config saved successfully", "success");
      })
      .catch(err => {
        console.error("Save failed:", err);
        showBanner("❌ Failed to save config", "error");
      });
  });



    document.getElementById("runNowBtn").addEventListener("click", () => {
      const runNow = {when: 0 };

      fetch("/api/v1/status/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(runNow)
      })
        .then(() => {
          showBanner("✅ Running Now...", "success");
        })
        .catch(err => {
          console.error("Run Now failed:", err);
          showBanner("❌ Failed to Run Now", "error");
        });
    });



    document.getElementById("rebootBtn").addEventListener("click", () => {
      const reboot = {when: 0 };

      fetch("/api/v1/reboot", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(reboot)
      })
        .then(() => {
          showBanner("✅ Rebooting...", "success");
        })
        .catch(err => {
          console.error("Reboot  failed:", err);
          showBanner("❌ Failed to Reboot", "error");
        });
    });



  document.getElementById("submitNetwork").addEventListener("click", () => {
    const network = {
      wifiSSID: document.getElementById("ssid").value,
      wifiPassword: document.getElementById("password").value
    };

    fetch("/api/v1/network", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(network)
    })
      .then(() => {
        showBanner("✅ Network settings saved — rebooting...", "success");
      })
      .catch(err => {
        console.error("Network save failed:", err);
        showBanner("❌ Failed to save network settings", "error");
      });
  });
};
