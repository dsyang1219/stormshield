# StormShield

StormShield is a wearable safety system built on **ESP32** with an mmWave motion sensor and Bluetooth Low Energy (BLE). It pairs with a browser-based web app to stream sensor data, compute a **risk score on the firmware**, and display alerts in real time.

## What’s New
- **Score is computed on the ESP32** (logistic regression). The web app now **displays** the firmware-provided `score` (and optional `p`) instead of recomputing it.
- **BLE UUIDs are unified** between firmware and web.
- The firmware characteristic now supports **NOTIFY** so the browser can receive live telemetry.

## Features
- ESP32 firmware: mmWave sensing, logistic-regression scoring, BLE GATT (WRITE/READ/NOTIFY)
- Web app (HTML/CSS/JS + Chart.js): live dashboard, logs, threshold/preset controls
- Threshold writes from web → device using `S=<int>` boundary
- Lightweight client: serve over https or `localhost` and connect via Web Bluetooth

## Repository Structure
```
stormshield/
├─ stormshield.ino      # ESP32 firmware (LR scoring + BLE GATT)
├─ index.html           # Web app UI, BLE client, charts
├─ style.css            # Web app styles
├─ favicon.ico          # Site icon
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
- A browser with Web Bluetooth (Chrome/Edge). Use https or `http://localhost`.

## Setup

### 1) Flash the firmware
1. Open `stormshield.ino` in Arduino IDE.
2. Select your ESP32 board and correct port.
3. Compile and upload.

### 2) Serve the web app
Web Bluetooth generally does not work from `file://`. Serve the folder over HTTP(S):
```bash
python3 -m http.server 8000
# open http://localhost:8000/index.html
```

## BLE Interface

### UUIDs (firmware + web)
- **Service:** `cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad`
- **Characteristic:** `79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493` (WRITE/READ/NOTIFY)

### Data flow
**Device → Web (notifications):**
```
D{distance},S{speed},score={score},p={p}
# example: D0.87,S1.25,score=53.2,p=0.61
```
**Web → Device (writes):**
- `S=<int>` sets the logistic boundary so that `p=0.5` at `score=S`.
- `buzz` triggers a short actuator test.

## Risk Scoring (on firmware)
```
p = 1 / (1 + exp(-(w0 + w1 * score)))
w0 = -w1 * S          # ensures p=0.5 at score=S
trigger when p ≥ 0.5  # equivalent to score ≥ S
```
- `S` is controlled from the web via the slider or presets.
- The browser **does not recompute** `score`; it displays values provided by the ESP32.

## Troubleshooting
- Must serve over https or `localhost` for Web Bluetooth.
- Ensure UUIDs match exactly between firmware and web app.
- Characteristic must have **NOTIFY** enabled; the browser calls `startNotifications()`.
- If no data appears, confirm firmware is sending `D...,S...,score=...,p=...` lines.

## License
This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See `LICENSE.txt` for details.
