# BilgeDry Production Refactor

This version is a real step closer to the alternator project style rather than a simple cleanup.

## What changed

- Reworked the pump logic into a non-blocking cycle controller with explicit states:
  - `Idle`
  - `Prime`
  - `Running`
  - `Settling`
  - `Complete`
  - `Fault`
- Split responsibilities into focused modules:
  - `Storage` for persistent config
  - `LogManager` for structured event logging
  - `SensorManager` for INA260 handling and unit normalization
  - `NetworkManager` for STA/AP/mDNS/time sync
  - `CycleController` for zone sequencing and decision logic
  - `WebServerManager` for a cleaner REST API
- Replaced the old flat UI field sprawl with a cleaner JSON contract built around arrays and grouped config.
- Added cycle fault tracking, abort handling, queueable manual runs, and explicit per-zone results.
- Added version/build metadata to the API.
- Moved web assets under `/assets` and structured the frontend into API/state/render/main files.

## Why this is better

The original project worked like a single long script. That is fine until you want to add retries, diagnostics, better status, richer logs, NMEA integration, or a more capable UI. Then it becomes a mess fast.

This version is shaped so those next steps are practical.

## Important behavior notes

- INA260 current is treated as **amps**, converted from the library's **mA** reading.
- A zone is considered to have seen water once current reaches `wetCurrentA` during the run.
- A zone is considered dry once current drops below `dryCurrentA`.
- A fault is raised if current reaches `faultCurrentA`.
- The controller no longer blocks the whole app with multi-second `delay()` calls during a cycle.

## Current API surface

- `GET /api/v1/status`
- `GET /api/v1/config`
- `POST /api/v1/config`
- `GET /api/v1/network`
- `POST /api/v1/network`
- `POST /api/v1/control/run`
- `POST /api/v1/control/abort`
- `POST /api/v1/control/enable`
- `GET /api/v1/logs`
- `POST /api/v1/logs/clear`
- `POST /api/v1/reboot`

## Project structure

```text
BilgeDry_production/
├── README.md
└── esp32/main/
    ├── main.ino
    ├── BilgeDryApp.cpp/.h
    ├── AppTypes.h
    ├── AppVersion.h
    ├── Defaults.h
    ├── Pins.h
    ├── Storage.cpp/.h
    ├── LogManager.cpp/.h
    ├── SensorManager.cpp/.h
    ├── NetworkManager.cpp/.h
    ├── CycleController.cpp/.h
    ├── WebServerManager.cpp/.h
    └── data/
        ├── index.html
        └── assets/
            ├── styles.css
            ├── api.js
            ├── state.js
            ├── render.js
            └── main.js
```

## What still needs real hardware validation

This is the honest part:

- It is structurally much better, but I did not compile and flash it on your exact board here.
- Threshold values will still need tuning on your actual pump and plumbing.
- The current-based wet/dry model is only as good as the pump signature in the real boat.
- If the solenoid timing needs per-zone customization, that is not added yet.
- There is no EEPROM wear strategy beyond normal SPIFFS file saves.

## Best next improvements

1. Add per-zone calibration values instead of one global wet/dry threshold.
2. Add startup self-test and sensor health reporting to status.
3. Add retry logic for suspicious short runs.
4. Add a lightweight event history / metrics page.
5. Add optional NMEA 2000 publish-only status later if you want it to match the rest of the boat systems.

## Recommendation

This is in a good place to become the one you build and share first. It is much easier to reason about, safer to extend, and closer to how the alternator project should feel across the whole codebase.
