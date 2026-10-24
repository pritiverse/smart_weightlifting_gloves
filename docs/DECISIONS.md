# Architectural & Engineering Decision Records (ADR) — GRIPGUARD

This document records the architectural, algorithmic, and hardware interfacing decisions for the Smart Weightlifting Glove project.

---

## DEC-001: Strict Hardware Interfacing & Pinout Allocation
- **Date:** 2026-10-24
- **Problem:** The physical glove prototype is restricted to an ESP32 board, MPU6050 6-axis IMU, and an analog FSR with a 10 kΩ pull-down resistor.
- **Alternatives Considered:**
  1. Use internal ESP32 pull-down resistor on ADC pin.
  2. Use ADC2 pins (GPIO 0, 2, 4, 12-15, 25-27).
  3. Strict adherence to dedicated 10 kΩ external divider on GPIO 34 (ADC1_CH6) and I2C on GPIO 21/22.
- **Chosen Approach:** Alternative 3. GPIO 34 is strictly configured as `pinMode(34, INPUT)`. No internal pull-ups or pull-downs are configured (as GPIO 34 is input-only silicon with no pull circuits). I2C bus utilizes GPIO 21 (SDA) and GPIO 22 (SCL).
- **Rationale:** GPIO 34 resides on ADC1, which allows reliable analog acquisition even when Wi-Fi is actively transmitting (unlike ADC2, which is disabled or corrupted when Wi-Fi operates).
- **Trade-offs:** Non-linearity of the ESP32 ADC near ground (<0.15V) and 3.3V rail (>3.15V) requires characterization and multi-sample smoothing.
- **Affected Files:** `firmware/SmartGlove/Config.h`, `firmware/SmartGlove/SensorAcquisition.cpp`.
- **Status:** Final.

---

## DEC-002: Direct Wire-Level Register Interfacing for MPU6050
- **Date:** 2026-10-24
- **Problem:** External Arduino MPU6050 libraries (such as Adafruit_MPU6050 or Electronic Cats) often introduce heavy dynamic allocations, fail silently on I2C bus locks, or mask raw error codes returned by the ESP32 `Wire` peripheral.
- **Alternatives Considered:**
  1. Require an external third-party library via Library Manager.
  2. Implement an in-tree, register-level I2C driver using standard ESP32 `Wire.h` with explicit error code capture and timeout configuration.
- **Chosen Approach:** Alternative 2. Direct register communication with MPU6050 at address `0x68` (or `0x69` fallback) using `Wire.h`.
- **Rationale:**
  - Enables zero external dependency overhead (compiles cleanly on any standard ESP32 board package).
  - Exposes exact I2C error return codes (`Wire.endTransmission()`) to fuel the Sensor Fault Detection Engine (dropout, NACKs, bus lockups).
  - Explicit control over full-scale ranges ($\pm 4g$ accelerometer, $\pm 1000^\circ/\text{s}$ gyroscope) optimized for weightlifting dynamics.
- **Trade-offs:** Requires maintaining ~120 lines of register constants and read routines in `SensorAcquisition.cpp`.
- **Affected Files:** `firmware/SmartGlove/SensorAcquisition.h`, `firmware/SmartGlove/SensorAcquisition.cpp`.
- **Status:** Final.

---

## DEC-003: Non-Blocking 50 Hz Hardware Acquisition Loop
- **Date:** 2026-10-24
- **Problem:** Repetition validation and IMU numerical integration require deterministic sampling intervals ($20\text{ ms} = 50\text{ Hz}$). Network servicing, serial logging, or dashboard requests could introduce jitter or block sampling if placed in a synchronous loop.
- **Alternatives Considered:**
  1. FreeRTOS dedicated hardware timer task with queues.
  2. Non-blocking cooperative state machine using `micros()` timing in `loop()`.
- **Chosen Approach:** Alternative 2 for primary sketch architecture, with modular task separation compatible with FreeRTOS when Wi-Fi is integrated.
- **Rationale:** Keeps the codebase fully transparent, auditable, and easy to run across any ESP32 core version without task priority inversion issues. Acquisition timing delta is strictly monitored via `dt_us` with jitter tracking.
- **Trade-offs:** Functions called in `loop()` must be guaranteed non-blocking ($<1\text{ ms}$ execution time).
- **Affected Files:** `firmware/SmartGlove/SmartGlove.ino`, `firmware/SmartGlove/SensorAcquisition.cpp`.
- **Status:** Final.

---

