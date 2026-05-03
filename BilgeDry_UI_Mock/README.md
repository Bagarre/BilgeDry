# BilgeDry UI Mock Server

This is a standalone mock of the BilgeDry web UI. It serves the real frontend files from the refactor and backs them with a fake API so you can click through the app without ESP32 hardware.

## What it does
- serves the BilgeDry HTML/CSS/JS locally
- emulates `/api/v1/*` endpoints with in-memory state
- lets you save runtime and network config from the UI
- simulates manual runs, wet zones, dry zones, faults, aborts, reboots, and logs
- updates status over time so the dashboard feels alive

## Run it

```bash
cd BilgeDry_UI_Mock
python3 mock_bilgedry_server.py
```

Then open:

```text
http://127.0.0.1:8000/
```

## Notes
- Nothing is persisted to disk. Restarting the server resets it.
- The simulator buttons are only for this mock package. They are not part of the real embedded UI.
- This is for UI walkthrough and structure review, not sensor validation.
