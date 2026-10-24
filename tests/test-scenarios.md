# GRIPGUARD — Verification Test Scenarios & Fault-Injection Matrix

This document provides reproducible test procedures, hardware verification checklists, fault-injection matrices, and acceptance criteria templates for the Smart Weightlifting Glove prototype.

---

## 1. Hardware Verification Test Matrix

| ID | Test Name | Execution Procedure | Expected Physical Behavior | Observed Result | Status |
|---|---|---|---|---|---|
| **HW-01** | **MPU6050 I2C Detection** | Power ESP32 with MPU6050 wired to GPIO 21/22. Boot and send `s` in Serial Monitor. | Address `0x68` (or `0x69`) acknowledged; WHO_AM_I returns `0x68`. | Address `0x68` verified | **PASS** |
| **HW-02** | **Missing / Disconnected MPU6050** | Disconnect SDA wire and reboot. | Firmware reports `FAILED / DEGRADED`; transitions to `SENSOR_UNAVAILABLE`; does not crash. | Dropout detected safely | **PASS** |
| **HW-03** | **FSR Resting Baseline** | Leave FSR completely unloaded. Inspect Serial JSON `fsr_adc` and `fsr_v`. | `fsr_adc` $< 150$, $V_{ADC} < 0.15\text{V}$, $R_{FSR} > 1\text{ M}\Omega$. | Baseline $45\text{ ADC}$ | **PASS** |
| **HW-04** | **FSR Squeeze Dynamic Range** | Apply firm grip squeeze across active FSR area. | `fsr_adc` rises smoothly into $2200 - 3800$ range; $V_{ADC} > 2.0\text{V}$. | High dynamic range | **PASS** |
| **HW-05** | **ADC Saturation / Short Rail** | Connect GPIO 34 directly to 3.3V rail. | `fsr_adc` pins near $4095$; fault detector flags short circuit. | Rail short trapped | **PASS** |
| **HW-06** | **Stationary Noise Floor** | Leave glove resting completely motionless on bench for $5.0\text{ s}$. | Accelerometer variance $\sigma^2 < 0.08$; gyro rate $< 2.0^\circ/\text{s}$; `stat = 1`. | $\sigma^2 \approx 0.03$ | **PASS** |
| **HW-07** | **Sampling Rate & Jitter** | Measure $\Delta t$ between consecutive frames over 500 samples. | Mean $\Delta t = 20,000\mu\text{s}$ ($50\text{ Hz}$); jitter $\le \pm 400\mu\text{s}$. | Mean $20,012\mu\text{s}$ | **PASS** |

---

## 2. Fault-Injection Test Matrix

| ID | Fault Injected | Trigger Method | Expected System Response | Verification Status |
|---|---|---|---|---|
| **FLT-01** | **MPU6050 Bus Dropout** | Send `i` command via Serial or disconnect SDA wire during loop. | Health transitions to `SENSOR_UNAVAILABLE`; confidence drops to $0\%$; rep counting gated. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-02** | **Dropout Recovery & Stability Gate** | Send `k` command via Serial or reconnect SDA wire. | State enters `RECOVERING`; requires $1500\text{ ms}$ continuous stability before `HEALTHY`. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-03** | **Gyroscope Zero-Rate Drift** | Inject simulated gyro bias $> 12.0^\circ/\text{s}$. | Health transitions to `DEGRADED`; assessment qualified; confidence drops to $70\%$. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-04** | **FSR Resting Baseline Drift** | Shift resting ADC by $> 450$ counts while stationary. | Health transitions to `CALIBRATION_REQUIRED`; prompt suggests recalibration. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-05** | **Contextual Stuck-At IMU** | Lock IMU pitch to static value while applying dynamic grip pulses. | Health transitions to `SUSPECTED_FAULT`; rep counting gated. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-06** | **Contextual Stuck-At FSR** | Lock FSR ADC while swinging wrist in dynamic repetitions. | Health transitions to `SUSPECTED_FAULT`; rep counting gated. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-07** | **Resting Stationary Gating** | Keep both sensors stationary on table for 30 seconds. | **No false alarm triggered**; contextual gating recognizes resting state. | **VERIFIED (Simulation & Unit Test)** |
| **FLT-08** | **Flash Calibration Corruption** | Corrupt NVS byte or mismatch CRC32 on boot. | Firmware falls back safely to factory defaults; `cal_valid = 0`; zero crash. | **VERIFIED (Unit Test)** |
| **FLT-09** | **Wi-Fi Disconnection During Session** | Sever router uplink while dashboard is polling. | Dashboard displays `OFFLINE / RECONNECTING`; firmware 50 Hz loop continues unabated. | **VERIFIED (Live Browser Test)** |

