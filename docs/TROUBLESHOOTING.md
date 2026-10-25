# GRIPGUARD — Hardware & Firmware Troubleshooting Guide

This manual covers diagnostic steps and corrective procedures for common electrical, sensor communication, and networking issues.

---

## 1. MPU6050 I2C Communication Issues

### Symptom: `[WARNING] MPU6050 did not respond or failed configuration`
- **Cause 1: Loose or Reversed Wiring**
  - Verify that ESP32 `GPIO 21` connects strictly to MPU6050 `SDA`.
  - Verify that ESP32 `GPIO 22` connects strictly to MPU6050 `SCL`.
  - Verify that MPU6050 `VCC` connects to `3V3` (NOT 5V).
- **Cause 2: Alternate I2C Address**
  - Standard MPU6050 address is `0x68`. If the `AD0` pin is connected to `3V3` instead of GND, the address shifts to `0x69`.
  - Fix: Send command `s` in the Serial Monitor. The firmware scans both addresses and automatically attaches to `0x69` if detected.
- **Cause 3: Missing I2C Pull-Up Resistors**
  - Most standard MPU6050 breakout boards (e.g. GY-521) include on-board $4.7\text{ k}\Omega$ pull-up resistors to 3.3V. If using a bare IC, add external $4.7\text{ k}\Omega$ pull-ups from SDA and SCL to 3.3V.

---

## 2. FSR & Voltage Divider Analog Issues

### Symptom: FSR reading remains pinned near 0 ADC counts
- **Cause 1: Open Circuit or Missing 3.3V connection**
  - Check that Terminal 1 of the FSR connects firmly to the ESP32 `3V3` rail.
  - Verify that Terminal 2 connects to ESP32 `GPIO 34`.
- **Cause 2: 10 kΩ Resistor Misplacement**
  - Ensure the $10\text{ k}\Omega$ resistor connects between `GPIO 34` and `GND`. If the resistor is accidentally connected between 3.3V and GPIO 34, the reading will be inverted.

### Symptom: FSR reading is saturated high ($ADC \ge 4080$) without touch
- **Cause: Terminal Short Circuit**
  - The FSR sensor leads may be shorted together at the soldering or breadboard junction. Inspect for bridged solder joints.

### Symptom: GPIO 34 ADC Noise Fluctuation ($\pm 30\text{ counts}$)
- **Normal ESP32 SAR ADC Behavior:**
  - The ESP32 ADC has thermal switching noise. GRIPGUARD natively handles this via **8x burst oversampling with trimmed-mean filtering** (`readFsrOversampled()`) and asymmetric EWMA smoothing. No extra external capacitor is required.

---

## 3. Wi-Fi & Dashboard Connectivity

### Symptom: Cannot connect to `GripGuard-AP` from phone or laptop
- **Procedure:**
  1. Open Wi-Fi settings on your phone or laptop.
  2. Search for the network named **`GripGuard-AP`**.
  3. Enter password: **`smartglove123`**.
  4. Once connected, open any modern browser and navigate to:
     `http://192.168.4.1`

### Symptom: Dashboard shows `OFFLINE / RECONNECTING` in dev mode
- **Cause: Target IP Mismatch**
  - When running `npm run dev` locally on your computer, your computer must either be connected to `GripGuard-AP` (in which case the target is `192.168.4.1`) or connected to the same home router as the ESP32.
  - Click the **Target** button in the dashboard header to update the IP address to match the ESP32 Station IP shown in the Serial Monitor.
- **Quick Evaluation Fallback:**
  - Click the **○ Offline Sim Mode** button in the dashboard header to immediately evaluate all interactive features with synthetic biomechanical telemetry.

---

## 4. ESP32 Slowness, High Latency & Frequent Disconnections

### Why "Clearing Cache" Does Not Fix ESP32 Slowness
Unlike a phone or PC, the ESP32 does not have an operating system cache or temp folder that accumulates junk. The ESP32 hardware cache is a tiny instruction cache on the silicon, and NVS flash only stores key-value pairs (like calibration). Erasing flash or NVS does not fix runtime lag or Wi-Fi dropouts.

### Root Causes & Solutions:

1. **Wi-Fi Modem Sleep (Default Power-Saving Mode)**
   - **Root Cause:** By default, the ESP32 radio sleeps between beacon intervals to conserve power. This introduces 100–300 ms latency spikes, packet drops, and timeouts.
   - **Fix:** Call `WiFi.setSleep(false);` immediately upon initialization in `NetworkManager.cpp`. This keeps the RF front-end fully awake and responsive.

2. **Excessive Dashboard Polling Rate (Overlapping HTTP Requests)**
   - **Root Cause:** The Arduino `WebServer` is synchronous and single-threaded. Firing HTTP GET requests at 10 Hz (100 ms) via `setInterval` causes requests to overlap whenever Wi-Fi latency fluctuates, exhausting TCP socket buffers and dropping the connection.
   - **Fix:** Use sequential polling with 250 ms pacing (`setTimeout` chained after the previous request completes) in `App.tsx`.

3. **Dual AP + Station Mode Interference (`WIFI_AP_STA`)**
   - **Root Cause:** The ESP32 has a single physical Wi-Fi radio. If Station mode cannot connect to your home router, the background driver continuously scans channels, which temporarily disconnects devices connected to `GripGuard-AP`.
   - **Fix:** If Station Wi-Fi is unconfigured or disconnected, `NetworkManager.cpp` automatically falls back to pure `WIFI_AP` mode to eliminate channel-hopping drops.

4. **Power Supply Brownout (USB Cable & Current Spikes)**
   - **Root Cause:** Wi-Fi transmit bursts draw sudden 300–500 mA spikes. Thin USB cables, unpowered USB hubs, or breadboard power rails can cause the 3.3V rail to dip below 2.7V, triggering a silent brownout reset.
   - **Diagnostic:** Check the Serial Monitor at 115200 baud for `Brownout detector was triggered` or `rst:0x10 (RTCWDT_RTC_RESET)`.
   - **Fix:** Use a high-quality, short data USB cable connected directly to the PC motherboard or a 5V/1A+ wall adapter. Add a 10 µF – 100 µF capacitor across the ESP32 3V3 and GND rails if prototyping on a breadboard.

5. **Dynamic String Heap Fragmentation**
   - **Root Cause:** Repeated string concatenation (`String json += ...`) across rapid telemetry responses fragments the FreeRTOS heap memory.
   - **Fix:** Pre-allocate string memory with `json.reserve(1024)` in `NetworkManager.cpp` before appending keys.
