#!/usr/bin/env python3
import json
import random
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

HOST = "127.0.0.1"
PORT = 8000
WEB_ROOT = Path(__file__).resolve().parent

state_lock = threading.Lock()

now = int(time.time())

STATE = {
    "appVersion": "BilgeDry v1.1-mock",
    "buildDate": "2026-04-23 12:00",
    "enabled": True,
    "cycleActive": False,
    "cycleState": "IDLE",
    "cycleEvent": "Waiting for next interval",
    "faultMessage": "",
    "lastRun": now - 900,
    "nextRun": now + 180,
    "currentA": 0.04,
    "peakCurrentA": 0.18,
    "busVoltage": 13.21,
    "totalPowerW": 0.53,
    "wifi": {
        "currentSSID": "MyBoatWiFi",
        "apSSID": "BilgeDry",
        "dhcp": True,
        "ip": "192.168.1.150",
        "gateway": "192.168.1.1",
        "subnet": "255.255.255.0",
    },
    "config": {
        "autoEnabled": True,
        "cycleIntervalSec": 60,
        "pumpPrimeMs": 600,
        "zoneRunTimeoutMs": 8000,
        "wetCurrentA": 1.10,
        "dryCurrentA": 0.35,
        "faultCurrentA": 6.50,
    },
    "zones": [
        {
            "index": 0,
            "name": "Zone 1",
            "enabled": True,
            "active": False,
            "faulted": False,
            "detectedWater": False,
            "attempted": True,
            "timedOut": False,
            "durationMs": 0,
            "peakCurrentA": 0.11,
            "endCurrentA": 0.04,
            "lastSuccessEpoch": now - 900,
        },
        {
            "index": 1,
            "name": "Zone 2",
            "enabled": True,
            "active": False,
            "faulted": False,
            "detectedWater": False,
            "attempted": True,
            "timedOut": False,
            "durationMs": 0,
            "peakCurrentA": 0.09,
            "endCurrentA": 0.03,
            "lastSuccessEpoch": now - 900,
        },
        {
            "index": 2,
            "name": "Zone 3",
            "enabled": True,
            "active": False,
            "faulted": False,
            "detectedWater": False,
            "attempted": True,
            "timedOut": False,
            "durationMs": 0,
            "peakCurrentA": 0.10,
            "endCurrentA": 0.04,
            "lastSuccessEpoch": now - 900,
        },
    ],
    "logs": [],
}

def add_log(level: str, message: str) -> None:
    STATE["logs"].insert(0, {
        "epoch": int(time.time()),
        "level": level,
        "message": message,
    })
    del STATE["logs"][80:]


def reset_zone_runtime(zone: dict) -> None:
    zone["active"] = False
    zone["durationMs"] = 0
    zone["endCurrentA"] = round(random.uniform(0.02, 0.08), 2)


def update_power(idle=False):
    if idle:
        STATE["currentA"] = round(random.uniform(0.02, 0.08), 2)
        STATE["peakCurrentA"] = max(STATE["peakCurrentA"], STATE["currentA"])
        STATE["totalPowerW"] = round(STATE["currentA"] * STATE["busVoltage"], 2)
    else:
        STATE["totalPowerW"] = round(STATE["currentA"] * STATE["busVoltage"], 2)


