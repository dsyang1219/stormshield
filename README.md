# 🌩️ Storm Shield Web: AI-Powered Awareness for Hard-of-Hearing Athletes

> “No one should have to choose between safety and participation.”

Storm Shield is a **web-based wearable companion** designed to enhance spatial awareness for hard-of-hearing athletes. Built with Bluetooth Low Energy (BLE), real-time motion data, and a responsive web interface, Storm Shield empowers users to stay safe—whether they’re on the field, track, or street.

🏅 **2025 Samsung Solve for Tomorrow National Finalist**

---

## 🧭 Project Vision

Hard-of-hearing individuals often remove their hearing aids during sports to avoid damage from sweat, weather, or impact—compromising their ability to hear coaches or teammates. We created Storm Shield to **bridge that gap**, combining:

- Weather-resistant hardware for hearing aid protection  
- Motion-based AI alerts for environmental awareness  
- A sleek, cross-device web app for real-time control  

---

## 🌟 Features

### 🖥️ Responsive Web App (PWA-Ready)
- Built with HTML/CSS/JavaScript and **Chart.js**
- Responsive layout optimized for both phones and laptops
- Live BLE connection to ESP32 device (via Web Bluetooth API)
- Adjustable alert thresholds from browser UI
- Data visualization of motion scores with smooth transitions

### 🎯 Smart Motion Detection
- mmWave sensor (DFRobot C1001) reads motion and presence
- Scores are calculated based on signal strength and frequency
- If danger score exceeds user-set threshold, a servo activates a tactile alert

### 🔄 Real-Time BLE Communication
- Web Bluetooth connects directly to the ESP32-S3 board
- Sends new threshold settings from browser to device
- Receives live sensor scores for display and logging

### 🛡️ Wearable + Weatherproof
- Athletic headband houses sensor, servo, and ESP32 module
- Keeps hearing aids or earbuds safe from water and collisions
- Designed to be secure, breathable, and comfortable

---

## 🧰 Tech Stack

| Component        | Technology                                   |
|------------------|----------------------------------------------|
| Frontend         | HTML, CSS, JavaScript, Chart.js              |
| BLE Integration  | Web Bluetooth API                            |
| Microcontroller  | ESP32-S3 WROOM / LilyGO T-Display S3         |
| Sensor           | DFRobot C1001 mmWave Human Presence Sensor   |
| Output           | Servo motor vibration alert (GPIO 27)        |
| Hosting          | GitHub Pages                                 |
| Domain           | [danielleyang.com](https://danielleyang.com) |

---

## 🚀 Setup Instructions

### 🖥️ 1. Web App
- Open [danielleyang.com](https://danielleyang.com) in a supported browser (Chrome or Edge)
- Click **“Connect Device”** to pair via Bluetooth
- View live sensor data and set custom danger thresholds
- Review graph logs for past movement events

### ⚙️ 2. Hardware
- Flash ESP32 with provided Arduino code
- Wire sensor to UART2 (TX=33, RX=32) and servo to GPIO 27
- Power device via battery or USB
- Make sure BLE advertising is enabled

---

## 📸 UI Highlights

- **Dashboard** with motion score gauge and score history chart  
- **Settings Panel** to change thresholds dynamically  
- **About Us Tab** with team mission and Samsung competition summary  
- Smooth page transitions and mobile-first design

---

## 🧑‍🔬 Team & Story

> “This product was born from lived experience. As a hard-of-hearing athlete, I often couldn’t wear my hearing aids during practice—leaving me unaware and unsafe. Storm Shield is the solution I wish I had growing up.” — *Danielle Yang*

| Name             | Role                    | Focus Areas                        |
|------------------|-------------------------|------------------------------------|
| Danielle Yang    | Hardware & App Lead     | Sensor code, BLE web integration   |
| Audrey Adams | Prototype Developer      | Prototype design and creation         |
| Sara Carmona | Community Research | User interviews, testing, outreach |

---

## 🧠 Feedback + Recognition

- Tamika Catchings: “I love the concept—I really believe this could be a great product.”
- Coaches, engineers, and hard-of-hearing students helped shape design
- Selected as **Samsung Solve for Tomorrow 2025 National Finalist**

---

## 🔮 Roadmap

- [ ] Expand sensor logging for cloud-based analysis  
- [ ] Add PWA installation for offline control  
- [ ] Integrate pedestrian warnings (e.g. electric vehicle proximity)  
- [ ] Accessibility improvements (screen reader + keyboard nav)  
- [ ] AI enhancements for predictive movement patterns

---

## 🔗 Links

- 🔗 [Live Site](https://danielleyang.com)  
- 💡 [Samsung Solve for Tomorrow](https://www.samsung.com/us/solvefortomorrow/)  
- 📂 [Web App Code](./stormshield.ino)  

---

## 📜 License

MIT License — Open to students, makers, and accessibility-focused innovators.

> With Storm Shield, we’re amplifying abilities—because safety should be for everyone.

---


