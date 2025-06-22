#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ✅ Define UART2 for C1001 mmWave Sensor
#define RX_PIN 32  // C1001 TX → ESP32 RX (matches your wiring)
#define TX_PIN 33  // C1001 RX ← ESP32 TX
#define BAUD_RATE 115200  

// ✅ Define Servo Pin
#define SERVO_PIN 27  
int scoreThreshold = 1;  // Default threshold

// ✅ Power Bank Fixes (PWM, LED, GPIO Load)
#define POWER_PIN 4        // Fake load pin
#define EXTRA_LOAD_PIN 5   // Second GPIO to toggle
#define LED_PIN 2          // Built-in LED
#define PWM_CHANNEL 0      // PWM Channel

// ✅ BLE UUIDs
#define SERVICE_UUID        "cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad"
#define CHARACTERISTIC_UUID "79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493"

// Globals
Servo servo;
HardwareSerial SensorSerial(2);

// BLE Callback class to handle threshold input
class ThresholdCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
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

void setup() {
    Serial.begin(115200);
    SensorSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    // Setup Servo
    servo.attach(SERVO_PIN);
    servo.write(0);

    // Power bank fix
    pinMode(POWER_PIN, OUTPUT);
    pinMode(EXTRA_LOAD_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    ledcSetup(PWM_CHANNEL, 5000, 8);
    ledcAttachPin(POWER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 128);

    // BLE Setup
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
}

void loop() {
    static String sensorData = "";
    while (SensorSerial.available()) {
        char c = SensorSerial.read();
        if (c == '\n') {
            sensorData.trim();
            if (sensorData.startsWith("Score:")) {
                int score = sensorData.substring(6).toInt();
                Serial.print("📡 Sensor Score: ");
                Serial.println(score);

                if (score >= scoreThreshold) {
                    Serial.println("⚠️ Score above threshold – rotating servo!");
                    servo.write(90);  // Alert position
                    delay(1000);
                    servo.write(0);   // Reset
                }
            }
            sensorData = "";
        } else {
            sensorData += c;
        }
    }

    // Toggle GPIO pins to simulate load
    digitalWrite(EXTRA_LOAD_PIN, millis() % 1000 < 500 ? HIGH : LOW);
}
