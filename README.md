
# StormShield

StormShield is a wearable safety system built on **ESP32** with an mmWave motion sensor and Bluetooth Low Energy (BLE). The ESP32 computes a risk score on-device and streams telemetry to a browser-based web app for display and logging.

## Features
- ESP32 firmware: UART sensor ingest, risk-score computation, logistic-regression boundary, BLE GATT (WRITE/READ/NOTIFY)
- Web app (HTML/JS + Chart.js): live dashboard, logs, and threshold presets
- Threshold control from web → device using `S=<int>`
- Lightweight client: serve over https or `http://localhost` for Web Bluetooth

## Repository Structure
```
stormshield/
├─ stormshield.ino      # ESP32 firmware (UART + LR scoring + BLE notify)
├─ index.html           # Web app UI, BLE client, charts
├─ style.css            # Optional styles (if used)
├─ favicon.ico          # Site icon (optional)
├─ README.md            # This document
└─ LICENSE.txt          # GPL-3.0
```

## Requirements
### Hardware
- ESP32 development board
- mmWave radar sensor module
- USB or battery power

### Software
- Arduino IDE with ESP32 board support
- Browser with Web Bluetooth (Chrome/Edge). Use https or `http://localhost`.

## Setup
### 1) Flash the firmware
1. Open `stormshield.ino` in Arduino IDE.
2. Select your ESP32 board and correct port.
3. Compile and upload.

### 2) Serve the web app
Web Bluetooth does not work from `file://`. Serve the folder via HTTP(S):
```bash
python3 -m http.server 8000
# open http://localhost:8000/index.html
```

## BLE Interface
- **Service UUID:** `cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad`
- **Characteristic UUID:** `79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493` (WRITE/READ/NOTIFY)

**Device → Web (notifications):**
```
d={meters},v={mps},score={0..100},p={0..1}
# example: d=0.87,v=1.25,score=53,p=0.61
```

**Web → Device (writes):**
- `S=<int>` sets the logistic boundary so that `p=0.5` at `score=S`.
- `buzz` triggers a short actuator test.

## Risk Scoring (on firmware)
```
# inverse-distance + speed bump + EMA smoothing -> Score in [0..100]
p = 1 / (1 + exp(-(w0 + w1 * Score)))
w0 = -w1 * S          # ensures p=0.5 at score=S
trigger when p ≥ 0.5  # equivalent to score ≥ S
```

## Troubleshooting
- Serve over https or `localhost` and use a Web Bluetooth–capable browser.
- Ensure UUIDs are identical in firmware and web app.
- Characteristic must enable **NOTIFY** and the browser must call `startNotifications()`.
- If no data appears, confirm the firmware is notifying lines like `d=...,v=...,score=...,p=...`.

## License
This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See `LICENSE.txt` for details.