def run_cycle(manual=True):
    with state_lock:
        if STATE["cycleActive"]:
            add_log("WARN", "Run requested while cycle already active")
            return
        STATE["cycleActive"] = True
        STATE["cycleState"] = "RUNNING"
        STATE["cycleEvent"] = "Starting manual cycle" if manual else "Starting auto cycle"
        STATE["faultMessage"] = ""
        STATE["lastRun"] = int(time.time())
        add_log("INFO", STATE["cycleEvent"])

        for z in STATE["zones"]:
            z["active"] = False
            z["faulted"] = False
            z["timedOut"] = False
            z["attempted"] = False
            z["detectedWater"] = False
            z["durationMs"] = 0

    for zone in STATE["zones"]:
        with state_lock:
            if not STATE["cycleActive"]:
                add_log("WARN", "Cycle aborted")
                break
            if not zone["enabled"]:
                continue

            zone["attempted"] = True
            zone["active"] = True
            STATE["cycleEvent"] = f"{zone['name']}: Pump started"
            zone_wet = random.random() < 0.35
            zone_fault = random.random() < 0.08

            if zone_fault:
                simulated_current = round(random.uniform(6.8, 9.2), 2)
            elif zone_wet:
                simulated_current = round(random.uniform(1.3, 2.8), 2)
            else:
                simulated_current = round(random.uniform(0.06, 0.22), 2)

            STATE["currentA"] = simulated_current
            STATE["peakCurrentA"] = max(STATE["peakCurrentA"], simulated_current)
            zone["peakCurrentA"] = simulated_current
            update_power(idle=False)
            add_log("INFO", f"{zone['name']}: Pump started")

        run_seconds = random.uniform(1.2, 4.2)
        time.sleep(run_seconds)

        with state_lock:
            zone["durationMs"] = int(run_seconds * 1000)
            zone["active"] = False

            if zone_fault:
                zone["faulted"] = True
                STATE["faultMessage"] = f"{zone['name']} overcurrent fault"
                STATE["cycleState"] = "FAULT"
                STATE["cycleEvent"] = STATE["faultMessage"]
                add_log("ERROR", STATE["faultMessage"])
            elif zone_wet:
                zone["detectedWater"] = True
                zone["lastSuccessEpoch"] = int(time.time())
                zone["endCurrentA"] = round(random.uniform(0.18, 0.40), 2)
                add_log("INFO", f"{zone['name']}: Water detected and cleared")
            else:
                zone["endCurrentA"] = round(random.uniform(0.03, 0.08), 2)
                add_log("INFO", f"{zone['name']}: Dry ({zone['endCurrentA']:.2f} A, {run_seconds:.1f}s)")

            STATE["currentA"] = round(random.uniform(0.03, 0.08), 2)
            update_power(idle=False)

            if STATE["faultMessage"]:
                break

    with state_lock:
        if STATE["faultMessage"]:
            STATE["cycleActive"] = False
            STATE["nextRun"] = int(time.time()) + STATE["config"]["cycleIntervalSec"]
            return

        STATE["cycleActive"] = False
        STATE["cycleState"] = "IDLE"
        wet_count = sum(1 for z in STATE["zones"] if z["detectedWater"])
        STATE["cycleEvent"] = "All zones dry" if wet_count == 0 else f"Cycle complete, {wet_count} wet zone(s) handled"
        STATE["nextRun"] = int(time.time()) + STATE["config"]["cycleIntervalSec"]
        STATE["currentA"] = round(random.uniform(0.03, 0.08), 2)
        update_power(idle=False)
        add_log("INFO", STATE["cycleEvent"])


def start_cycle(manual=True):
    t = threading.Thread(target=run_cycle, kwargs={"manual": manual}, daemon=True)
    t.start()


def background_simulator():
    while True:
        time.sleep(2)
        with state_lock:
            # Small drift on power/network values.
            STATE["busVoltage"] = round(max(12.6, min(13.8, STATE["busVoltage"] + random.uniform(-0.03, 0.03))), 2)
            if not STATE["cycleActive"]:
                STATE["currentA"] = round(max(0.02, min(0.10, STATE["currentA"] + random.uniform(-0.02, 0.02))), 2)
                update_power(idle=True)

            now_ts = int(time.time())
            if STATE["enabled"] and not STATE["cycleActive"] and not STATE["faultMessage"] and now_ts >= STATE["nextRun"]:
                start_cycle(manual=False)


