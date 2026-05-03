#!/usr/bin/env python3
import json
import threading
import time
from copy import deepcopy
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent
APP_VERSION = "BilgeDry UI Mock v0.1"

lock = threading.RLock()

DEFAULT_CONFIG = {
    "systemEnabled": True,
    "demoMode": True,
    "runIntervalHours": 1,
    "primeDelayMs": 1200,
    "zoneMaxRunMs": 12000,
    "sampleIntervalMs": 250,
    "settleDelayMs": 350,
    "wetCurrentA": 1.8,
    "dryCurrentA": 0.35,
    "faultCurrentA": 7.5,
    "zones": [
        {"index": 0, "name": "Shower Sump", "enabled": True},
        {"index": 1, "name": "Engine Bilge", "enabled": True},
        {"index": 2, "name": "Main Bilge", "enabled": True},
        {"index": 3, "name": "Anchor Locker", "enabled": False},
    ],
}

DEFAULT_NETWORK = {
    "wifiSSID": "MarinaWiFi",
    "wifiPassword": "",
    "apSSID": "BilgeDry",
    "apPassword": "bilgedry123",
    "mdnsHost": "bilgedry",
    "mode": "station+ap",
    "ip": "192.168.4.1",
    "currentSSID": "MarinaWiFi",
}


def now():
    return int(time.time())


