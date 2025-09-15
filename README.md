# StormShield

StormShield is a wearable safety system built on **ESP32** with an mmWave motion sensor and Bluetooth Low Energy (BLE). It pairs with a browser-based web app to stream sensor data, compute a risk score, and trigger alerts. The project was developed for the 2025 Samsung Solve for Tomorrow contest.

## Features
- ESP32 firmware for mmWave motion detection and BLE GATT service
- Web app (HTML/CSS/JS + Chart.js) for live dashboards, logs, and settings
- Adjustable sensitivity thresholds (slider + presets), sent to the device over BLE
- Lightweight, no-build client: open in a modern browser that supports Web Bluetooth

## Repository Structure
```
stormshield/
├─ stormshield.ino      # ESP32 firmware (BLE + sensor)
├─ index.html           # Web app UI + BLE client + charts
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
- A browser with Web Bluetooth (Chrome/Edge; macOS/Windows/Linux/Android). Note: Web Bluetooth requires a secure context (https or `localhost`).

## Setup

### 1) Flash the firmware
1. Open `stormshield.ino` in Arduino IDE.
2. Select your ESP32 board and correct port.
3. Compile and upload.

### 2) Serve the web app
Web Bluetooth does not run from `file://` in most browsers. Serve the folder over HTTP(S). For a quick local server from the project root:
```bash
python3 -m http.server 8000
# then open http://localhost:8000/index.html
```

## Using the App
1. Open the web app and click **Connect** to pair with the ESP32 over BLE.
2. Watch **Distance**, **Speed**, and **Score** update in real time on the dashboard.
3. Use **Settings** to adjust the threshold slider or choose preset sensitivity levels.
4. Open **Logs** to view a rolling chart and timestamped score entries.

## BLE Interface (GATT)
- **Service UUID:** `cda3dd4c-e224-4a47-93d3-7c7cc39cb005`
- **Characteristic UUID:** `8b9843f5-2ed5-4bbd-abec-860f1f5ef2a7`

### Notifications (device → browser)
The firmware notifies the browser with ASCII messages in the form:
```
D{distance},S{speed}
# example: D0.87,S1.25
```

### Writes (browser → device)
- **Threshold update:** one-byte value (0–100) sent whenever the slider changes.
- **Buzz test:** the string `buzz` can be written to trigger a haptic/buzzer test on supported firmware.

## Risk Scoring
p = 1 / (1 + exp(-(β0 + β1·distance + β2·speed)))

# trigger when p ≥ threshold
```
Where `β` are model weights learned offline and embedded in the device or app.

## UI Overview
- **Dashboard:** BLE connect, connection status, live metrics (distance, speed, score)
- **Settings:** threshold slider + preset buttons; optional “buzz” test
- **Logs:** Chart.js time series of score + scrollable event log
- **About:** background and team images (optional)

## Troubleshooting
- The **Connect** button is disabled or fails: use Chrome/Edge and load the page via `https://` or `http://localhost`.
- Device not found: ensure the ESP32 is advertising the listed service UUID.
- No data in the chart: confirm the firmware sends `D{...},S{...}` notifications and that the browser has permission to access the device.
- Threshold not changing behavior: verify the firmware handles one-byte writes as a sensitivity parameter.

## License
This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See `LICENSE.txt` for details.
