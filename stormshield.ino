#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <algorithm>  // std::remove_if
#include <cctype>     // ::isspace
#include <cmath>      // expf
#include <string>

// ✅ UART2 for C1001
#define RX_PIN 32
#define TX_PIN 33
#define BAUD_RATE 115200

// ✅ Servo
#define SERVO_PIN 27

// === Logistic Regression (score → probability) ===
// We only adjust the *score threshold* S via BLE.
// Internally, we set w0 so that p=0.5 at score=S:  w0 = -w1 * S
static float w1 = 0.20f;        // slope (tune in code if needed)
static float w0 = 0.0f;         // intercept (auto-set from S)
static int   scoreThreshold = 50; // default S

// Convenience: recompute intercept to align p=0.5 at S
static inline void set_boundary_at_score(int S) {
  scoreThreshold = S;
  w0 = -w1 * (float)S;
}

// ✅ Power-bank keepalive
#define POWER_PIN 4
#define EXTRA_LOAD_PIN 5
#define LED_PIN 2
#define PWM_CHANNEL 0

// ✅ BLE
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad"
#define CHARACTERISTIC_UUID "79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493"

Servo servo;
HardwareSerial SensorSerial(2);

// --- Helpers ---
static inline float sigmoidf(float x) {
  if (x >= 20.0f) return 1.0f;
  if (x <= -20.0f) return 0.0f;
  return 1.0f / (1.0f + expf(-x));
}

static inline void trimSpaces(std::string& s) {
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
}

// BLE: accept only S=NN or bare integer (keeps UX identical to before)
class ThresholdCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string value = pChar->getValue();
    trimSpaces(value);
    if (value.empty()) return;

    bool handled = false;

    // S=NN form
    if (value.rfind("S=", 0) == 0 && value.size() > 2) {
      try {
        int S = std::stoi(value.substr(2));
        set_boundary_at_score(S);
        Serial.print("🎯 Score threshold set: S=");
        Serial.print(scoreThreshold);
        Serial.print("  → LR boundary p=0.5 at S; w0=");
        Serial.print(w0, 4);
        Serial.print(", w1=");
        Serial.println(w1, 4);
        handled = true;
      } catch (...) {
        Serial.println("⚠️ Failed to parse S. Use S=50");
      }
    }

    // Bare integer form
    if (!handled) {
      try {
        // If it parses as int, treat as S
        int S = std::stoi(value);
        set_boundary_at_score(S);
        Serial.print("🎯 Score threshold set (bare): S=");
        Serial.print(scoreThreshold);
        Serial.print("  → w0=");
        Serial.print(w0, 4);
        Serial.print(", w1=");
        Serial.println(w1, 4);
        handled = true;
      } catch (...) {
        Serial.println("⚠️ Unrecognized input. Send S=50 or just 50.");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  SensorSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

  // Init LR boundary at default S
  set_boundary_at_score(scoreThreshold);

  // Servo
  servo.attach(SERVO_PIN);
  servo.write(0);

  // Power-bank keepalive
  pinMode(POWER_PIN, OUTPUT);
  pinMode(EXTRA_LOAD_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  ledcSetup(PWM_CHANNEL, 5000, 8);
  ledcAttachPin(POWER_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 128);

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

  Serial.println("🚀 BLE Started and Advertising");
  Serial.print("🧠 LR: boundary p=0.5 at S=");
  Serial.print(scoreThreshold);
  Serial.print(" ⇒ w0=");
  Serial.print(w0, 4);
  Serial.print(", w1=");
  Serial.println(w1, 4);
}

void loop() {
  static String sensorData = "";
  while (SensorSerial.available()) {
    char c = SensorSerial.read();
    if (c == '\n') {
      sensorData.trim();
      if (sensorData.startsWith("Score:")) {
        int score = sensorData.substring(6).toInt();

        // Logistic probability
        float p = sigmoidf(w0 + w1 * (float)score);

        Serial.print("📡 Score=");
        Serial.print(score);
        Serial.print("  p=");
        Serial.println(p, 4);

        // Decision: p >= 0.5 (i.e., score is on/above S boundary)
        if (p >= 0.5f) {
          Serial.println("✅ p >= 0.5 (>= S) → TRIGGER");
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

  // Fake load toggling
  digitalWrite(EXTRA_LOAD_PIN, millis() % 1000 < 500 ? HIGH : LOW);
}