class Simulator:
    def __init__(self):
        self.reset()

    def reset(self):
        self.config = deepcopy(DEFAULT_CONFIG)
        self.network = deepcopy(DEFAULT_NETWORK)
        ts = now()
        self.logs = [
            {"epoch": ts - 120, "level": "INFO", "message": "Mock server started"},
            {"epoch": ts - 60, "level": "INFO", "message": "BilgeDry standing by"},
        ]
        self.mode = "idle"
        self.mode_started = time.time()
        self.current_zone = None
        self.status = {
            "appVersion": APP_VERSION,
            "enabled": True,
            "cycleActive": False,
            "cycleState": "Idle",
            "cycleEvent": "Waiting for next interval",
            "currentA": 0.0,
            "peakCurrentA": 0.0,
            "nextRun": ts + 1800,
            "lastRun": ts - 3600,
            "faultMessage": "",
            "zones": [self._zone_status(z) for z in self.config["zones"]],
        }

    def _zone_status(self, zone):
        return {
            "index": zone["index"],
            "name": zone["name"],
            "enabled": zone["enabled"],
            "faulted": False,
            "active": False,
            "timedOut": False,
            "detectedWater": False,
            "attempted": False,
            "durationMs": 0,
            "peakCurrentA": 0.0,
            "endCurrentA": 0.0,
            "lastSuccessEpoch": 0,
        }

    def add_log(self, level, message):
        self.logs.insert(0, {"epoch": now(), "level": level, "message": message})
        self.logs = self.logs[:100]

    def get_status(self):
        with lock:
            self._tick_locked()
            return deepcopy(self.status)

    def get_config(self):
        with lock:
            return deepcopy(self.config)

    def save_config(self, payload):
        with lock:
            for k in [
                "systemEnabled", "demoMode", "runIntervalHours", "primeDelayMs", "zoneMaxRunMs",
                "sampleIntervalMs", "settleDelayMs", "wetCurrentA", "dryCurrentA", "faultCurrentA"
            ]:
                if k in payload:
                    self.config[k] = payload[k]
            incoming_zones = payload.get("zones", [])
            by_index = {z["index"]: z for z in incoming_zones}
            for zone in self.config["zones"]:
                new = by_index.get(zone["index"])
                if new:
                    zone["name"] = new.get("name", zone["name"])
                    zone["enabled"] = bool(new.get("enabled", zone["enabled"]))
            self.status["enabled"] = bool(payload.get("systemEnabled", self.status["enabled"]))
            for i, zone in enumerate(self.config["zones"]):
                self.status["zones"][i]["name"] = zone["name"]
                self.status["zones"][i]["enabled"] = zone["enabled"]
            self.add_log("INFO", "Runtime config updated")
            return {"ok": True}

    def get_network(self):
        with lock:
            return deepcopy(self.network)

    def save_network(self, payload):
        with lock:
            for k in ["wifiSSID", "wifiPassword", "apSSID", "apPassword", "mdnsHost"]:
                if k in payload:
                    self.network[k] = payload[k]
            self.network["currentSSID"] = self.network["wifiSSID"] or self.network["apSSID"]
            self.add_log("INFO", "Network config updated")
            return {"ok": True, "rebootRequired": True}

    def get_logs(self):
        with lock:
            self._tick_locked()
            return deepcopy(self.logs)

    def clear_logs(self):
        with lock:
            self.logs = []
            self.add_log("INFO", "Logs cleared")
            return {"ok": True}

    def set_enabled(self, enabled):
        with lock:
            self.status["enabled"] = bool(enabled)
            self.config["systemEnabled"] = bool(enabled)
            self.add_log("INFO", f"System {'enabled' if enabled else 'disabled'}")
            if not enabled:
                self.mode = "idle"
                self.current_zone = None
                self.status["cycleActive"] = False
                self.status["cycleState"] = "Disabled"
                self.status["cycleEvent"] = "System turned off"
                self.status["currentA"] = 0.0
            else:
                self.status["cycleState"] = "Idle"
                self.status["cycleEvent"] = "Waiting for next interval"
            return {"ok": True}

    def run_now(self):
        with lock:
            if not self.status["enabled"]:
                self.add_log("WARN", "Run requested while system disabled")
                return {"ok": False, "message": "System disabled"}
            self.mode = "run"
            self.mode_started = time.time()
            self.current_zone = self._first_enabled_zone()
            self.status["cycleActive"] = True
            self.status["cycleState"] = "Priming"
            self.status["cycleEvent"] = f"Preparing zone {self.current_zone + 1}" if self.current_zone is not None else "No zones enabled"
            self.status["peakCurrentA"] = 0.0
            self._reset_zone_flags()
            self.add_log("INFO", "Manual run queued")
            return {"ok": True}

    def abort(self):
        with lock:
            self.mode = "idle"
            self.current_zone = None
            self.status["cycleActive"] = False
            self.status["cycleState"] = "Aborted"
            self.status["cycleEvent"] = "Cycle stopped by user"
            self.status["currentA"] = 0.0
            for z in self.status["zones"]:
                z["active"] = False
            self.add_log("WARN", "Cycle aborted by user")
            return {"ok": True}

    def reboot(self):
        with lock:
            self.add_log("INFO", "Mock reboot requested")
            self.status["cycleEvent"] = "Reboot requested"
            return {"ok": True}

    def sim_wet(self):
        with lock:
            self.mode = "wet"
            self.mode_started = time.time()
            self.current_zone = 2 if len(self.status["zones"]) > 2 else self._first_enabled_zone()
            self._reset_zone_flags()
            self.add_log("INFO", "Simulated wet bilge event")
            return {"ok": True}

    def sim_fault(self):
        with lock:
            self.mode = "fault"
            self.mode_started = time.time()
            self.current_zone = 1 if len(self.status["zones"]) > 1 else self._first_enabled_zone()
            self._reset_zone_flags()
            self.add_log("ERROR", "Simulated pump over-current fault")
            return {"ok": True}

    def sim_idle(self):
        with lock:
            self.mode = "idle"
            self.mode_started = time.time()
            self.current_zone = None
            self.status["faultMessage"] = ""
            self.status["cycleActive"] = False
            self.status["cycleState"] = "Idle"
            self.status["cycleEvent"] = "Waiting for next interval"
            self.status["currentA"] = 0.0
            for z in self.status["zones"]:
                z["active"] = False
                z["faulted"] = False
                z["timedOut"] = False
            self.add_log("INFO", "Simulator returned to idle")
            return {"ok": True}

    def _first_enabled_zone(self):
        for zone in self.config["zones"]:
            if zone["enabled"]:
                return zone["index"]
        return None

    def _reset_zone_flags(self):
        for z in self.status["zones"]:
            z["faulted"] = False
            z["active"] = False
            z["timedOut"] = False
            z["detectedWater"] = False
            z["attempted"] = False
            z["durationMs"] = 0
            z["peakCurrentA"] = 0.0
            z["endCurrentA"] = 0.0
        self.status["faultMessage"] = ""
        self.status["peakCurrentA"] = 0.0

    def _tick_locked(self):
        ts = now()
        self.status["appVersion"] = APP_VERSION
        self.status["nextRun"] = ts + int(self.config["runIntervalHours"] * 3600)
        elapsed = time.time() - self.mode_started
        zones = self.status["zones"]
        current = 0.0

        if not self.status["enabled"]:
            self.status["cycleActive"] = False
            self.status["currentA"] = 0.0
            return

        if self.mode == "idle":
            self.status["cycleActive"] = False
            self.status["cycleState"] = "Idle"
            self.status["cycleEvent"] = "Waiting for next interval"
            current = 0.08
        elif self.mode == "run":
            self.status["cycleActive"] = True
            if self.current_zone is None:
                self.mode = "idle"
                self.status["cycleState"] = "Idle"
                self.status["cycleEvent"] = "No zones enabled"
                current = 0.08
            else:
                zone = zones[self.current_zone]
                if elapsed < 1.2:
                    self.status["cycleState"] = "Priming"
                    self.status["cycleEvent"] = f"Priming {zone['name']}"
                    current = 0.65
                elif elapsed < 4.5:
                    self.status["cycleState"] = "Sampling"
                    self.status["cycleEvent"] = f"Checking {zone['name']}"
                    zone["active"] = True
                    zone["attempted"] = True
                    zone["durationMs"] = int((elapsed - 1.2) * 1000)
                    current = max(self.config["dryCurrentA"] + 0.12, 0.42)
                    zone["peakCurrentA"] = max(zone["peakCurrentA"], current)
                    zone["endCurrentA"] = current
                else:
                    zone["active"] = False
                    zone["attempted"] = True
                    zone["endCurrentA"] = self.config["dryCurrentA"]
                    self.status["lastRun"] = ts
                    self.mode = "idle"
                    self.mode_started = time.time()
                    self.status["cycleActive"] = False
                    self.status["cycleState"] = "Complete"
                    self.status["cycleEvent"] = f"{zone['name']} dry"
                    self.add_log("INFO", f"{zone['name']} sampled dry")
                    current = 0.09
        elif self.mode == "wet":
            zone = zones[self.current_zone]
            self.status["cycleActive"] = True
            zone["attempted"] = True
            zone["active"] = elapsed < 6.0
            zone["detectedWater"] = True
            zone["durationMs"] = int(min(elapsed, 6.0) * 1000)
            current = 2.3 + (0.25 if int(elapsed * 2) % 2 else 0.0)
            zone["peakCurrentA"] = max(zone["peakCurrentA"], current)
            zone["endCurrentA"] = current
            zone["lastSuccessEpoch"] = ts
            self.status["cycleState"] = "Pumping"
            self.status["cycleEvent"] = f"Water detected in {zone['name']}"
            if elapsed >= 6.0:
                zone["active"] = False
                self.status["lastRun"] = ts
                self.mode = "idle"
                self.mode_started = time.time()
                self.add_log("INFO", f"{zone['name']} pumped successfully")
        elif self.mode == "fault":
            zone = zones[self.current_zone]
            self.status["cycleActive"] = True
            zone["attempted"] = True
            zone["active"] = elapsed < 2.0
            zone["faulted"] = True
            zone["durationMs"] = int(min(elapsed, 2.0) * 1000)
            current = self.config["faultCurrentA"] + 1.2
            zone["peakCurrentA"] = max(zone["peakCurrentA"], current)
            zone["endCurrentA"] = current
            self.status["cycleState"] = "Fault"
            self.status["cycleEvent"] = f"Over-current on {zone['name']}"
            self.status["faultMessage"] = f"Pump current exceeded limit on {zone['name']}"
            if elapsed >= 2.0:
                zone["active"] = False
                self.status["cycleActive"] = False
                self.mode = "idle"
                self.mode_started = time.time()
        
        self.status["currentA"] = round(current, 2)
        self.status["peakCurrentA"] = max(float(self.status.get("peakCurrentA", 0.0)), round(current, 2))


