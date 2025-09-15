/**
 * StormShield - ESP32 Firmware
 * - UART (pins 32/33) to read mmWave sensor lines
 * - Risk score computed on-device: inverse-distance (+ optional speed bump) with EMA smoothing
 * - Logistic-regression decision boundary p = sigmoid(w0 + w1 * Score), with p=0.5 at Score=S
 * - BLE GATT: WRITE (set S / buzz), READ, and NOTIFY (publish telemetry to web app)
 *
 * BLE UUIDs (must match the web app):
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

// ================== Pins & UART ==================
#define RX_PIN 32          // Sensor TX → ESP32 RX (U2RX)
#define TX_PIN 33          // Sensor RX ← ESP32 TX (U2TX)
#define BAUD_RATE 115200
#define SERVO_PIN 27

// ================== BLE UUIDs ====================
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad"
#define CHARACTERISTIC_UUID "79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493"

// ================== Risk score params ==================
// Inverse-distance core
static const float D_MIN_M   = 0.25f;  // clamp near distance (sensor’s reliable min)
static const float D_MAX_M   = 2.50f;  // ignore beyond this range
static const float SHAPE_GAM = 1.5f;   // gamma > 1 => steeper near field

// Optional speed bump (very light)
static const float V_MAX_MPS = 1.00f;  // speed considered “fast”
static const float ETA_SPEED = 0.20f;  // 0..0.3 recommended; 0 disables speed bump

// Smoothing
static const float EMA_ALPHA = 0.30f;  // 0..1; higher = snappier, lower = steadier

// ================== Logistic regression ==================
// p = sigmoid(w0 + w1 * Score), Score in [0..100]
// We expose only S via BLE and set w0 = -w1 * S so p=0.5 at Score=S
static float w1 = 0.20f;   // slope (how sharp the transition is)
static float w0 = 0.0f;    // intercept (derived from S)
static int   scoreThreshold = 50; // S (set via BLE: "S=55" or "55")

// ================== Globals ==================
Servo servo;
HardwareSerial SensorSerial(2);
float ema01 = 0.0f;       // EMA state in [0,1]

BLECharacteristic* gChar = nullptr; // for notifications

// ------------------ Utils ---------------------
static inline float clamp(float x, float a, float b){
  return x < a ? a : (x > b ? b : x);
}
static inline float clamp01(float x){ return clamp(x, 0.f, 1.f); }

static inline float sigmoidf(float x){
  if (x >= 20.0f) return 1.0f;
  if (x <= -20.0f) return 0.0f;
  return 1.0f / (1.0f + expf(-x));
}
static inline void set_boundary_at_score(int S){
  scoreThreshold = S;
  w0 = -w1 * (float)S;
}
static inline void trimSpaces(std::string& s){
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
}

// -------------- BLE: adjust S only --------------
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
        Serial.printf("Score threshold set: S=%d -> w0=%.3f, w1=%.3f\n", scoreThreshold, w0, w1);
        handled = true;
      } catch (...) { Serial.println("Failed to parse S. Use S=55"); }
    }
    if (!handled) {
      if (value == "buzz") {
        Serial.println("Buzz command received.");
        servo.write(90);
        delay(300);
        servo.write(0);
        handled = true;
      }
    }
    if (!handled) {
      try {
        int S = std::stoi(value);
        set_boundary_at_score(S);
        Serial.printf("Score threshold set (bare): S=%d -> w0=%.3f, w1=%.3f\n", scoreThreshold, w0, w1);
      } catch (...) {
        Serial.println("Unrecognized input. Send S=55 or just 55.");
      }
    }
  }
};

// -------------- Parsing: distance & speed ---------------
// Accepts flexible lines like:
//  "Dist:1.2m Speed:-0.3m/s", "d=120cm,v=-30cm/s", "1.20, -0.30" (m, m/s)
bool parseDistanceSpeed(const String& line, float& d_m, float& v_mps){
  String s = line; s.toLowerCase();
  float d = NAN, v = NAN;

  auto grabNumberAfter = [&](int idx)->float {
    if (idx < 0) return NAN;
    int start = s.indexOfAny("0123456789-.", idx);
    if (start < 0) return NAN;
    int end = start;
    while (end < s.length() && (isDigit(s[end]) || s[end]=='.' || s[end]=='-')) end++;
    return s.substring(start, end).toFloat();
  };

  // distance by key
  int id = s.indexOf("dist"); if (id < 0) id = s.indexOf("d=");
  if (id >= 0){
    float val = grabNumberAfter(id);
    if (!isnan(val)){
      String unit = s.substring(s.indexOf(String(val), id) + String(val).length());
      unit.trim();
      if (unit.startsWith("mm")) d = val / 1000.f;
      else if (unit.startsWith("cm")) d = val / 100.f;
      else d = val; // default meters
    }
  }
  // speed by key
  int iv = s.indexOf("speed"); if (iv < 0) iv = s.indexOf("v=");
  if (iv >= 0){
    float val = grabNumberAfter(iv);
    if (!isnan(val)){
      String unit = s.substring(s.indexOf(String(val), iv) + String(val).length());
      unit.trim();
      if (unit.startsWith("mm/s")) v = val / 1000.f;
      else if (unit.startsWith("cm/s")) v = val / 100.f;
      else v = val; // default m/s
    }
  }
  // CSV fallback "d, v" in meters
  if (isnan(d) || isnan(v)){
    int comma = s.indexOf(',');
    if (comma > 0){
      String a = s.substring(0, comma); a.trim();
      String b = s.substring(comma+1);  b.trim();
      float d2 = a.toFloat(), v2 = b.toFloat();
      if (!isnan(d2) && !isnan(v2)){ d = d2; v = v2; }
    }
  }

  if (isnan(d) || isnan(v)) return false;
  d_m = d; v_mps = v;
  return true;
}

// -------------- Inverse-distance + speed bump --------------
// Steps:
// 1) clamp d to [D_MIN, D_MAX]
// 2) u = normalized inverse distance in [0,1] (0 at D_MAX, 1 at D_MIN)
// 3) shape: f = u^gamma
// 4) optional speed bump m in [0,1]; blend: f' = (1-eta)f + eta*m
// 5) EMA smooth → scale to 0..100
int computeRiskScore(float d_m, float v_mps){
  float d = clamp(d_m, D_MIN_M, D_MAX_M);

  // Inverse-distance normalization to [0,1]
  float inv_d = 1.0f / d;
  float inv_min = 1.0f / D_MIN_M;
  float inv_max = 1.0f / D_MAX_M;
  float u = (inv_d - inv_max) / (inv_min - inv_max);   // 0 at far, 1 at near
  u = clamp01(u);

  // Shape near field
  float f = powf(u, SHAPE_GAM);

  // Optional speed bump
  float m = clamp01(fabsf(v_mps) / V_MAX_MPS);
  float f_mix = (1.0f - ETA_SPEED) * f + ETA_SPEED * m;

  // EMA smoothing
  ema01 = EMA_ALPHA * f_mix + (1.0f - EMA_ALPHA) * ema01;

  // Scale to 0..100
  int score = (int) lroundf(ema01 * 100.0f);
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  return score;
}

// ================== Setup ==================
void setup() {
  Serial.begin(115200);
  SensorSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

  set_boundary_at_score(scoreThreshold);

  servo.attach(SERVO_PIN);
  servo.write(0);

  // BLE
  BLEDevice::init("StormShield");
  BLEServer* pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService(SERVICE_UUID);
  gChar = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_READ  |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  gChar->setCallbacks(new ThresholdCallback());
  gChar->addDescriptor(new BLE2902()); // CCCD for notify
  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE Started and Advertising");
  Serial.printf("LR: p=0.5 at S=%d  -> w0=%.3f, w1=%.3f\n", scoreThreshold, w0, w1);
  Serial.printf("Params: Dmin=%.2fm Dmax=%.2fm gamma=%.2f  eta=%.2f  alpha=%.2f\n",
                D_MIN_M, D_MAX_M, SHAPE_GAM, ETA_SPEED, EMA_ALPHA);
  Serial.println("Send 'S=55' over BLE to shift the decision boundary; 'buzz' to test actuator.");
}

// ================== Loop ==================
void loop() {
  static String line = "";
  while (SensorSerial.available()) {
    char c = SensorSerial.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        float d_m, v_mps;
        if (parseDistanceSpeed(line, d_m, v_mps)) {
          int score = computeRiskScore(d_m, v_mps);
          float p = sigmoidf(w0 + w1 * (float)score);

          // Notify browser with a single ASCII line
          if (gChar) {
            char payload[128];
            snprintf(payload, sizeof(payload), "d=%.3f,v=%.3f,score=%d,p=%.4f", d_m, v_mps, score, p);
            gChar->setValue((uint8_t*)payload, strlen(payload));
            gChar->notify();
          }

          // Debug on USB serial
          Serial.printf("d=%.2f m  v=%.2f m/s  -> score=%d  p=%.3f\n", d_m, v_mps, score, p);

          // Decision: trigger when p >= 0.5  (equiv. score >= S)
          if (p >= 0.5f) {
            servo.write(90);
            delay(800);
            servo.write(0);
          }
        } else {
          // Optional legacy path: "Score:NN"
          if (line.startsWith("Score:") || line.startsWith("score:")) {
            int score = line.substring(6).toInt();
            float p = sigmoidf(w0 + w1 * (float)score);
            if (gChar) {
              char payload[96];
              snprintf(payload, sizeof(payload), "score=%d,p=%.4f", score, p);
              gChar->setValue((uint8_t*)payload, strlen(payload));
              gChar->notify();
            }
            Serial.printf("legacy score=%d  p=%.3f\n", score, p);
            if (p >= 0.5f) { servo.write(90); delay(800); servo.write(0); }
          } else {
            Serial.printf("Parse error: %s\n", line.c_str());
          }
        }
      }
      line = "";
    } else {
      line += c;
    }
  }

  delay(1); // yield
}
