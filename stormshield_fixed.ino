/**
 * StormShield - ESP32 Firmware
 * - mmWave sensing (placeholder: parses "D<dist>,S<speed>" from sensor/serial)
 * - Logistic-regression risk scoring on-device
 * - BLE GATT service: WRITE (threshold commands), NOTIFY (live telemetry)
 *
 * BLE UUIDs (match the web app):
 *   Service:        cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad
 *   Characteristic: 79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493
 */

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

// ================= BLE UUIDs =================
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad"
#define CHARACTERISTIC_UUID "79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493"

// ================= Logistic Regression =================
// Score -> p via p = sigmoid(w0 + w1 * score)
// We keep w1 fixed and move w0 so that p=0.5 at score=S (user threshold).
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

// ================= Servo / Alert =================
static const int SERVO_PIN = 13;
Servo servo;

// ================= Keepalive (optional current spike to keep regulator awake) =================
#define KEEPALIVE_INTERVAL_MS 1000
#define KEEPALIVE_BURST_MS     120
unsigned long lastKeepAlive = 0;
static inline void keepAliveCpuBurst() {
  unsigned long t0 = millis();
  volatile float x = 0.f;
  while (millis() - t0 < KEEPALIVE_BURST_MS) {
    x += sinf(x) * 0.0001f;  // meaningless FLOPs
  }
}

// ================= BLE characteristic (global for notify) =================
BLECharacteristic* gChar = nullptr;

// ================= Threshold command callback =================
static inline void trimSpaces(std::string& s) {
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
}

class ThresholdCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string value = pChar->getValue();
    trimSpaces(value);
    if (value.empty()) return;

    // Accept "S=<int>" to reposition LR boundary, and "buzz" for actuator test.
    if (value.rfind("S=", 0) == 0 && value.size() > 2) {
      int S = scoreThreshold;
      try { S = std::stoi(value.substr(2)); } catch (...) {}
      set_boundary_at_score(S);
      Serial.print("Score threshold set: S=");
      Serial.print(scoreThreshold);
      Serial.print("  -> w0=");
      Serial.println(w0, 4);
      return;
    }

    if (value == "buzz") {
      Serial.println("Buzz command received.");
      servo.write(90);
      delay(300);
      servo.write(0);
      return;
    }

    // If a single byte 0-100 arrives, treat as S for compatibility.
    if (value.size() == 1) {
      uint8_t b = static_cast<uint8_t>(value[0]);
      set_boundary_at_score((int)b);
      Serial.print("Score threshold set (byte): S=");
      Serial.println(scoreThreshold);
      return;
    }

    Serial.print("Unrecognized write: ");
    Serial.println(value.c_str());
  }
};

// ================= Sensor parsing helpers =================
// Expected ASCII like: "D0.87,S1.25"
static bool parseDS(const std::string& s, float& D, float& S) {
  // Very lightweight parser
  size_t dpos = s.find('D');
  size_t spos = s.find(",S");
  if (dpos == std::string::npos || spos == std::string::npos || spos <= dpos) return false;
  try {
    D = std::stof(s.substr(dpos + 1, spos - (dpos + 1)));
    S = std::stof(s.substr(spos + 2));
    return true;
  } catch (...) {
    return false;
  }
}

// ================= Setup / Loop =================
void setup() {
  Serial.begin(115200);
  delay(50);

  // Initialize servo
  servo.attach(SERVO_PIN);
  servo.write(0);

  // Initialize BLE
  BLEDevice::init("StormShield");
  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Characteristic supports WRITE (for commands) and NOTIFY/READ (to stream telemetry)
  gChar = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_READ  |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  gChar->setCallbacks(new ThresholdCallback());
  gChar->addDescriptor(new BLE2902());  // enables CCCD for notifications

  pService->start();
  pServer->getAdvertising()->start();

  // Initial LR boundary
  set_boundary_at_score(scoreThreshold);

  Serial.println("BLE Started and Advertising");
  Serial.print("LR boundary p=0.5 at S=");
  Serial.print(scoreThreshold);
  Serial.print("  -> w0=");
  Serial.println(w0, 4);
}

static std::string sensorBuf;

void loop() {
  // Example: read lines from Serial1 or a sensor driver.
  // For demo, we read from Serial (USB). Replace with actual sensor integration.
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (!sensorBuf.empty()) {
        float dist = 0.f, spd = 0.f;
        if (parseDS(sensorBuf, dist, spd)) {
          // Compute a *score* (domain-specific). Example: inverse distance + speed.
          // If your sensor driver already emits "score", replace this with that value.
          float score = (dist > 0.f ? (100.f / dist) : 1000.f) + spd;

          // Logistic probability at current boundary
          float p = sigmoidf(w0 + w1 * score);

          // Notify browser
          char payload[128];
          snprintf(payload, sizeof(payload), "D%.3f,S%.3f,score=%.2f,p=%.4f", dist, spd, score, p);
          if (gChar) {
            gChar->setValue((uint8_t*)payload, strlen(payload));
            gChar->notify();
          }

          // Also print to Serial for debug
          Serial.print("D="); Serial.print(dist, 3);
          Serial.print(" S="); Serial.print(spd, 3);
          Serial.print(" score="); Serial.print(score, 2);
          Serial.print(" p="); Serial.println(p, 4);

          // Example decision: trigger when p >= 0.5  (equiv. score >= S)
          if (p >= 0.5f) {
            servo.write(90);
            delay(300);
            servo.write(0);
          }
        } else {
          Serial.print("Parse error: ");
          Serial.println(sensorBuf.c_str());
        }
      }
      sensorBuf.clear();
    } else {
      sensorBuf.push_back(c);
    }
  }

  // Keep-alive CPU burst (optional)
  unsigned long now = millis();
  if (now - lastKeepAlive >= KEEPALIVE_INTERVAL_MS) {
    lastKeepAlive = now;
    keepAliveCpuBurst();
  }

  delay(1);
}
