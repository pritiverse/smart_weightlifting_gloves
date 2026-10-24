# GRIPGUARD — Demonstration & Project Evaluation Script

This script provides a step-by-step procedure for demonstrating GRIPGUARD during project evaluations, academic reviews, and laboratory demonstrations.

---

## 1. Setup & Pre-Demo Check (2 Minutes)

1. **Hardware Preparation:**
   - Connect ESP32 to computer via Micro-USB.
   - Open Arduino IDE -> Tools -> Serial Monitor at **115200 baud**.
   - Verify boot banner:
     ```
     [INIT] Initializing hardware acquisition subsystem... SUCCESS.
     [INIT] MPU6050 active at address 0x68
     [INIT] FSR calibrated on GPIO 34 (ADC 12-bit, 11dB attenuation).
     [INIT] Starting Network Manager & REST API Server... ONLINE.
     ```
2. **Dashboard Launch:**
   - In terminal, navigate to `dashboard/` and run:
     ```bash
     npm run dev
     ```
   - Open browser to `http://localhost:5173`.
   - Verify connection badge shows `ONLINE` with round-trip latency ($< 50\text{ ms}$).

---

## 2. Live Sensor Telemetry Demonstration (1 Minute)

1. Show the **Live Sensor Acquisition Panel**:
   - Move the glove up and down: Point to real-time **Pitch** and **Roll** tilt gauges responding instantly without lag.
   - Point to the **Rate: 50 Hz** indicator and ESP32 die temperature.
2. Demonstrate **FSR Grip Pressure**:
   - Squeeze the palm sensor firmly: Show the **FSR Relative Grip Pressure** bar rising into the green zone past the $35\%$ threshold marker.
   - Release fingers: Show the bar dropping immediately to resting baseline ($< 10\%$).

---

## 3. Guided Personal Calibration Demonstration (2 Minutes)

1. Click the **Personal Calibration** button in the dashboard header.
2. The 5-Step Wizard modal opens:
   - **Step 1 (Preflight):** Show automatic sensor sanity check passing.
   - **Step 2 (Neutral Wrist):** Hold hand flat for 3.0 seconds. Point to the live progress bar.
   - **Step 3 (Relaxed Grip):** Rest hand lightly on a dumbbell handle for 2.0 seconds.
   - **Step 4 (Firm Squeeze):** Squeeze firmly for 2.0 seconds.
   - **Step 5 (Validation):** Show statistical validation passing:
     - Point to the **Separation Ratio $\ge 3.0$** (e.g., $26.4$). Explain that this mathematically guarantees $<0.13\%$ overlap between relaxed and firm grip states.
     - Note that the profile is atomically written to ESP32 flash memory (`Preferences` with CRC32).

---

## 4. Valid Repetition Counting Demonstration (1.5 Minutes)

1. Ensure the active profile is **Bicep Curl** (selected on dashboard).
2. Execute a smooth, deliberate repetition:
   - Start from resting horizontal posture.
   - Squeeze the weight firmly (grip bar $> 35\%$).
   - Curl upward through $> 40^\circ$ excursion. Show the movement phase change:
     `IDLE` $\rightarrow$ `READY` $\rightarrow$ `CONCENTRIC` $\rightarrow$ `TRANSITION` $\rightarrow$ `ECCENTRIC`.
   - Return fully to the starting posture.
3. Show the **Validated Repetitions** counter increment by 1.
4. Point to the **Real-Time Movement Trajectory** SVG curve capturing the rep arc.

---

## 5. Rejection Classification & Form Intelligence (2 Minutes)

Demonstrate that GRIPGUARD rejects improper or partial movements:

1. **Test A: Partial Movement ("Cheating"):**
   - Perform a shallow $25^\circ$ half-curl.
   - Show the **Rejected Attempts** counter increment.
   - Show the alert banner: **`INSUFFICIENT_RANGE_OF_MOTION`**.
2. **Test B: Ballistic Jolt / Accidental Gesture:**
   - Rapidly flick the wrist up and down in $< 500\text{ ms}$.
   - Show rejection: **`DURATION_TOO_SHORT (BALLISTIC_SPIKE)`**.
3. **Test C: Movement Without Weight:**
   - Swing arm up while keeping fingers completely open (unloaded FSR).
   - Show rejection: **`INSUFFICIENT_GRIP_PRESSURE`**.
4. **Test D: Turnaround Duplicate Suppression:**
   - Execute a hard repetition with rapid turnaround bounce.
   - Show that the $450\text{ ms}$ refractory period strictly prevents double-counting.

---

## 6. Sensor Reliability & Fault Gating Demonstration (1.5 Minutes)

1. In Serial Monitor (or via simulation), send command `i` to inject a simulated **I2C Bus Dropout**.
2. Show the **Sensor Reliability Centre** on the dashboard immediately transition:
   - Overall Health: `SENSOR_UNAVAILABLE`.
   - Confidence drops to $0\%$.
   - Red diagnostic banner: "MPU6050 I2C communication lost".
   - Repetition counting becomes **strictly gated** (no false counts).
3. Send command `k` to clear the fault:
   - Show state enter `RECOVERING`.
   - Explain the **1500 ms continuous stability gate** that prevents contact bounce.
   - Show smooth recovery back to `HEALTHY`.

---

## 7. Session Data Export (1 Minute)

1. Point to the **Session Repetition Log** table showing every attempted rep with duration, peak ROM, grip %, and explanation.
2. Click **Export CSV**: Open the downloaded CSV spreadsheet showing formatted tabular workout metrics.
3. Click **Export JSON**: Open the complete JSON package showing firmware schema version and device ID.
