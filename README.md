# GRIPGUARD — Adaptive Lifting Intelligence
### Personalized Repetition Validation & Sensor Reliability Intelligence for Smart Weightlifting Gloves

[![Target](https://img.shields.io/badge/Hardware-ESP32%20%7C%20MPU6050%20%7C%20FSR-cyan.svg)](#hardware-wiring)
[![Firmware](https://img.shields.io/badge/Firmware-Arduino%20C%2B%2B%20%7C%2050%20Hz-blue.svg)](firmware/SmartGlove/)
[![Frontend](https://img.shields.io/badge/Dashboard-React%2018%20%7C%20Vite%20%7C%20TypeScript-purple.svg)](dashboard/)
[![Branch](https://img.shields.io/badge/Git%20Branch-main-green.svg)](#git-compliance)

---

## 1. Project Overview

**GRIPGUARD** is a high-integrity embedded and IoT wearable system designed for smart weightlifting gloves. Unlike ordinary repetition counters that rely merely on accelerometer zero-crossings, GRIPGUARD provides:

1. **Deterministic 8-State Biomechanical FSM:** Enforces movement onset, concentric ascension, turnaround inflection, controlled eccentric descent, and return-to-start verification.
2. **Dynamic Grip-Eligibility Gating:** Requires active, confirmed grip pressure ($\ge 35\%\text{--}45\%$ of personal firm squeeze) throughout the concentric lift phase, rejecting gestures and unloaded arm swings.
3. **5-Step Personal Calibration Wizard:** Learns each lifter's resting neutral posture and calculates a Gaussian statistical separation ratio ($(\mu_{firm} - \mu_{relaxed})/(\sigma_{firm} + \sigma_{relaxed}) \ge 3.0$) before saving to ESP32 non-volatile storage with CRC32 integrity checks.
4. **Transparent Sensor Reliability Engine:** Continuously diagnoses MPU6050 and FSR health across 4 fault classes (Dropout, Drift, Context-Aware Stuck-At, Gain Loss) with a $1500\text{ ms}$ continuous stability recovery gate. Repetitions during sensor dropouts are gated or qualified.
5. **Decoupled Asynchronous REST Telemetry:** Dual-mode Wi-Fi (SoftAP `192.168.4.1` + Station) exposes versioned JSON APIs without stalling the 50 Hz sensor sampling loop. Wi-Fi modem sleep is disabled to ensure responsive, low-latency transmission.
6. **Dark-Mode Sports-Science Dashboard:** Responsive React 18 + Vite + TypeScript web interface featuring live SVG trajectory plotting, grip gauges, diagnostic confidence bars, and one-click CSV/JSON session exports.

---

## 2. Strict Hardware Pinout

| Sensor / Component | Pin / Terminal | ESP32 Connection | Electrical Role / Protocol |
|---|---|---|---|
| **MPU6050** | VCC | ESP32 `3V3` | 3.3V Power Supply |
| **MPU6050** | GND | ESP32 `GND` | Ground Reference |
| **MPU6050** | SDA | ESP32 `GPIO 21` | I2C Data Line (400 kHz Fast-Mode) |
| **MPU6050** | SCL | ESP32 `GPIO 22` | I2C Clock Line |
| **FSR Sensor** | Terminal 1 | ESP32 `3V3` | Voltage Divider High |
| **FSR Sensor** | Terminal 2 | ESP32 `GPIO 34` | Voltage Divider Midpoint (ADC1_CH6) |
| **10 kΩ Resistor** | Terminal 1 | ESP32 `GPIO 34` | Pull-Down to GND |
| **10 kΩ Resistor** | Terminal 2 | ESP32 `GND` | Ground Reference |

> **IMPORTANT:** GPIO 34 is an input-only ADC pin on the ESP32 ADC1 peripheral. It lacks internal pull-up/pull-down transistors. The $10\text{ k}\Omega$ external pull-down resistor to GND is strictly required.

---

## 3. Directory Structure

```
smart-weightlifting-glove/
├── firmware/
│   └── SmartGlove/
│       ├── SmartGlove.ino            # Main setup and non-blocking 50 Hz executive loop
│       ├── Config.h                  # Parameter defaults, pinouts, and tuning rationale
│       ├── SensorAcquisition.h       # MPU6050 I2C & 8x oversampled ADC headers
│       ├── SensorAcquisition.cpp     # Direct register-level Wire driver implementation
│       ├── SignalProcessing.h        # Stationary detection, bias tracking & complementary filter
│       ├── SignalProcessing.cpp
│       ├── Calibration.h             # 5-step guided calibration wizard & NVS persistence
│       ├── Calibration.cpp
│       ├── RepEngine.h               # 8-state movement FSM, exercise profiles & grip gating
│       ├── RepEngine.cpp
│       ├── FaultDetection.h          # 6-state sensor health model & 4 fault detectors
│       ├── FaultDetection.cpp
│       ├── NetworkManager.h          # Dual Wi-Fi (SoftAP/STA) & REST API server
│       ├── NetworkManager.cpp
│       ├── Arduino_Compat.h          # Desktop static analysis compatibility shims
│       └── secrets.example.h         # Wi-Fi credential isolation template
├── dashboard/
│   ├── package.json                  # React 18, Vite, TypeScript dependencies
│   ├── vite.config.ts
│   ├── index.html
│   └── src/
│       ├── main.tsx
│       ├── App.tsx                   # Main app with paced 250ms polling and export routines
│       ├── types.ts                  # TypeScript interfaces matching ESP32 JSON schema
│       ├── services/api.ts           # REST API client for live hardware streaming
│       ├── styles/index.css          # Sports-science dark mode Vanilla CSS design system
│       └── components/
│           ├── Header.tsx            # Connectivity status, network source toggle, and wizard button
│           ├── InfoGuide.tsx         # Floating contextual intelligence guide & help modal
│           ├── OverviewCards.tsx     # Hero valid/rejected counters, timer, and profile switcher
│           ├── MovementTrace.tsx     # Real-time SVG movement trajectory & ROM markers
│           ├── SensorPanel.tsx       # FSR grip pressure gauge and raw IMU values
│           ├── ReliabilityCentre.tsx # 6-state sensor health badges & diagnostic alerts
│           ├── CalibrationModal.tsx  # 5-step interactive guided calibration wizard
│           └── SessionReport.tsx     # Repetition history table with CSV/JSON export
├── tests/
│   ├── simulation/
│   │   └── test_rep_validation.py    # Python simulation test harness (6/6 passing)
│   └── test-scenarios.md             # Hardware, fault-injection, and repetition test matrix
└── docs/
    ├── PRITIKA_PROGRESS.md           # Persistent task checklist and progress log
    ├── DECISIONS.md                  # Architectural Decision Records (ADR 001 - 016)
    ├── ARCHITECTURE.md               # Subsystem data flow, memory budgets, and timing
    ├── WIRING.md                     # Exact electrical schematic and circuit transfer functions
    ├── SETUP.md                      # Arduino IDE and Vite setup/flash manual
    ├── CALIBRATION.md                # 5-step wizard protocol & distribution separation math
    ├── REPETITION_ENGINE.md          # 8-state FSM state transitions and rejection taxonomy
    ├── FAULT_DETECTION.md            # Sensor reliability model, stuck-at logic, and recovery gate
    ├── API.md                        # Complete REST API specification and JSON schemas
    ├── TROUBLESHOOTING.md            # Hardware, ADC noise, and network debug manual
    ├── LIMITATIONS.md                # Observable scientific boundary conditions
    └── DEMO_SCRIPT.md                # Step-by-step project evaluation & defense script
```

---

## 4. Quickstart Guide

### 4.1 Flashing Firmware to ESP32
1. Open `firmware/SmartGlove/SmartGlove.ino` in Arduino IDE.
2. Select Board: **ESP32 Dev Module** (or **NodeMCU-32S**).
3. Select COM Port, configure Baud to **115200**, and click **Upload**.
4. Open Serial Monitor at **115200 baud** to view real-time initialization and JSON telemetry.

### 4.2 Running the Web Dashboard
1. Open a terminal in the `dashboard/` directory:
   ```bash
   cd dashboard
   npm install
   npm run dev
   ```
2. Open your browser to `http://localhost:5173`.
3. Connect your laptop/phone Wi-Fi to **`GripGuard-AP`** (Password: `smartglove123`).
4. The dashboard will automatically connect and stream live telemetry at 10 Hz!
   *(Note: You can also click **"○ Offline Sim Mode"** in the header to evaluate all interactive features with synthetic biomechanical telemetry immediately without physical hardware).*

### 4.3 Running Offline Verification Tests
Run the standalone Python simulation test suite:
```bash
python tests/simulation/test_rep_validation.py
```
Expected output:
```
Results: 6 / 6 Passed (100.0%)
```

---

## 5. Architectural Decision Records (ADR Summary)

All engineering decisions are documented in [`docs/DECISIONS.md`](docs/DECISIONS.md):
- **DEC-001:** Strict hardware pinouts and GPIO 34 input-only ADC1 configuration.
- **DEC-002:** Direct register-level `Wire.h` driver for MPU6050 without third-party library bloat.
- **DEC-003:** Non-blocking 50 Hz hardware acquisition loop governed by `micros()`.
- **DEC-004:** 8x burst oversampling with trimmed-mean filtering for FSR electrical noise reduction.
- **DEC-005:** Stationary-gated leaky zero-rate gyroscope bias tracking ($\beta = 0.015$).
- **DEC-006:** Asymmetric dual-mode complementary orientation fusion (pitch & roll).
- **DEC-007:** 5-step statistical calibration protocol with distribution separation ($\ge 3.0$).
- **DEC-008:** Non-volatile storage persistence using ESP32 `Preferences` with IEEE 802.3 CRC32.
- **DEC-009:** Deterministic 8-state biomechanical movement finite state machine.
- **DEC-010:** Dynamic grip-aware repetition acceptance and 450 ms refractory lockout.
- **DEC-011:** 6-state sensor reliability model and assessment gating.
- **DEC-012:** Cross-sensor contextual stuck-at detection to eliminate resting false alarms.
- **DEC-013:** Asynchronous decoupled dual Wi-Fi (SoftAP + STA) and REST architecture.
- **DEC-014:** Stateless versioned JSON telemetry protocol with sequence tracking.
- **DEC-015:** React 18 + TypeScript + Vite dark-mode sports-science dashboard.
- **DEC-016:** Client-side biomechanical simulation mode & CSV/JSON data portability.

---

## 6. Git Compliance Status

- **Working Branch:** `Pritika` (strictly maintained)
- **Commits Created:** **0** (strictly adheres to rule: all changes remain unstaged in working tree for user review)
- **Pushes Performed:** **0**
- **Other Branches Modified:** **None**