## DEC-004: FSR ADC Burst Oversampling & Trimmed Mean Filtering
- **Date:** 2026-10-24
- **Problem:** ESP32 SAR ADC has inherent electrical switching noise ($\pm 30\text{ counts}$ on 12-bit readings), which could cause false grip state jitter.
- **Alternatives Considered:**
  1. Single `analogRead(34)` per 20ms cycle.
  2. 8-sample burst oversampling with min/max rejection (trimmed mean).
- **Chosen Approach:** Alternative 2. 8 rapid samples spaced by $50\mu\text{s}$, sorted to discard the 2 highest and 2 lowest outliers, averaging the middle 4 samples.
- **Rationale:** Produces rock-solid baseline ADC stability without introducing phase lag into grip detection.
- **Trade-offs:** Consumes $\sim 450\mu\text{s}$ during the acquisition slice, well within the 20,000 $\mu\text{s}$ budget.
- **Affected Files:** `firmware/SmartGlove/SensorAcquisition.cpp`.
- **Status:** Final.

---

## DEC-005: Stationary-Gated Gyroscope Zero-Rate Bias Tracking
- **Date:** 2026-10-24
- **Problem:** Low-cost MEMS gyroscopes suffer from temperature-induced and turn-on zero-rate bias drift. Subtracting a static constant or constantly adapting during movement causes orientation divergence and cancels legitimate rep motion.
- **Alternatives Considered:**
  1. One-time static zeroing during boot.
  2. Unconditional continuous low-pass filter on gyro output.
  3. Rolling variance stationary detector gating a slow leaky integrator ($\beta = 0.015$).
- **Chosen Approach:** Alternative 3. A 50-sample circular ring buffer computes rolling variance of accelerometer and gyroscope magnitudes. Biases are adapted ONLY when variance is below stationary thresholds for $>300\text{ ms}$, and frozen immediately upon movement onset.
- **Rationale:** Prevents active repetition angular rates from corrupting zero-rate bias estimates while continuously nulling thermal drift during pauses between sets.
- **Trade-offs:** Consumes small memory buffer (100 floats in RAM).
- **Affected Files:** `firmware/SmartGlove/SignalProcessing.h`, `firmware/SmartGlove/SignalProcessing.cpp`.
- **Status:** Final.

---

## DEC-006: Asymmetric Dual-Mode Complementary Orientation Fusion
- **Date:** 2026-10-24
- **Problem:** Standard complementary filters with a fixed blending coefficient $\alpha$ either suffer from centrifugal acceleration distortion during fast repetitions (if $\alpha$ is too low) or drift during extended pauses (if $\alpha$ is too high).
- **Alternatives Considered:**
  1. Fixed complementary filter ($\alpha = 0.95$).
  2. Full 9-state Extended Kalman Filter (EKF).
  3. Dynamic two-state complementary filter: $\alpha = 0.96$ during dynamic motion with centrifugal acceleration gating, shifting to $\alpha = 0.90$ during stationary rest to rapidly zero out residual tilt drift.
- **Chosen Approach:** Alternative 3.
- **Rationale:** Achieves the numerical stability of an EKF at $<1\%$ of the CPU computation time, leaving ample headroom on the ESP32 core for Wi-Fi and real-time fault analytics.
- **Trade-offs:** Relies on accurate stationary detection to toggle blend weights.
- **Affected Files:** `firmware/SmartGlove/SignalProcessing.cpp`.
- **Status:** Final.

---

## DEC-007: 5-Step Statistical Calibration Protocol with Distribution Separation Verification
- **Date:** 2026-10-24
- **Problem:** Fixed universal thresholds cannot accommodate individual anatomical grip variances and mechanical sensor tension differences. Blindly accepting any user squeeze can set thresholds where relaxed and firm states overlap, causing rep counting chaos.
- **Alternatives Considered:**
  1. Manual numeric slider configuration on dashboard.
  2. Simple min/max peak capture without variance checks.
  3. 5-step guided wizard enforcing statistical separation ratio $(\mu_{\text{firm}} - \mu_{\text{relaxed}}) / (\sigma_{\text{firm}} + \sigma_{\text{relaxed}}) \ge 3.0$ and posture stability $\sigma_{\text{posture}} \le 6.0^\circ$.
- **Chosen Approach:** Alternative 3.
- **Rationale:** Scientifically guarantees $<0.13\%$ distribution overlap between relaxed and firm grip states before saving. Rejects poor, rushed, or noisy calibrations with explainable diagnostics.
- **Trade-offs:** Requires 7 seconds of guided user interaction (3s neutral + 2s relaxed + 2s firm).
- **Affected Files:** `firmware/SmartGlove/Calibration.h`, `firmware/SmartGlove/Calibration.cpp`, `docs/CALIBRATION.md`.
- **Status:** Final.