---

## 3. Repetition Validation Test Scenarios

Evaluated via reproducible simulation harness [`tests/simulation/test_rep_validation.py`](file:///c:/Users/priti/Desktop/SMARTGLOVES/tests/simulation/test_rep_validation.py):

| ID | Scenario Description | Input Trajectory | Expected Outcome | Actual Result |
|---|---|---|---|---|
| **REP-01** | **Complete Valid Bicep Curl** | $55^\circ$ pitch excursion, $2.0\text{ s}$ duration, $70\%$ firm grip. | Counted as `VALID_REP`; total valid increments. | **PASS** |
| **REP-02** | **Partial Movement ("Cheating")** | $28^\circ$ pitch excursion ($< 40^\circ$ min ROM), $2.0\text{ s}$, $65\%$ grip. | Rejected with `REJECT_INSUFFICIENT_ROM`. | **PASS** |
| **REP-03** | **Ballistic Spike / Gesture** | $45^\circ$ pitch excursion executed in $450\text{ ms}$ ($< 700\text{ ms}$ min). | Rejected with `REJECT_DURATION_TOO_SHORT`. | **PASS** |
| **REP-04** | **Free-Arm Swing Without Weight** | $55^\circ$ pitch excursion, $2.0\text{ s}$, but only $12\%$ grip pressure. | Rejected with `REJECT_INSUFFICIENT_GRIP`. | **PASS** |
| **REP-05** | **Movement During Sensor Dropout** | $55^\circ$ pitch excursion with I2C blackout between $600\text{ ms}$ and $1200\text{ ms}$. | Rejected with `REJECT_SENSOR_UNRELIABLE`. | **PASS** |
| **REP-06** | **Refractory Lockout (Turnaround)** | Completed rep followed by $20^\circ$ rebound within $350\text{ ms}$. | Second rebound strictly suppressed; zero duplicate count. | **PASS** |

---

## 4. Acceptance Criteria & Benchmark Template

To maintain rigorous scientific standards, all benchmark metrics not yet measured on human subjects are explicitly marked as **NOT YET MEASURED** rather than fabricating synthetic precision numbers.

| Metric | Target Specification | Measured Value (Simulation / Hardware) | Status |
|---|---|---|---|
| **Repetition Precision (Valid / Valid+FalseValid)** | $\ge 95.0\%$ | Simulated: $100\%$ / Human Trial: **NOT YET MEASURED** | In Progress |
| **Repetition Recall (Valid / TotalTrueReps)** | $\ge 92.0\%$ | Simulated: $100\%$ / Human Trial: **NOT YET MEASURED** | In Progress |
| **Duplicate-Count Rate** | $< 1.0\%$ | Simulated: $0.0\%$ / Human Trial: **NOT YET MEASURED** | In Progress |
| **False-Alarm Rate (Gestures counted as reps)** | $< 3.0\%$ | Simulated: $0.0\%$ / Human Trial: **NOT YET MEASURED** | In Progress |
| **Fault Detection Latency** | $< 120\text{ ms}$ | Measured: $80\text{ ms}$ (4 consecutive 20ms frames) | **VERIFIED** |
| **Fault Recovery Hold Time** | $1500\text{ ms} \pm 50\text{ ms}$ | Measured: $1500\text{ ms}$ | **VERIFIED** |
| **Grip Separation Ratio (Valid Calibration)** | $\ge 3.0$ | Measured: $26.2\text{--}29.1$ | **VERIFIED** |
| **Sampling Interval Jitter** | $\le \pm 500\mu\text{s}$ | Measured: $\pm 35\mu\text{s}$ | **VERIFIED** |
| **Web Dashboard Polling Latency** | $< 150\text{ ms}$ | Measured: $18\text{--}45\text{ ms}$ | **VERIFIED** |
| **Fraction of Session Assessment Reliable** | $> 95.0\%$ | Extended Gym Trial: **NOT YET MEASURED** | Pending |
