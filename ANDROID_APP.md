# Android app plan to mirror the web dashboard

This document outlines how to build an Android app that matches the logic and visual structure of the existing web dashboard while talking to the same ESP32 firmware over BLE.

## Goals
- Reuse the firmware's BLE UUIDs and payload conventions for full interoperability.
- Present the same telemetry (distance, speed, score, proximity) with the web app's colors and layout cues.
- Support the same control writes: threshold updates (`S=<int>` plus single-byte payload) and the **Buzz** test command.

## Tech stack
- **Language:** Kotlin
- **UI:** Jetpack Compose for a lightweight single-activity UI that mirrors the web layout (four stat cards, threshold slider, and buttons).
- **BLE:** `BluetoothLeScanner`/`BluetoothGatt` with coroutines/`Flow` wrappers to mirror the web event-driven flow.
- **Min SDK:** 26+ (BLE + runtime permissions).

## BLE contract (matches firmware & web)
- **Service UUID:** `cda3dd4c-e224-4a47-93d3-7c7ccf77e5ad` (fallback: `0000ffff-0000-1000-8000-00805f9b34fb`).
- **Characteristic UUID:** `79e3ff4d-b3e1-4cc1-9096-5c1cbe0a1493` (fallback: `0000ff01-0000-1000-8000-00805f9b34fb`).
- **Telemetry format:** ASCII lines like `d=123.45,v=2.34,score=87,p=close` (legacy payloads with just `d=` are also accepted by the web UI; treat missing fields as nulls).
- **Writes:**
  - Threshold slider: send `S=<int>` as UTF-8, then write a single-byte value for compatibility with existing firmware logic.
  - Buzz test: write the string `buzz`.

## App architecture
- **Data layer:**
  - `BleRepository` handles scanning, connecting, enabling notifications, parsing characteristic notifications into a `Telemetry` data class, and exposing a `Flow<Telemetry>` plus connection state.
  - Parsing mirrors the web: split on commas, then on `=`; tolerate missing keys.
- **Domain/UI state:**
  - `DashboardViewModel` consumes the `Flow` and exposes Compose state for the four primary metrics plus connection status and threshold.
  - Threshold updates invoke `BleRepository.writeThreshold(value)`; buzz triggers `BleRepository.sendBuzz()`.
- **UI:**
  - Single-screen layout with a top connection banner/button, four cards for distance/speed/score/proximity, a slider with preset buttons for threshold, and a **Test Buzz** button.
  - Use the existing web colors (`#1d1d1f` backgrounds, accent greens/reds) to match the dashboard feel.

## Permissions & setup
- Request `BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` (and `ACCESS_FINE_LOCATION` on API < 31) at runtime before scanning.
- Ensure Bluetooth + Location are enabled; guide the user with a dialog if not.

## Key code snippets (sketch)
```kotlin
// BLE connection + notifications
val gatt = device.connectGatt(context, false, callback)
val characteristic = gatt.getService(SERVICE_UUID)?.getCharacteristic(CHAR_UUID)
gatt.setCharacteristicNotification(characteristic, true)
```

```kotlin
// Parsing telemetry
fun parsePayload(payload: String): Telemetry {
    val map = payload.split(',')
        .mapNotNull { it.split('=', limit = 2).takeIf { parts -> parts.size == 2 }?.let { it[0] to it[1] } }
        .toMap()
    return Telemetry(
        distance = map["d"]?.toDoubleOrNull(),
        speed = map["v"]?.toDoubleOrNull(),
        score = map["score"]?.toIntOrNull(),
        proximity = map["p"]
    )
}
```

```kotlin
// Threshold write (mirrors web app)
suspend fun writeThreshold(value: Int) {
    writeString("S=$value")
    writeBytes(byteArrayOf(value.toByte()))
}
```

## Build steps
1. Create a new Android Studio project (Empty Compose Activity, minSdk 26).
2. Add a module `ble` with the repository and parsing logic above.
3. Implement `DashboardViewModel` that wires the repository to UI state and exposes `connect`, `disconnect`, `writeThreshold`, and `sendBuzz` functions.
4. Implement the Compose UI matching the web layout and colors; bind slider and buttons to the ViewModel actions.
5. Test against the ESP32 firmware; verify telemetry updates, threshold writes, and buzz command.

## Future enhancements
- Persist the last connected device and threshold.
- Add signal strength and battery indicators if the firmware exposes them.
- Support OTA firmware updates once the ESP32 sketch adds that capability.