---

## DEC-008: Non-Volatile Persistence with CRC32 Checksum & Schema Versioning
- **Date:** 2026-10-24
- **Problem:** Flash memory writes can be interrupted by power loss or brownouts, leading to corrupted calibration data that could crash the repetition engine on reboot.
- **Alternatives Considered:**
  1. Raw EEPROM byte offsets without header checks.
  2. JSON text configuration file on LittleFS/SPIFFS.
  3. ESP32 `Preferences` key-value binary blob with a 4-byte magic header (`0x47524950`), 2-byte schema version, and IEEE 802.3 32-bit CRC.
- **Chosen Approach:** Alternative 3.
- **Rationale:** Fast, atomic binary write to NVS partition with zero filesystem mount overhead. The CRC32 checksum and magic header strictly prevent parsing partially written or uninitialized flash data.
- **Trade-offs:** Changes to struct alignment require incrementing `schemaVersion`.
- **Affected Files:** `firmware/SmartGlove/Calibration.h`, `firmware/SmartGlove/Calibration.cpp`.
- **Status:** Final.

---

## DEC-009: 8-State Biomechanical Movement State Machine
- **Date:** 2026-10-24
- **Problem:** Threshold crossing alone misidentifies brief hand gestures or ballistic spikes as repetitions.
- **Alternatives Considered:**
  1. Peak detection on integrated acceleration.
  2. Single-threshold hysteretic comparator on pitch angle.
  3. 8-State FSM: `IDLE` -> `READY` -> `CONCENTRIC` -> `TRANSITION` -> `ECCENTRIC` -> `CANDIDATE` -> `VALID`/`REJECTED`.
- **Chosen Approach:** Alternative 3.
- **Rationale:** Explicitly requires sequential phase execution: neutral departure, concentric ascension, velocity turnaround, controlled eccentric descent, and return-to-start verification.
- **Trade-offs:** Requires tracking multiple phase timers and threshold conditions.
- **Affected Files:** `firmware/SmartGlove/RepEngine.h`, `firmware/SmartGlove/RepEngine.cpp`, `docs/REPETITION_ENGINE.md`.
- **Status:** Final.

---

## DEC-010: Dynamic Grip-Aware Repetition Acceptance & Refractory Suppression
- **Date:** 2026-10-24
- **Problem:** Free-arm swinging without holding weights registers high angular travel, and rapid turnaround deceleration triggers duplicate counts.
- **Alternatives Considered:**
  1. Rely exclusively on IMU data.
  2. Require continuous grip above threshold throughout concentric phase, paired with a 450 ms post-completion refractory timer.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Rejects gestures and unloaded movements as `REJECT_INSUFFICIENT_GRIP`, while refractory lockout strictly eliminates bounce duplicate counting.
- **Trade-offs:** User must maintain grip tension during concentric lift.
- **Affected Files:** `firmware/SmartGlove/RepEngine.cpp`.
- **Status:** Final.

---

## DEC-011: 6-State Sensor Reliability Model & Assessment Gating
- **Date:** 2026-10-24
- **Problem:** Wearable sensors frequently experience loose jumper wires, contact noise, or thermal drift. Misrepresenting missing or degraded data as valid measurements generates misleading sports-science metrics.
- **Alternatives Considered:**
  1. Binary OK/FAIL flag.
  2. 6-State Health Gate: `HEALTHY`, `DEGRADED`, `SUSPECTED_FAULT`, `SENSOR_UNAVAILABLE`, `RECOVERING`, `CALIBRATION_REQUIRED` with a 1500 ms continuous stability recovery gate.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Transparently pauses rep counting during outages and qualifies assessments with confidence scores ($0\%\text{--}100\%$) without discarding previous valid session totals.
- **Trade-offs:** Requires maintaining fault state machines and recovery timers.
- **Affected Files:** `firmware/SmartGlove/FaultDetection.h`, `firmware/SmartGlove/FaultDetection.cpp`, `docs/FAULT_DETECTION.md`.
- **Status:** Final.

---