SIM = Simulator()


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def _send_json(self, payload, code=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/v1/status":
            return self._send_json(SIM.get_status())
        if path == "/api/v1/config":
            return self._send_json(SIM.get_config())
        if path == "/api/v1/network":
            return self._send_json(SIM.get_network())
        if path == "/api/v1/logs":
            return self._send_json(SIM.get_logs())
        return super().do_GET()

    def do_POST(self):
        path = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            payload = json.loads(raw.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            payload = {}

        routes = {
            "/api/v1/config": lambda: SIM.save_config(payload),
            "/api/v1/network": lambda: SIM.save_network(payload),
            "/api/v1/control/enable": lambda: SIM.set_enabled(payload.get("enabled", False)),
            "/api/v1/control/run": SIM.run_now,
            "/api/v1/control/abort": SIM.abort,
            "/api/v1/reboot": SIM.reboot,
            "/api/v1/logs/clear": SIM.clear_logs,
            "/api/v1/sim/wet": SIM.sim_wet,
            "/api/v1/sim/fault": SIM.sim_fault,
            "/api/v1/sim/idle": SIM.sim_idle,
        }
        handler = routes.get(path)
        if handler:
            return self._send_json(handler())
        self._send_json({"ok": False, "message": "Not found"}, code=404)


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", 8000), Handler)
    print("BilgeDry UI mock running at http://127.0.0.1:8000/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
