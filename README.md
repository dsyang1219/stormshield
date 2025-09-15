# StormShield

StormShield is a wearable safety system built on **ESP32** with mmWave sensors and Bluetooth Low Energy (BLE). It pairs with a web application to provide real-time motion detection, scoring, and alerts. The project was developed for the 2025 Samsung Solve for Tomorrow contest.

## Features

- ESP32 firmware for mmWave motion detection  
- BLE communication between device and browser  
- Web app dashboard with live distance, speed, and score data  
- Adjustable sensitivity thresholds with preset modes  
- Score logging and chart visualization using Chart.js  
- Open-source under GPL-3.0  

## Project Structure

```
stormshield/
├── stormshield.ino    # Arduino firmware for ESP32
├── index.html         # Web app interface
├── style.css          # Web app styling
├── favicon.ico        # Application icon
├── README.md          # Documentation
└── LICENSE.txt        # GNU GPL v3 license
```

## Requirements

### Hardware
- ESP32 development board  
- mmWave radar sensor module  
- Power source (USB or battery)  

### Software
- [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support installed  
- Modern web browser (Chrome/Edge/Firefox with Web Bluetooth support)  

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/dsyang1219/stormshield.git
   cd stormshield
   ```

2. Upload the firmware:
   - Open `stormshield.ino` in Arduino IDE  
   - Select the ESP32 board and correct COM port  
   - Compile and upload to the device  

3. Open the web app:
   - Launch `index.html` in a supported browser  
   - Ensure Bluetooth is enabled on your computer  

## Usage

- Click **Connect** in the dashboard to pair the browser with the ESP32 device  
- Monitor live distance, speed, and score values  
- Adjust the **threshold slider** or select preset sensitivity modes  
- View logs and charts in the **Logs** section  

## Development Notes

- Risk score calculation:
  ```
  Score = (100 / distance) + speed
  ```
- Threshold settings are transmitted over BLE to the ESP32 in real time  
- Chart.js is used for score visualization in the browser  

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.  
See [LICENSE.txt](LICENSE.txt) for details.