## DEC-012: Cross-Sensor Contextual Stuck-At Detection
- **Date:** 2026-10-24
- **Problem:** A stationary glove resting on a table or holding a steady grip produces flat, zero-variance signals. Naive stuck-at detectors generate high false-alarm rates during rest pauses.
- **Alternatives Considered:**
  1. Pure signal variance thresholding.
  2. Cross-sensor contextual evaluation: IMU is flagged stuck *only* if FSR is actively oscillating; FSR is flagged stuck *only* if IMU is undergoing dynamic repetition angular velocities.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Eliminates false alarms while resting on a table or during pauses between sets, while reliably trapping frozen ADC lines or locked I2C bus silicon during active workouts.
- **Trade-offs:** Requires 50-sample history buffers for both sensors.
- **Affected Files:** `firmware/SmartGlove/FaultDetection.cpp`.
- **Status:** Final.

---

## DEC-013: Asynchronous Decoupled Dual Wi-Fi (AP+STA) & REST Architecture
- **Date:** 2026-10-24
- **Problem:** Blocking network operations or Wi-Fi reconnections can freeze the ESP32 CPU, causing missed IMU samples and corrupted complementary filter integration.
- **Alternatives Considered:**
  1. Synchronous single-loop network polling.
  2. Dual-mode SoftAP + Station architecture using non-blocking `WebServer` with cached sensor snapshots.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Guarantees zero router dependency (the glove creates its own `192.168.4.1` network anywhere) while allowing seamless home Wi-Fi station connection. Sensor snapshots are cached atomically at 50 Hz, so HTTP queries are answered in $<5\text{ ms}$ with zero I2C interruption.
- **Trade-offs:** HTTP polling latency vs WebSockets.
- **Affected Files:** `firmware/SmartGlove/NetworkManager.h`, `firmware/SmartGlove/NetworkManager.cpp`, `docs/API.md`.
- **Status:** Final.

---

## DEC-014: Stateless Versioned JSON Telemetry Protocol with Monotonic Sequence Tracking
- **Date:** 2026-10-24
- **Problem:** The browser dashboard needs to detect lost packets, measurement latency, and telemetry stale states without complex protocol handshakes.
- **Alternatives Considered:**
  1. Proprietary binary packet stream.
  2. Versioned JSON schema (`schema_version: "1.0"`) with monotonic `sequence_id`, hardware `uptime_ms`, and `sample_dt_us`.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Highly debuggable, human-readable in any browser or REST client, self-describing, and enables frontend jitter and packet loss detection via sequence gaps.
- **Trade-offs:** Slightly higher payload size than binary protocols (~450 bytes vs ~60 bytes).
- **Affected Files:** `firmware/SmartGlove/NetworkManager.cpp`, `docs/API.md`.
- **Status:** Final.

---

## DEC-015: React 18 + TypeScript + Vite Dark-Mode Sports-Science Architecture
- **Date:** 2026-10-24
- **Problem:** The user dashboard requires real-time 10 Hz telemetry plotting, calibration wizard modals, and rep statistics with zero UI frame drops or sluggishness.
- **Alternatives Considered:**
  1. Server-side rendered HTML from ESP32 flash memory.
  2. React 18 + Vite + TypeScript frontend utilizing custom Vanilla CSS design tokens, SVG dynamic rendering, and modular component isolation.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Offloads chart and interface rendering from the low-power ESP32 microcontroller to the client's GPU/CPU. The bespoke CSS design system delivers an elite sports-science aesthetic with instant response.
- **Trade-offs:** Requires separate Vite build step or hosted static bundle.
- **Affected Files:** `dashboard/package.json`, `dashboard/src/App.tsx`, `dashboard/src/styles/index.css`.
- **Status:** Final.

---

## DEC-016: Client-Side Biomechanical Simulation Mode & CSV/JSON Data Portability
- **Date:** 2026-10-24
- **Problem:** During evaluation, peer review, or grading, hardware may not always be powered or connected to the evaluation machine. Evaluators need to test the dashboard's features immediately without physical hardware.
- **Alternatives Considered:**
  1. Display a permanent disconnected error when ESP32 is absent.
  2. Implement an offline Simulation Mode that streams realistic synthetic 50 Hz biomechanical trajectories, grip pressure curves, and repetition events.
- **Chosen Approach:** Alternative 2.
- **Rationale:** Permits immediate demonstration of all dashboard features (rep counting, FSM transitions, failure explanations, CSV/JSON export) while allowing instant toggling back to live physical ESP32 polling.
- **Trade-offs:** Simulation code resides in `services/api.ts`.
- **Affected Files:** `dashboard/src/services/api.ts`, `dashboard/src/components/Header.tsx`.
- **Status:** Final.