class Handler(BaseHTTPRequestHandler):
    server_version = "BilgeDryMock/1.1"

    def log_message(self, fmt, *args):
        return

    def _json(self, payload, status=200):
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _text(self, payload, status=200, content_type="text/plain; charset=utf-8"):
        raw = payload.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _read_json_body(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        data = self.rfile.read(length)
        if not data:
            return {}
        try:
            return json.loads(data.decode("utf-8"))
        except Exception:
            return {}

    def _serve_static(self, relpath: str):
        target = (WEB_ROOT / relpath.lstrip("/")).resolve()
        if WEB_ROOT not in target.parents and target != WEB_ROOT:
            self._text("Forbidden", status=403)
            return
        if not target.exists() or not target.is_file():
            self._text("Not found", status=404)
            return

        suffix = target.suffix.lower()
        content_type = {
            ".html": "text/html; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".js": "application/javascript; charset=utf-8",
            ".json": "application/json; charset=utf-8",
            ".png": "image/png",
            ".jpg": "image/jpeg",
            ".jpeg": "image/jpeg",
            ".svg": "image/svg+xml",
            ".ico": "image/x-icon",
        }.get(suffix, "application/octet-stream")

        raw = target.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/":
            self._serve_static("/index.html")
            return

        if path == "/api/v1/status":
            with state_lock:
                payload = {
                    "appVersion": STATE["appVersion"],
                    "buildDate": STATE["buildDate"],
                    "enabled": STATE["enabled"],
                    "cycleActive": STATE["cycleActive"],
                    "cycleState": STATE["cycleState"],
                    "cycleEvent": STATE["cycleEvent"],
                    "faultMessage": STATE["faultMessage"],
                    "lastRun": STATE["lastRun"],
                    "nextRun": STATE["nextRun"],
                    "currentA": STATE["currentA"],
                    "peakCurrentA": STATE["peakCurrentA"],
                    "busVoltage": STATE["busVoltage"],
                    "totalPowerW": STATE["totalPowerW"],
                    "zones": STATE["zones"],
                }
            self._json(payload)
            return

        if path == "/api/v1/logs":
            with state_lock:
                self._json(STATE["logs"])
            return

        if path == "/api/v1/network":
            with state_lock:
                self._json(STATE["wifi"])
            return

        if path == "/api/v1/config":
            with state_lock:
                self._json(STATE["config"])
            return

        # Backward-compat helpers
        if path == "/api/v1/run":
            start_cycle(manual=True)
            self._json({"ok": True, "started": True})
            return

        if path == "/api/v1/run_all":
            start_cycle(manual=True)
            self._json({"ok": True, "started": True})
            return

        self._serve_static(path)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        body = self._read_json_body()

        if path == "/api/v1/control/run":
            start_cycle(manual=True)
            self._json({"ok": True, "started": True})
            return

        if path == "/api/v1/control/abort":
            with state_lock:
                if STATE["cycleActive"]:
                    STATE["cycleActive"] = False
                    STATE["cycleState"] = "ABORTED"
                    STATE["cycleEvent"] = "Cycle aborted by user"
                    STATE["nextRun"] = int(time.time()) + STATE["config"]["cycleIntervalSec"]
                    add_log("WARN", "Cycle aborted by user")
            self._json({"ok": True})
            return

        if path == "/api/v1/reboot":
            with state_lock:
                STATE["cycleActive"] = False
                STATE["cycleState"] = "IDLE"
                STATE["cycleEvent"] = "System rebooted"
                STATE["faultMessage"] = ""
                STATE["nextRun"] = int(time.time()) + STATE["config"]["cycleIntervalSec"]
                update_power(idle=True)
                add_log("INFO", "System rebooted")
            self._json({"ok": True})
            return

        if path == "/api/v1/network":
            with state_lock:
                STATE["wifi"].update({
                    "currentSSID": body.get("currentSSID", STATE["wifi"]["currentSSID"]),
                    "apSSID": body.get("apSSID", STATE["wifi"]["apSSID"]),
                    "dhcp": bool(body.get("dhcp", STATE["wifi"]["dhcp"])),
                    "ip": body.get("ip", STATE["wifi"]["ip"]),
                    "gateway": body.get("gateway", STATE["wifi"]["gateway"]),
                    "subnet": body.get("subnet", STATE["wifi"]["subnet"]),
                })
                add_log("INFO", f"Network updated: {STATE['wifi']['currentSSID']}")
                self._json({"ok": True, "network": STATE["wifi"]})
            return

        if path == "/api/v1/config":
            with state_lock:
                for key in list(STATE["config"].keys()):
                    if key in body:
                        STATE["config"][key] = body[key]
                STATE["enabled"] = bool(body.get("autoEnabled", STATE["config"]["autoEnabled"]))
                add_log("INFO", "Config updated")
                self._json({"ok": True, "config": STATE["config"]})
            return

        self._json({"ok": False, "error": "Not found"}, status=404)


def ensure_default_files():
    index_path = WEB_ROOT / "index.html"
    if not index_path.exists():
        index_path.write_text(
            "<!doctype html><html><body><h1>BilgeDry mock server</h1><p>Add your index.html here.</p></body></html>",
            encoding="utf-8",
        )

    add_log("INFO", "System initialized")
    add_log("INFO", "Mock server ready")


def main():
    ensure_default_files()
    sim = threading.Thread(target=background_simulator, daemon=True)
    sim.start()

    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"BilgeDry mock server running on http://{HOST}:{PORT}/")
    print(f"Serving files from: {WEB_ROOT}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
