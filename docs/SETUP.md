# GRIPGUARD — Arduino IDE & Firmware Setup Guide

This guide walks through opening, configuring, compiling, and flashing the GRIPGUARD firmware onto an ESP32 development board using the Arduino IDE.

---

## 1. Prerequisites & Board Support

### 1.1 Arduino IDE
- Arduino IDE 2.x (or 1.8.19) installed.

### 1.2 ESP32 Board Package
If not already installed:
1. Open **Arduino IDE -> File -> Preferences**.
2. Add this URL to **Additional boards manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools -> Board -> Boards Manager...**, search for `esp32` by **Espressif Systems**, and click **Install**.

---

## 2. Opening the Project

1. Navigate to the project firmware directory:
   `c:\Users\priti\Desktop\SMARTGLOVES\firmware\SmartGlove\`
2. Double-click `SmartGlove.ino` to open the sketch in Arduino IDE.
3. Notice that `Config.h`, `SensorAcquisition.h`, and `SensorAcquisition.cpp` appear as tabs alongside `SmartGlove.ino`.

---

## 3. Board & Port Settings

In the Arduino IDE menu, configure:
- **Board:** `ESP32 Dev Module` (or `NodeMCU-32S`)
- **Upload Speed:** `921600` (or `115200` if upload fails)
- **CPU Frequency:** `240 MHz (WiFi/BT)`
- **Flash Frequency:** `80 MHz`
- **Flash Mode:** `QIO`
- **Flash Size:** `4MB (32Mb)`
- **Partition Scheme:** `Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)`
- **Core Debug Level:** `None`
- **Port:** Select your ESP32 COM port (e.g., `COM3`, `COM4`, etc.)

---

## 4. Compiling & Uploading

1. Click **Verify** (Checkmark icon) to compile the code.
   - The sketch relies on built-in standard `Wire.h` and ESP32 core libraries. No external library installations are required for Milestone 1.
2. Click **Upload** (Arrow icon).
   - If the console displays `Connecting........_____.....`, press and hold the **BOOT** button on your ESP32 board for 1-2 seconds until the flashing percentage begins.

---

## 5. Serial Monitor Verification (Milestone 1)

1. Open **Tools -> Serial Monitor**.
2. Set the baud rate in the bottom-right corner to **115200 baud**.
3. Press the **EN / RST** button on the ESP32.
4. You will observe the startup banner followed by real-time JSON telemetry streamed at 10 Hz:

```json
================================================================
  GRIPGUARD — SMART WEIGHTLIFTING GLOVE (Milestone 1 Mode)     
  Personalized Rep Validation & Sensor Reliability Intelligence 
================================================================
Hardware Mapping:
  [MPU6050] SDA: GPIO 21 | SCL: GPIO 22 | VCC: 3V3
  [FSR 10k] SIG: GPIO 34 (ADC1) | Pull-Down: 10 kOhm to GND
Acquisition Schedule: 50 Hz (20,000 us) | Serial Telemetry: 10 Hz
Type '?' in Serial Monitor for interactive commands.
================================================================
[INIT] Initializing hardware acquisition subsystem... SUCCESS.
[INIT] MPU6050 active at address 0x68
[INIT] FSR calibrated on GPIO 34 (ADC 12-bit, 11dB attenuation).

{"seq":1,"t_ms":100,"dt_us":20015,"jitter_us":15,"ax":0.12,"ay":9.81,"az":0.25,"gx":0.1,"gy":-0.2,"gz":0.0,"fsr_adc":45,"fsr_v":0.04,"fsr_kohm":814.2,"mpu_ok":1,"i2c_err":0,"temp_c":27.8}
{"seq":2,"t_ms":200,"dt_us":19998,"jitter_us":-2,"ax":0.11,"ay":9.80,"az":0.26,"gx":0.0,"gy":-0.1,"gz":0.1,"fsr_adc":44,"fsr_v":0.04,"fsr_kohm":832.5,"mpu_ok":1,"i2c_err":0,"temp_c":27.8}
```

### 5.1 Interactive Commands
In the Serial Monitor input bar:
- Send `?` -> Displays the diagnostic help menu.
- Send `s` -> Runs an on-demand I2C bus scan across GPIO 21/22 and reports the active MPU6050 address.
- Send `r` -> Re-runs hardware initialization without requiring an ESP32 hard reset.

---

## 6. Milestone 1 Acceptance Checklist

| Test Item | Verification Procedure | Expected Result | Pass/Fail Criteria |
|---|---|---|---|
| **I2C Bus Communication** | Boot ESP32 with MPU6050 connected | Output shows `MPU6050 active at address 0x68` | `mpu_ok: 1`, `i2c_err: 0` |
| **50 Hz Timing & Jitter** | Inspect `dt_us` and `jitter_us` in JSON | `dt_us` averages 20,000 $\mu\text{s}$, jitter $\le \pm 500\mu\text{s}$ | Steady 50 Hz acquisition |
| **Accelerometer Gravity Check** | Hold glove flat (palm down) | $A_y \approx 9.8\text{ m/s}^2$ or $A_z \approx 9.8\text{ m/s}^2$ | Matches Earth gravity vector |
| **Gyroscope Zero-Rate** | Leave glove stationary on table | $G_x, G_y, G_z < 2.0^\circ/\text{s}$ | Stationary zero-rate stability |
| **FSR Relaxed Response** | No pressure applied to FSR | `fsr_adc` $< 150$, `fsr_v` $< 0.15\text{V}$ | Low resting baseline |
| **FSR Firm Grip Response** | Squeeze FSR firmly with fingers | `fsr_adc` rises to $2000 - 3800$, `fsr_v` $> 1.8\text{V}$ | High dynamic range separation |
