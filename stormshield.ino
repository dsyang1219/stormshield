#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

// ====================== User Tunables ======================
// Keep-alive via CPU spike
#define KEEPALIVE_INTERVAL_MS 1000   // How often to spike current
#define KEEPALIVE_BURST_MS     120   // How long to burn CPU each interval (80–200ms typical)

// UART2 for C1001 mmWave Sensor
#define RX_PIN 32  // C1001 TX → ESP32 RX
#define TX_PIN 33  // C1001 RX ← ESP32 TX
#define BAUD_RATE 115200

// Servo
#define SERVO_PIN 27

// BLE UUIDs
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad"
#define CHARACTERISTIC_UUID "79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493"

// ================= Logistic Regression =================
// We keep a fixed slope and move the intercept so that p=0.5 at score=S.
static float w1 = 0.20f;        // slope
static float w0 = 0.0f;         // intercept (auto-set from S)
static int   scoreThreshold = 50;

static inline void set_boundary_at_score(int S) {
  scoreThreshold = S;
  w0 = -w1 * (float)S;
}

static inline float sigmoidf(float x) {
  if (x >= 20.0f) return 1.0f;
  if (x <= -20.0f) return 0.0f;
  return 1.0f / (1.0f + expf(-x));
}

// ================= BLE threshold callback =================
static inline void trimSpaces(std::string& s) {
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
}

class ThresholdCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string value = pChar->getValue();
    trimSpaces(value);
    if (value.empty()) return;

    bool handled = false;

    if (value.rfind("S=", 0) == 0 && value.size() > 2) {
      try {
        int S = std::stoi(value.substr(2));
        set_boundary_at_score(S);
        Serial.print("Score threshold set: S=");
        Serial.print(scoreThreshold);
        Serial.print("  → w0=");
        Serial.print(w0, 4);
        Serial.print(", w1=");
        Serial.println(w1, 4);
        handled = true;
      } catch (...) {
        Serial.println("⚠Failed to parse S. Use S=50");
      }
    }

    if (!handled) {
      try {
        int S = std::stoi(value);
        set_boundary_at_score(S);
        Serial.print("Score threshold set (bare): S=");
        Serial.print(scoreThreshold);
        Serial.print("  → w0=");
        Serial.print(w0, 4);
        Serial.print(", w1=");
        Serial.println(w1, 4);
        handled = true;
      } catch (...) {
        Serial.println("⚠Unrecognized input. Send S=50 or just 50.");
      }
    }
  }
};

// =================== Globals ===================
Servo servo;
HardwareSerial SensorSerial(2);

// Keep-alive timing
unsigned long lastKeepAlive = 0;

// =================== Setup ===================
void setup() {
  Serial.begin(115200);
  SensorSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

  // Init LR boundary at default S
  set_boundary_at_score(scoreThreshold);

  // Servo
  servo.attach(SERVO_PIN);
  servo.write(0);

  // BLE
  BLEDevice::init("StormShield");
  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic* pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new ThresholdCallback());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  Serial.println("BLE Started and Advertising");
  Serial.print("LR: boundary p=0.5 at S=");
  Serial.print(scoreThreshold);
  Serial.print(" ⇒ w0=");
  Serial.print(w0, 4);
  Serial.print(", w1=");
  Serial.println(w1, 4);
}

// =================== CPU Keep-Alive Spike ===================
static inline void keepAliveCpuBurst() {
  // Burn CPU on purpose to spike current consumption briefly.
  unsigned long t0 = millis();
  while (millis() - t0 < KEEPALIVE_BURST_MS) {
    volatile float acc = 0.0f;
    for (int i = 0; i < 2500; ++i) {
      float a = (float)i * 0.0031f;
      float b = (float)i * 0.0017f + 0.5f;
      acc += sinf(a) * cosf(b) + sqrtf(a + 1.0f);
    }
    volatile uint32_t x = 0xA5A5A5A5;
    for (int k = 0; k < 5000; ++k) {
      x ^= (x << 5) + (x >> 3) + k;
    }
  }
}

// =================== Main Loop ===================
void loop() {
  static String sensorData = "";
  while (SensorSerial.available()) {
    char c = SensorSerial.read();
    if (c == '\n') {
      sensorData.trim();
      if (sensorData.startsWith("Score:")) {
        int score = sensorData.substring(6).toInt();

        float p = sigmoidf(w0 + w1 * (float)score);
        Serial.print("📡 Score=");
        Serial.print(score);
        Serial.print("  p=");
        Serial.println(p, 4);

        // Logistic decision: p >= 0.5 ↔ score >= S
        if (p >= 0.5f) {
          Serial.println("p >= 0.5 (>= S) → TRIGGER");
          servo.write(90);
          delay(1000);
          servo.write(0);
        }
      }
      sensorData = "";
    } else {
      sensorData += c;
    }
  }

  unsigned long now = millis();
  if (now - lastKeepAlive >= KEEPALIVE_INTERVAL_MS) {
    lastKeepAlive = now;
    keepAliveCpuBurst();
  }

  delay(1);
}
