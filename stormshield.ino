#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// === UART2 Configuration for C1001 Sensor ===
#define RX_PIN 32  // C1001 TX → ESP32 RX
#define TX_PIN 33  // C1001 RX ← ESP32 TX
#define BAUD_RATE 115200

// === Servo & Power Control Pins ===
#define SERVO_PIN 27
#define LED_PIN 2
#define POWER_PIN 4
#define EXTRA_LOAD_PIN 5

// === BLE UUIDs ===
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7cc7391f4b"
#define CHARACTERISTIC_UUID "f1ac9b44-381d-4c34-b04f-740efc5c9a7e"

// === Globals ===
HardwareSerial SensorSerial(2);
Servo servo;
int scoreThreshold = 1;

void setup() {
  Serial.begin(115200);
  SensorSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(LED_PIN, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);
  pinMode(EXTRA_LOAD_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(POWER_PIN, HIGH);
  digitalWrite(EXTRA_LOAD_PIN, HIGH);

  servo.attach(SERVO_PIN);
  servo.write(0);

  BLEDevice::init("StormShield");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->addDescriptor(new BLE2902());

  class ThresholdCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
      std::string value = pChar->getValue();
      value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
      if (!value.empty()) {
        int newThreshold = atoi(value.c_str());
        if (newThreshold > 0) {
          scoreThreshold = newThreshold;
          Serial.print("🎯 New threshold set via BLE: ");
          Serial.println(scoreThreshold);
        }
      }
    }
  };

  pCharacteristic->setCallbacks(new ThresholdCallback());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("✅ BLE Started");
}

void loop() {
  if (SensorSerial.available()) {
    String line = SensorSerial.readStringUntil('\n');
    if (line.indexOf("Det") != -1 && line.indexOf("sig") != -1) {
      int sigIndex = line.indexOf("sig:") + 4;
      String sigStr = line.substring(sigIndex);
      int score = sigStr.toInt();

      Serial.print("📡 Motion Score: ");
      Serial.println(score);

      if (score >= scoreThreshold) {
        Serial.println("🚨 ALERT — Servo Triggered!");
        servo.write(90);
        delay(300);
        servo.write(0);
      }
    }
  }
}
