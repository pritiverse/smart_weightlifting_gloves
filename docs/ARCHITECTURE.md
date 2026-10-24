# GRIPGUARD — End-to-End System Architecture

This document describes the component boundaries, execution timing, data flow, memory budget, and synchronization mechanisms of the Smart Weightlifting Glove system.

---

## 1. System Block Diagram & Subsystem Boundaries

```
+========================================================================================================+
|                                           PHYSICAL LAYER                                               |
|  - MPU6050 6-Axis IMU (VCC=3.3V, GND, SDA=GPIO 21, SCL=GPIO 22)                                        |
|  - Force Sensitive Resistor (FSR) with 10 kOhm Voltage Divider to ESP32 GPIO 34 (ADC1_CH6)             |
+========================================================================================================+
                                                    |
                                                    v
+========================================================================================================+
|                                    FIRMWARE ARCHITECTURE (ESP32)                                       |
|                                                                                                        |
|  [Hardware Acquisition Task] (50 Hz / 20,000 us non-blocking timer)                                    |
|    - Direct Wire register burst reads (14 bytes from MPU6050 REG_ACCEL_XOUT_H)                         |
|    - 8x Over-sampled ADC burst with trimmed mean on GPIO 34                                            |
|    - Sequence indexing & microsecond jitter monitoring                                                 |
|                                     |                                                                  |
|                                     v                                                                  |
|  [Signal Processing Engine]                                                                            |
|    - 50-sample circular ring buffer rolling variance estimator                                         |
|    - Stationary-gated leaky gyro zero-rate bias integrator (beta = 0.015)                              |
|    - Dual-mode complementary orientation filter (Pitch & Roll fusion)                                  |
|    - Asymmetric EWMA grip smoothing (fast attack, slower decay)                                        |
|                                     |                                                                  |
|                  +------------------+------------------+                                               |
|                  |                                     |                                               |
|                  v                                     v                                               |
|  [Personal Calibration Engine]         [Sensor Fault Detection Engine]                                 |
|    - 5-step guided wizard                - 4 fault classes: Dropout, Drift, Context Stuck-At, Gain Loss|
|    - Gaussian separation ratio >= 3.0    - 6-state health FSM (HEALTHY, DEGRADED, FAULT, etc.)         |
|    - NVS Preferences storage with CRC32  - 1500 ms continuous stability recovery gate                  |
|                  |                                     |                                               |
|                  +------------------+------------------+                                               |
|                                     |                                                                  |
|                                     v                                                                  |
|  [Repetition State Machine]                                                                            |
|    - 8-State movement FSM (IDLE -> READY -> CONCENTRIC -> TRANSITION -> ECCENTRIC -> EVALUATING)      |
|    - Biomechanical Range of Motion & duration bounds                                                   |
|    - Dynamic grip-eligibility gating (>= 35% firm grip required)                                       |
|    - 450 ms refractory lockout to prevent turnaround duplicate counts                                  |
|    - Rejection classifier (ROM, Duration, Grip, Return, Reliability)                                  |
|                                     |                                                                  |
|                                     v                                                                  |
|  [Network Manager & REST API Server] (Port 80, non-blocking asynchronous)                              |
|    - Concurrent SoftAP (192.168.4.1) + Station Wi-Fi mode                                              |
|    - CORS-enabled versioned endpoints: /api/telemetry, /api/calibration, /api/exercise, etc.           |
+========================================================================================================+
                                                    | Wi-Fi HTTP / REST (10 Hz Polling)
                                                    v
+========================================================================================================+
|                                    PRESENTATION LAYER (React / Vite)                                   |
|                                                                                                        |
|  - Dark-mode sports-science interface ("GRIPGUARD — Adaptive Lifting Intelligence")                   |
|  - Real-time SVG movement trace with ROM threshold markers                                             |
|  - Dynamic FSR grip pressure gauge and wrist orientation gauges                                        |
|  - Sensor reliability centre with live confidence scores and transparent fault alerts                  |
|  - 5-step interactive personal calibration wizard modal                                                |
|  - Session repetition history table with one-click CSV and JSON data export                            |
|  - Client-side realistic biomechanical simulation mode for immediate offline evaluation                |
+========================================================================================================+
```

---

## 2. Memory & Execution Budget

To ensure bounded latency and avoid CPU starvation, the firmware adheres to strict memory budgets:

| Subsystem | RAM Footprint | Execution Time per 20ms Slice | Allocation Strategy |
|---|---|---|---|
| **Sensor Acquisition** | ~180 bytes | ~450 $\mu\text{s}$ (I2C 400kHz + 8x ADC) | Static structures; zero dynamic heap |
| **Signal Processing** | ~480 bytes | ~65 $\mu\text{s}$ (Trig & variance) | Fixed 50-sample float ring buffers |
| **Personal Calibration** | ~120 bytes | ~30 $\mu\text{s}$ (Moment accumulators) | Static profile struct; NVS on commit |
| **Repetition Engine** | ~96 bytes | ~20 $\mu\text{s}$ (FSM logic) | Static state variables |
| **Fault Detection** | ~240 bytes | ~45 $\mu\text{s}$ (Contextual checks) | Fixed 50-sample variance buffers |
| **Network Manager** | ~4.2 KB (Buffers) | ~1.5 - 3.5 ms (When query arrives) | Reused String buffers; non-blocking |

**Total CPU Utilization at 50 Hz:** $\approx 3.2\%$ during normal acquisition, spiking to $\approx 18\%$ momentarily during active HTTP JSON payload serialization. Over $80\%$ of ESP32 dual-core cycles remain idle.

---

## 3. Data Flow & Synchronization

1. **Hardware Sampling Slice:** Every $20,000\mu\text{s}$, a non-blocking check against `micros()` triggers a synchronized read of MPU6050 and FSR.
2. **Snapshot Caching:** The acquired `RawSensorData` and computed `ProcessedSensorData` are copied atomically into `NetworkManager`'s cached buffer.
3. **HTTP Client Decoupling:** When a browser queries `GET /api/telemetry`, the server serializes the cached snapshot directly without taking locks or pausing the 50 Hz acquisition cycle.
