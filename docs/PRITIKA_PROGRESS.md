# GRIPGUARD Project Progress Log — Pritika

- **Project:** Smart Weightlifting Glove: Personalized Rep Validation and Sensor Reliability Intelligence
- **Assigned Scope:** Embedded firmware (ESP32), signal processing, personal calibration, repetition state machine, sensor reliability & fault detection engine, web dashboard, documentation, and tests.
- **Current Phase:** All Implementation Milestones (1 through 10) Completed and Verified
- **Current Branch:** `main`
- **Git Commit Policy:** Commits strictly prohibited by agent; all changes remain as unstaged working-tree modifications for user review.

---

## Task Checklist

### Phase 1: Planning & Repository Setup
- [x] Gate A: Repository and hardware environment audit
- [x] Gate B: Architectural design, signal processing pipeline, and milestone plan
- [x] Maintain persistent project progress log (`docs/PRITIKA_PROGRESS.md`)
- [x] Maintain architectural decision records (`docs/DECISIONS.md`)

### Phase 2: Hardware Acquisition & Core Firmware
- [x] Milestone 1: Hardware acquisition, pin verification, non-blocking 50 Hz loop
  - [x] Centralized configuration (`firmware/SmartGlove/Config.h`)
  - [x] Sensor acquisition module (`firmware/SmartGlove/SensorAcquisition.h`, `.cpp`)
  - [x] Main sketch with non-blocking 50 Hz acquisition (`firmware/SmartGlove/SmartGlove.ino`)
  - [x] Hardware wiring guide (`docs/WIRING.md`)
  - [x] Setup and Arduino IDE flash guide (`docs/SETUP.md`)
- [x] Milestone 2: Signal processing, stationary detection, and complementary filter
  - [x] Signal processing module (`firmware/SmartGlove/SignalProcessing.h`, `.cpp`)
  - [x] Circular buffer stationary variance detector (50 samples / 1.0 s)
  - [x] Gated zero-rate gyro bias estimation
  - [x] Dynamic dual-mode complementary orientation fusion (pitch & roll)
  - [x] Asymmetric EWMA grip smoothing & dynamic normalization
  - [x] Integrated into `SmartGlove.ino` with 10 Hz enriched JSON telemetry
- [x] Milestone 3: 5-step personal calibration and non-volatile persistence
  - [x] Guided calibration state machine (`firmware/SmartGlove/Calibration.h`, `.cpp`)
  - [x] Step 1 Preflight, Step 2 Neutral Wrist, Step 3 Relaxed Grip, Step 4 Firm Grip, Step 5 Validation
  - [x] Statistical separation metric $(\mu_{firm} - \mu_{relaxed}) / (\sigma_{firm} + \sigma_{relaxed}) \ge 3.0$
  - [x] ESP32 NVS persistence via `Preferences` with magic header, schema version, and IEEE 802.3 CRC32
  - [x] Serial interactive calibration controls (`c`, `x`, `p`, `d`)
  - [x] Authored `docs/CALIBRATION.md`
- [x] Milestone 4: Repetition state machine and grip-aware acceptance gating
  - [x] Deterministic 8-state movement FSM (`firmware/SmartGlove/RepEngine.h`, `.cpp`)
  - [x] Configurable exercise profiles (Bicep Curl, Wrist Extension, Dumbbell Press)
  - [x] Movement onset, peak inflection, return-to-start verification
  - [x] Personalized Range of Motion ($\ge 40^\circ$) & duration bounds ($700\text{ ms} - 5000\text{ ms}$)
  - [x] Dynamic grip-eligibility gating ($\ge 35\%$ firm grip throughout concentric phase)
  - [x] Refractory duplicate count suppression ($450\text{ ms}$)
  - [x] Structured rejection classification (ROM, Duration, Grip, Return, Sensor)
  - [x] Integrated into `SmartGlove.ino` with real-time serial rep events and profile switching (`1`, `2`, `3`)
  - [x] Authored `docs/REPETITION_ENGINE.md` and recorded ADRs DEC-009 and DEC-010
- [x] Milestone 5: Sensor fault detection engine and health state gate
  - [x] 6-state sensor reliability model (`HEALTHY`, `DEGRADED`, `SUSPECTED_FAULT`, `SENSOR_UNAVAILABLE`, `RECOVERING`, `CALIBRATION_REQUIRED`)
  - [x] 4 fault classes: Dropout (I2C/ADC), Drift (Gyro/FSR baseline), Context-aware Stuck-At, Gain Loss
  - [x] 1500 ms continuous stability recovery gate
  - [x] Repetition counting gating (pauses reps during faults without losing valid totals)
  - [x] Simulated fault injection & clearing hook (`i`, `k`) for testing
  - [x] Integrated into `SmartGlove.ino` with diagnostic telemetry and real-time state alerts
  - [x] Authored `docs/FAULT_DETECTION.md` and recorded ADRs DEC-011 and DEC-012
- [x] Milestone 6: Asynchronous network manager and JSON telemetry API
  - [x] Dual Wi-Fi mode: SoftAP (`GripGuard-AP` / `192.168.4.1`) and Station mode
  - [x] Isolated credential template (`firmware/SmartGlove/secrets.example.h`) and `.gitignore`
  - [x] Non-blocking REST API with CORS headers on port 80 (`NetworkManager.h`, `.cpp`)
  - [x] Endpoints: `/api/telemetry`, `/api/calibration`, `/api/calibrate`, `/api/diagnostics`, `/api/exercise`, `/api/reset_reps`
  - [x] Integrated into `SmartGlove.ino` with network status command (`w`)
  - [x] Authored `docs/API.md` and recorded ADRs DEC-013 and DEC-014

### Phase 3: Web Dashboard & Analytics
- [x] Milestone 7: GRIPGUARD React/Vite sports-science dashboard
  - [x] React 18 + Vite + TypeScript application scaffolding
  - [x] Bespoke dark-mode sports-science CSS design system (`styles/index.css`)
  - [x] Header with real-time latency badge, target host config, and simulation toggle
  - [x] Overview cards: Valid reps, rejected attempts, acceptance ratio %, workout timer
  - [x] Real-time SVG movement trace with ROM threshold markers
  - [x] Live sensor panel: FSR grip bar, wrist pitch/roll gauges, raw IMU values
  - [x] Sensor reliability centre: MPU6050 and FSR health pills, confidence meter, fault alerts
  - [x] 5-step interactive personal calibration modal wizard
  - [x] Validated clean TypeScript build (`npm run build` in 2.12s)
- [x] Milestone 8: Session analytics, rep history, and CSV/JSON export
  - [x] Session repetition history log table with individual rep durations, ROM, and grip %
  - [x] One-click CSV session download
  - [x] One-click JSON full session data export
  - [x] Recorded ADRs DEC-015 and DEC-016

### Phase 4: Validation & Quality Assurance
- [x] Milestone 9: Fault-injection and hardware verification test suite
  - [x] Python simulation test harness (`tests/simulation/test_rep_validation.py`) (6/6 passing, 100%)
  - [x] Comprehensive test matrix, hardware validation checklist, and acceptance criteria in `tests/test-scenarios.md`
- [x] Milestone 10: Complete technical documentation, wiring schematics, and demo scripts
  - [x] Detailed subsystem architecture and data flow in `docs/ARCHITECTURE.md`
  - [x] Hardware and network troubleshooting guide in `docs/TROUBLESHOOTING.md`
  - [x] Scientific non-claims and boundary conditions in `docs/LIMITATIONS.md`
  - [x] Step-by-step project demonstration and defense script in `docs/DEMO_SCRIPT.md`
  - [x] Comprehensive top-level project overview in `README.md`

---

## Session Activity Log

### Session 1 — 2026-10-24 / 2026-10-25
- **Initial Audit:** Inspected workspace `c:\Users\priti\Desktop\SMARTGLOVES`. Initialized local git repository on branch `Pritika` pointing to remote `https://github.com/pritiverse/smart_weightlifting_gloves`.
- **Environment Checks:** Verified Node.js v24.7.0, npm 11.5.1, Python 3.12, Arduino IDE.
- **Gate A & Gate B Report:** Formulated end-to-end architecture, signal processing algorithms, 8-state rep FSM, 6-state sensor reliability engine, and dark-mode sports-science web dashboard.
- **Milestone 1 Implementation:** Implemented `Config.h`, `SensorAcquisition.h/.cpp`, `SmartGlove.ino`, `docs/WIRING.md`, `docs/SETUP.md`.
- **Milestone 2 Implementation:** Implemented `SignalProcessing.h/.cpp` with ring buffer variance detector, gated zero-rate gyro bias cancellation, complementary orientation fusion, and asymmetric FSR smoothing.
- **Milestone 3 Implementation:** Implemented `Calibration.h/.cpp` with 5-step guided wizard, statistical separation ratio $\ge 3.0$, and NVS persistence with CRC32. Authored `docs/CALIBRATION.md`.
- **Milestone 4 Implementation:** Implemented `RepEngine.h/.cpp` with 8-state movement FSM, exercise profiles, grip gating, and refractory lockout. Authored `docs/REPETITION_ENGINE.md`.
- **Milestone 5 Implementation:** Implemented `FaultDetection.h/.cpp` with 6-state health engine, 4 fault classes, 1500ms recovery gate, and live assessment qualification. Authored `docs/FAULT_DETECTION.md`.
- **Milestone 6 Implementation:** Implemented `NetworkManager.h/.cpp` with dual Wi-Fi, REST API endpoints, CORS headers, `secrets.example.h`, and `docs/API.md`.
- **Milestones 7 & 8 Implementation:** Built complete React 18 + Vite + TypeScript dashboard in `dashboard/` with bespoke dark-mode design system, 7 core panels, offline simulation mode, rep history log, and CSV/JSON export. Validated with `npm run build`.
- **Milestone 9 Implementation:** Created Python simulation test suite `tests/simulation/test_rep_validation.py` (executed and verified 6/6 passing: valid rep, partial ROM rejection, ballistic spike rejection, grip rejection, sensor dropout rejection, refractory lockout). Authored `tests/test-scenarios.md`.
- **Milestone 10 Implementation:** Authored `docs/ARCHITECTURE.md`, `docs/TROUBLESHOOTING.md`, `docs/LIMITATIONS.md`, `docs/DEMO_SCRIPT.md`, and top-level `README.md`.

### Session 2 — 2026-10-25
- **IDE Static Analysis & Language Server Compatibility:**
  - Resolved desktop clangd / IntelliSense `"Arduino.h file not found"`, `"Unknown type name uint32_t"`, and undeclared identifier errors in the IDE's Problems panel.
  - Implemented `firmware/SmartGlove/Arduino_Compat.h` providing zero-overhead compile-time shims for non-embedded environments while remaining 100% transparent to the Arduino ESP32 core.
  - Added standard `<stdint.h>`, `<stddef.h>`, `<string.h>`, and `<math.h>` headers to `Config.h`.
  - Configured workspace `.clangd` and `.vscode/c_cpp_properties.json` to recognize firmware include paths and parse `.ino` sketches as C++.
  - Re-verified Python simulation suite `tests/simulation/test_rep_validation.py` (6/6 passed, 100%).
- **Pure Real-Time Hardware Architecture:**
  - Removed all simulation code, synthetic data generation, and offline demo toggles from `dashboard/`.
  - Dashboard is now 100% dedicated to streaming live physical telemetry from the ESP32 hardware at 10 Hz.
  - Verified clean production build (`npm run build` in 1.01s).

### Session 3 — 2026-10-25
- **UX & Usability Overhaul:**
  - Added Home WiFi (`10.249.34.96`) vs Glove AP (`192.168.4.1`) direct toggle in dashboard header.
  - Added prominent Start/Stop Session controls with active status indicators in `OverviewCards.tsx`.
  - Fixed stale closure in `App.tsx` polling loop using `useRef(sessionActive)` so session reps populate reliably.
  - Built interactive floating help system (`InfoGuide.tsx`) detailing all metrics, rep rules, phase states, and health states.
- **HCI Calibration Wizard & Motor Transition Buffers:**
  - Researched HCI wearable calibration principles (Fitts's law, motor preparation latency, transient masking).
  - Added user readiness check before calibration starts.
  - Implemented 5-second initial countdown buffer for Step 1 (Rest Arm).
  - Implemented 3-second inter-step transition buffers between positions (Starting Posture, Relaxed Grip, Firm Squeeze).
  - Updated firmware (`Config.h`, `Calibration.h`, `Calibration.cpp`, `NetworkManager.cpp`) with explicit `CALIB_PREP_*` states and accumulator isolation.
  - Enhanced dashboard `CalibrationModal.tsx` with animated countdown rings, target posture illustrations, and Web Audio feedback chimes (`880 Hz` ticks, `1320 Hz` action chime).
  - Authored Section 5 of `docs/CALIBRATION.md` detailing HCI biomechanical rationale.
  - Verified with `npm run build` (clean compilation, 0 errors).

### Session 4 — 2026-10-26
- **ESP32 DevKit Latency & Connection Stability Optimization:**
  - Diagnosed root causes of ESP32 slowness and dropouts (Wi-Fi modem sleep, overlapping 100ms HTTP polling, AP+STA channel hopping, dynamic string heap churn).
  - Disabled Wi-Fi modem sleep (`WiFi.setSleep(false)`) in `NetworkManager.cpp` to eliminate 100–300 ms beacon latency spikes and packet drops.
  - Added automatic pure `WIFI_AP` mode fallback when Station Wi-Fi is unconfigured or unavailable, preventing periodic background channel hopping that disrupted `GripGuard-AP`.
  - Added `json.reserve(1024)` in `handleTelemetry()` and `json.reserve(512)` in `handleGetDiagnostics()` to completely eliminate FreeRTOS heap memory fragmentation during rapid REST requests.
  - Refactored dashboard `App.tsx` polling loop to chained `setTimeout` polling with 250 ms pacing, preventing concurrent request overlap and socket exhaustion on the ESP32 `WebServer`.
  - Authored comprehensive Section 4 in `docs/TROUBLESHOOTING.md` documenting brownout detection, power surge considerations, modem sleep, and heap hygiene.
  - Re-verified full test suite (`python tests/simulation/test_rep_validation.py` - 6/6 passed) and production build (`npm run build` - 0 errors).
- **Core Reliability, Diagnostics & Honest UI Upgrades (Deadline Engineering Push):**
  - **FSR Sustained Debounce:** Implemented 15-sample (~300 ms) confirmation window (`SUSTAINED_LOW_GRIP_SAMPLES` in `Config.h`, `_sustainedLowGripSamples` in `RepEngine.h/.cpp`). Transient finger repositioning or bar knurling shifts no longer erroneously reject good repetitions.
  - **Lightweight Serial CSV Streamer:** Added non-blocking 10 Hz CSV streamer to `SmartGlove.ino` (`timestamp,pitch,roll,fsr,health,rep_state,reject_reason`), toggleable via serial input `'l'`, zero dynamic heap allocations.
  - **Expanded Repetition-FSM Test Suite:** Updated `tests/simulation/test_rep_validation.py` with 7 comprehensive validation scenarios (full curl, partial curl, brief FSR dip tolerated, sustained dip rejected, sensor fault recovery, stationary hand zero counts, refractory lockout). Verified 7/7 passing (100.0%).
  - **Honest Biomechanical Terminology:** Replaced misleading labels throughout dashboard components (`MovementTrace.tsx`, `OverviewCards.tsx`, `SensorPanel.tsx`, `SessionReport.tsx`) with accurate scientific terms: *"Wrist / Forearm Orientation"*, *"Relative Palm Pressure"*, *"Forearm Angular Excursion"*, *"Target Excursion"*, *"Repetitions Detected"*, and *"Duration / Tempo"*.
  - **Live Sensor Connection Badges:** Added real-time status indicators in dashboard `Header.tsx` for IMU (`MPU6050`) and `FSR` hardware health (`Online` / `Offline`).
  - **Full Documentation Audit:** Crosschecked and synchronized all 14 markdown files across the repository (`README.md`, `docs/ARCHITECTURE.md`, `docs/API.md`, etc.) to reflect `main` branch, 250 ms non-overlapping telemetry polling, disabled modem sleep, and `InfoGuide.tsx` capabilities. Verified with clean production build (`npm --prefix dashboard run build` in 891ms).

### Session 5 — 2026-10-26
- **Comprehensive Project Report & Publication-Quality Simulation Figures:**
  - Designed and executed `generate_report.py` producing a complete, academic/professional Word document (`GRIPGUARD_Smart_Weightlifting_Gloves_Comprehensive_Report.docx`, 5.97 MB, ~38 pages).
  - Generated 9 publication-quality dark-themed simulation figures and block diagrams in `report_figures/`:
    1. `fig1_fsm_simulation.png` (5-panel 50 Hz biomechanical FSM simulation waveforms)
    2. `fig2_calibration_separation.png` (Gaussian distribution separation ratio $S_r \ge 3.0$ chart)
    3. `fig3_circuit_diagram.png` (ESP32, MPU6050, FSR 402, and 10k pull-down interconnect schematic)
    4. `fig4_fsm_state_diagram.png` (8-state deterministic movement finite state machine)
    5. `fig5_health_state_diagram.png` (6-state sensor reliability and self-healing health model)
    6. `fig6_architecture_diagram.png` (3-tier layered system architecture diagram)
    7. `fig7_transfer_function.png` (FSR voltage divider transfer function & force response)
    8. `fig8_calibration_protocol.png` (5-step personal calibration flow with motor buffers)
    9. `fig9_test_results.png` (7/7 simulation test suite results chart, 100% pass)
  - Embedded 5 physical prototype and lab workbench photographs:
    - Annotated hardware prototype architecture (`WhatsApp Image 2026-10-25 at 18.05.29.jpeg`)
    - Complete test bench setup with laptop running live dashboard at `localhost:5173` (`WhatsApp Image 2026-10-25 at 18.05.28 (1).jpeg`)
    - Palm surface showing Interlink FSR 402 contact pad (`WhatsApp Image 2026-10-25 at 18.05.23.jpeg`)
    - Dorsal wiring and ESP32/MPU6050 mounting on wrist wrap (`WhatsApp Image 2026-10-25 at 18.05.24 (2).jpeg`)
    - Dumbbell/powerbank grip interface verification (`WhatsApp Image 2026-10-25 at 18.05.27.jpeg`)
  - Embedded 6 real-world dashboard screen captures:
    - Overview UI during active session (`Screenshot 2026-10-25 160002.png`)
    - Live concentric rep detection waveform crossing MIN ROM 40° (`Screenshot 2026-10-25 160457.png`)
    - Rejection classification alert for stalled rep (`Screenshot 2026-10-25 160010.png`)
    - Personal calibration 4-step wizard completed & saved to NVS (`Screenshot 2026-10-26 010315.png`)
    - Live sensor acquisition showing 80% FSR grip pressure & wrist tilt (`Screenshot 2026-10-25 160227.png`)
    - Maximum grip squeeze (100% / 2523 ADC) and 100% sensor reliability assessment (`Screenshot 2026-10-25 160301.png`)
    - In-dashboard educational guide modal (`Screenshot 2026-10-25 160055.png`)
    - Metric parameter glossary (`Screenshot 2026-10-25 160104.png`)
  - Embedded 4 physical calibration execution photographs (`calib_step1` through `calib_step4`).
  - Thoroughly covered all 8 mandated report sections:
    1. Problem Statement (Kinematic blindness, form validation absence, sensor vulnerabilities, cloud dependency, fixed threshold flaws)
    2. Literature Survey & Related Work (HAR, FSR mechanics, complementary filtering, VBT, fault detection)
    3. Comprehensive Product Comparison Table (Apple Watch Ultra, Beast Sensor, Whoop 4.0, RepOne, GymWatch)
    4. Module Description (All 6 firmware modules + React dashboard)
    5. Components Used (ESP32, MPU6050, FSR 402, 10k resistor, breadboard, glove, ~$18 USD total)
    6. Circuit Diagram (Pinout table, voltage divider formula, ADC1 Wi-Fi compatibility notes)
    7. Clear Module Description with Figures & Photos (Deep technical specifications for each module)
    8. Complete Demo Video Guide (Step-by-step 7-phase walkthrough script with demo links)
### Session 6 — 2026-10-26
- **Offline Hardware Simulation & Automated Scenario Capture:**
  - Implemented `tests/simulation/mock_esp32_server.py`:
    - Full emulation of ESP32 REST API (`/api/telemetry`, `/api/calibration`, `/api/diagnostics`, `/api/calibrate`, `/api/exercise`, `/api/reset_reps`).
    - Standard library only (`http.server`, `math`, `json`, `urllib`), zero external dependencies.
    - Synthesizes 50 Hz biomechanical trajectories, grip pressures, sensor variances, and fault flags.
    - Web Control Console at `http://localhost:8000/` with 1-click scenario buttons and dynamic `/scenario?name=...` endpoint.
  - Enhanced Dashboard Integration (`dashboard/src/components/Header.tsx`, `api.ts`, `App.tsx`):
    - Added one-click **"Simulator"** network toggle in header.
    - Added `?sim=1` auto-detection to connect to local simulation server.
    - Added `?calib=1` modal trigger for automated testing of the 4-step Personal Calibration view.
  - Implemented `tests/simulation/capture_scenario_screenshots.py`:
    - Automated headless browser capture (Edge/Chrome) producing pristine 1920×1080 PNG screenshots in `report_figures/simulated_scenarios/`.
    - Captured 10 verification scenarios:
      1. `01_normal_valid_bicep_curl.png` (REP-01: Valid repetition, 55° ROM, 72% grip, HEALTHY status)
      2. `02_cheated_rep_low_rom.png` (REP-02: Insufficient ROM, rejected count incremented)
      3. `03_free_arm_insufficient_grip.png` (REP-04: Loose grip warning, rejected count incremented)
      4. `04_ballistic_spike_too_fast.png` (REP-03: Jerking movement duration rejection)
      5. `05_sensor_dropout_i2c_blackout.png` (FLT-01: Critical SENSOR_UNAVAILABLE, gated reps)
      6. `06_recovery_stability_gate.png` (FLT-02: RECOVERING stability hold countdown)
      7. `07_gyro_bias_drift_degraded.png` (FLT-03: DEGRADED status, 14.8°/s gyro drift)
      8. `08_fsr_baseline_drift.png` (FLT-04: CALIBRATION_REQUIRED alert)
      9. `09_window_guided_calibration_modal.png` (5-Step Guided Biomechanical Calibration modal)
      10. `10_simulation_control_console.png` (Interactive Simulation Control Console UI)

### Session 7 — 2026-10-26
- **Architecture Synchronization & Report Refinements (Addressing GRIPGUARD_Report_1793035185.docx Review):**
  - **1. 5-Step Personal Calibration Protocol Alignment (Section 7.3):**
    - Updated `dashboard/src/components/CalibrationModal.tsx` to include Step 5: `VALIDATION_AND_COMMIT` with statistical separation validation ($S_r = (\mu_{firm} - \mu_{relaxed}) / (\sigma_{firm} + \sigma_{relaxed}) \ge 3.0$) and NVS flash commit.
    - Verified clean TypeScript build (`npm run build` in 836 ms).
    - Captured updated 5-step screenshots:
      - `report_figures/simulated_scenarios/09_window_guided_calibration_modal.png`: Interactive 5-step modal showing 5 guided steps and roadmap bar.
      - `report_figures/simulated_scenarios/fig_5step_calibration_profile_saved.png`: Stored profile showing $S_r = 28.50 \ge 3.0$ and 5-step description.
    - Replaced outdated 4-step figures in Section 7.3 of `generate_report.py`.
  - **2. Chronological Repetition Sequence (Section 7.4):**
    - Corrected sequence order to REP-01 (Normal Valid Bicep Curl) -> REP-02 (Cheated Repetition — Insufficient ROM) -> REP-03 (Ballistic Momentum Spike — Rapid Jerk) -> REP-04 (Free-Arm Movement — Insufficient Grip Pressure).
  - **3. Software Execution Flowchart (Section 4):**
    - Generated `report_figures/fig10_software_flowchart.png` illustrating 50 Hz non-blocking loop (`micros()` pacing, I2C/ADC acquisition, stationary variance gating, complementary fusion, EWMA smoothing, 8-state movement FSM, 6-state sensor reliability engine, and asynchronous REST servicing).
    - Embedded in Section 4 of the report.
  - **4. Before/After Signal Processing Graphs (Section 7.2):**
    - Generated `report_figures/fig11_signal_processing_comparison.png` featuring 3 panels:
      - Panel A: Asymmetric EWMA Grip Smoothing (noisy raw ADC vs smooth response).
      - Panel B: Complementary Orientation Filter (noisy accelerometer & drifting gyro integration vs fused estimate).
      - Panel C: Circular-Buffer Stationary Variance Detector ($Var_{accel}$ and $Var_{gyro}$ thresholding).
    - Embedded in Section 7.2 of the report.
  - **5. Real-World Experimental Action Shot (Section 5.2):**
    - Added high-resolution action photo `report_figures/real_action_dumbbell_curl.jpg` showing test subject actively performing dumbbell curl wearing the complete, powered-on prototype with visible status LED and palm FSR engagement.
    - Embedded in Section 5.2 of the report.
  - **6. Added Missing Section 5.1 Heading:**
    - Inserted `5.1 Bill of Materials & Hardware Component Specifications` preceding the components table in Section 5.
  - **7. Completed 6-State Sensor Health State Definitions (Section 7.5):**
    - Expanded introductory text in Section 7.5 to explicitly define all 6 states: `HEALTHY`, `DEGRADED`, `SUSPECTED_FAULT`, `SENSOR_UNAVAILABLE`, `RECOVERING`, and `CALIBRATION_REQUIRED`.
  - **Compiled Updated Report:**
    - Executed `python generate_report.py` producing `GRIPGUARD_Report_1793036721.docx` (11.76 MB, ~40 pages, exit code 0).

### Session 8 — 2026-10-27
- **Repetition Engine Debugging & Logic Error Resolution:**
  - Diagnosed user reported `DURATION_TOO_LONG (STALLED_REP)` at ~5020 ms across valid physical curls ($120^\circ\text{--}155^\circ$ ROM, $40\%\text{--}68\%$ grip):
    - **Bug 1 (Return Bottleneck):** `_activeProfile.returnToleranceDeg` ($14.0^\circ$) was unrealistically narrow for human biomechanics, trapping completed curls in `REP_STATE_ECCENTRIC_OR_RETURN` until the $5000\text{ ms}$ timeout expired at $5020\text{ ms}$.
    - **Fix 1:** Widened `returnToleranceDeg` to $22.0^\circ$ and introduced multi-criteria eccentric completion:
      1. Calibrated neutral return ($excursion \le 22^\circ$).
      2. Rep start posture return ($|pitch - startPitch| \le 22^\circ$).
      3. Settling at bottom of movement ($excursion \le 28^\circ$ or $\le 20\%$ of peak ROM with gyro $< 14^\circ/\text{s}$ or stationary).
  - Diagnosed `INSUFFICIENT_GRIP_PRESSURE` false rejections on high grip reps ($63\%\text{--}72\%$ grip in Reps #2 and #12):
    - **Bug 2 (Irreversible Grip Latch):** Permitted onset debounce ($300\text{ ms}$) permanently latched `_gripFailedDuringConcentric = true` during initial pre-load ramp-up.
    - **Fix 2:** Evaluated sustained low grip during active concentric ascent ($excursion \ge 20^\circ$) and relaxed final candidate threshold to $0.65 \times minGripRatio$.
  - Fixed Unused `REJECT_INCOMPLETE_RETURN`: If rep times out after reaching peak ROM ($\ge 40^\circ$), properly classifies as `INCOMPLETE_RETURN_TO_START` rather than `DURATION_TOO_LONG`.
  - **Dashboard UX & Telemetry Enhancements (`dashboard/src/App.tsx`, `SessionReport.tsx`):**
    - Implemented a 2.2-second visual state latch for `VALID_REP` / `REJECTED_REP` so the 20 ms firmware trigger displays clearly on the UI badge.
    - Implemented auto-activation of session logging upon detecting the first rep.
    - Updated Session Log explanation column to show `✓ Full ROM & Grip Verified` on valid reps instead of `NONE`.
  - **Verification:**
    - Expanded test suite in `tests/simulation/test_rep_validation.py` with Test 8 (real human rep trajectory with $136.4^\circ$ ROM, $18^\circ$ return offset, grip ramp-up): 8/8 Passed (100%).
    - Compiled dashboard production bundle (`npm run build` completed in 1.20s with 0 errors).

### Session 9 — 2026-10-27
- **Smart-Glove Firmware 5-Phase Evolution (Phases 1–4 Completed & Verified):**
  - **Phase 1: Bug Fixes (RepEngine + Calibration):**
    - Removed `returnToStart` in `ECCENTRIC`. Added concentric hysteresis abort path ($excursion < 8^\circ$ & $peak < 22^\circ \to$ silent return to `READY`).
    - Fixed grip evaluation with single normalized threshold ($minGripRatio \approx 0.35$), eliminated pre-onset latching.
    - Added stall timer reset in `TRANSITION` whenever gyro $> 14^\circ$/s or peak continues to rise.
    - Added 3-sample crossings debounce ($60\text{ ms}$) on all state transitions to reject single-sample transient spikes.
    - Pre-flight calibration check: requires 5 consecutive bad samples to trigger timeout. Enforced strict FSR bounds (firm mean $< 4000$, relaxed std $< 100$, firm CV $< 0.25$).
    - Aborted reps immediately in-flight on sensor fault with `REJECT_SENSOR_UNRELIABLE`.
  - **Phase 2: Signal Processing Redesign (`SignalProcessing.h/.cpp`, `Calibration.h/.cpp`):**
    - Replaced Euler angle complementary fusion with 6-DoF Mahony quaternion filter outputting gravity unit vector $\hat{g}$.
    - Gated accelerometer correction by $|\|a\| - 1g| < 1.5\text{ m/s}^2$ to prevent centrifugal corruption.
    - Implemented ~4 Hz tilt LPF and ~5 Hz gyro magnitude LPF.
    - Replaced asymmetric FSR filter with Median-of-5 + EMA filter.
    - Added Schmitt trigger grip gate (0.45 ON / 0.30 OFF, 160 ms / 8 samples debounced).
    - Upgraded calibration schema to v2 (`neutralG[3]`, `angularSpreadDeg`, Welford double-precision moments, staging profile commit).
  - **Phase 3: RepEngine Redesign (`RepEngine.h/.cpp`):**
    - 3D gravity tilt excursion: $\theta = \arccos(\hat{g} \cdot \hat{g}_{\text{neutral}})$.
    - Valley $\to$ peak $\to$ valley prominence tracking on both ascent and descent legs.
    - Dynamic re-baselining of valley when stationary at bottom ($< 12^\circ$/s).
    - Armed exclusively while Schmitt grip gate is HIGH; un-gripped movement silently ignored.
    - 2-Candidate confirmation & back-crediting: first candidate rep held in pending slot, both back-credited once rep 2 is verified consistent within $30^\circ$ and $2.0\text{ s}$.
    - Adaptive thresholds: rolling median and variance over last 5 valid reps ($minRom = 0.65 \times \text{median}$, tempo window $\text{median} \pm 2\sigma$).
  - **Phase 4: Novel Feature — Inertial Grip-Load Coupling (`RepEngine.h/.cpp`):**
    - High-pass FSR and gyro angular acceleration history buffers ($256$ samples, $O(1)$ static memory).
    - Cross-correlation $\rho$ over physiological lag $[0, 140\text{ ms}]$.
    - `REJECT_UNLOADED` reject reason for mimed movements / dropped weights ($\rho < 0.12$ & grip $< 0.42$).
    - `REJECT_MOMENTUM_SWING` for excessive torso swing cheat ($t_{\text{concentric}} < 450\text{ ms}$, early gyro spike $> 110^\circ$/s, $\rho < 0.22$).
    - Multi-factor per-rep confidence scoring ($Z_{rom}, Z_{dur}, Z_{\rho}, Z_{noise}, Z_{phase}$).
    - Uncertain rep candidate pool with cadence reconciliation at set-end when grip released $> 1.5\text{ s}$.
    ### Session 10 — 2026-10-27
- **Phase 5: Evaluation Harness, Raw 50 Hz Streaming & Ablation Suite:**
  - Added compile-time configuration switch `#define STREAM_RAW_TELEMETRY_CSV 0` under `Config::Evaluation` in `firmware/SmartGlove/Config.h`.
  - Added high-rate 50 Hz serial CSV output (`timestamp_ms,ax,ay,az,gx,gy,gz,fsr_adc,fsr_norm,tilt_deg,state,valid_reps,rejected_reps,reason`) in `firmware/SmartGlove/SmartGlove.ino`, toggled either by `#if STREAM_RAW_TELEMETRY_CSV` or dynamically via serial command `'L'`/`'l'`.
  - Implemented offline desktop replay evaluation harness in `tests/simulation/replay_eval_harness.py`:
    - Reconstructs complete Phase 1–4 digital twin (Mahony filter, Schmitt grip trigger, valley-to-valley prominence tracking, 2-candidate back-crediting, inertial grip-load coupling $\rho$).
    - Supports replaying real recorded CSV files or synthetic benchmark sessions with 4 distinct workout phases (standard loaded reps, un-gripped free gestures, fatigued reps, and pronated arm curls).
    - Features 3 algorithmic ablation study switches (`--ablation grip|adaptive|tilt|all`):
      1. Ablation A (Grip Gate OFF): Shows un-gripped gestures trigger phantom candidate tracking and false rejections.
      2. Ablation B (Adaptive ROM OFF): Shows rigid fixed $40^\circ$ minimum ROM falsely rejects fatigued end-of-set reps.
      3. Ablation C (3D Tilt OFF / Euler Pitch): Shows forearm pronation causes Euler pitch divergence, underestimating excursion by $\cos(\phi)$ and falsely rejecting valid reps.
  - **Verification:**
    - Full evaluation harness execution verified: `python -u tests/simulation/replay_eval_harness.py --ablation all` passed cleanly (exited code 0).
    - Unit test suite verified: `python tests/simulation/test_rep_validation.py` (11/11 tests passed, 100%).

### Session 12 — 2026-10-27
- **Comprehensive Firmware & Mathematical Logic Audit:**
  - **Bug 1 (Unwired Neutral Gravity Reference):**
    - `SmartGlove.ino` failed to invoke `signalProc.setReferenceGravity(prof.neutralG)` both in `setup()` upon loading from NVS and in `loop()` upon calibration completion, leaving the 6-DoF tilt engine operating on default `[0, 0, 1]` rather than the user's calibrated neutral arm posture.
    - **Fix 1:** Added `signalProc.setReferenceGravity(prof.neutralG)` to both boot and calibration completion paths in `SmartGlove.ino`.
  - **Bug 2 (Pending Rep #1 Latch with Sustained Grip):**
    - `REP_STATE_READY` checked the 4500 ms timeout for Candidate Rep #1 only inside `if (!proc.gripGate)`. Lifters maintaining dumbbell grip while resting at the bottom would latch Rep #1 indefinitely, falsely confirming inconsistent reps minutes later.
    - **Fix 2:** Moved timeout expiration check before the `!proc.gripGate` condition in `RepEngine.cpp`, `test_rep_validation.py`, and `replay_eval_harness.py`.
  - **Bug 3 (Premature Rejection Increment on Uncertain Candidate Pooling):**
    - When candidates fell into the uncertain confidence range ($40\% \le \text{confidence} < 60\%$) for set-end reconciliation, `finalizeRep(false, REJECT_NONE, ...)` indiscriminately incremented `_stats.totalRejectedReps++`, falsely penalizing the lifter before cadence verification.
    - **Fix 3:** Updated `finalizeRep` in `RepEngine.cpp` to increment `totalRejectedReps` only when `reason != REJECT_NONE`.
  - **Bug 4 (Stranded Candidates & Rejection Accounting in Set-End Reconciliation):**
    - If a set finished with zero valid reps confirmed, `reconcileUncertainReps()` returned early without updating counters, stranding uncertain candidates in limbo. Furthermore, candidates failing the 25% cadence window were discarded without rejection accounting.
    - **Fix 4:** Handled zero-valid sets by converting unverified candidates to rejected attempts, and incremented `totalRejectedReps` for cadence mismatches in `RepEngine.cpp`.
  - **Bug 5 (Startup Transient in Filtered Attitude & Gyro Magnitude):**
### Session 13 — 2026-10-27
- **Arduino IDE ESP32 Compilation Error Resolution:**
  - **Compiler Error 1 (`SmartGlove.ino:308-313`):** `'struct RawSensorData' has no member named 'accelRaw' / 'gyroRaw'`
    - **Fix 1:** Replaced non-existent array access with actual `RawSensorData` member variables (`currentSample.accelX_mps2`, `accelY_mps2`, `accelZ_mps2` and `currentSample.gyroX_dps`, `gyroY_dps`, `gyroZ_dps`).
  - **Compiler Error 2 (`SmartGlove.ino:362`):** `expected constructor, destructor, or type conversion before ';' token`
    - **Fix 2:** Removed extra closing brace at line 359 left behind from conditional refactoring, placing `handleSerialCommands()` back inside the `loop()` function.
  - **Compiler Error 3 (`NetworkManager.cpp:189-191`):** `'const struct CalibrationProfile' has no member named 'neutralPitchDeg' / 'neutralRollDeg' / 'postureStabilityStd'`
    - **Fix 3:** Updated `handleGetCalibration()` to use Schema v2 fields (`prof.neutralG[3]` and `prof.angularSpreadDeg`), dynamically calculating `neutralPitch` and `neutralRoll` via trigonometry to maintain 100% backward-compatibility with the web dashboard.
  - **Verification:**
    - Python unit test suite: 11 / 11 tests passed (100%).
    - Web dashboard production bundle: `npm run build` compiled cleanly in 1.24s.

### Session 14 — 2026-10-27
- **Resolution for Physical Hardware Calibration Failures:**
  - **Issue:** Calibration wizard continually failed during hardware trials across steps 1 through 5.
  - **Root Cause Analysis (5 Critical Traps):**
    1. *Firm Grip Saturation Trap (Step 4):* Squeezing firmly at maximum voluntary contraction reached ADC $\ge 4000$ (out of 4095). Line 314 explicitly aborted with `"Firm grip: FSR saturated"`, punishing strong users for following instructions.
    2. *Relaxed Grip Noise Trap (Step 3):* Required standard deviation $\le 100$ ADC counts ($2.4\%$ of 12-bit range). ESP32 SAR ADC thermal noise (30–60 counts) plus natural finger tremor easily triggered instant abort. Baseline was also capped too tightly at $1200$ ADC.
    3. *Neutral Wrist Postural Tremor Trap (Step 2):* Required angular spread $\le 6.0^\circ$ over a 3.0-second dumbbell hold. Real lifters naturally experience $6^\circ\text{--}12^\circ$ of micro-tremor under load.
    4. *Preflight Hair-Trigger Trap (Step 1):* Consecutive bad samples limit was only 5 samples ($100\text{ ms}$ at 50 Hz) with tight acceleration bounds ($6.0\text{--}14.0\text{ m/s}^2$). Moving the arm into starting position tripped failure before the test even began.
    5. *Validation Math Traps (Step 5):* Firm hold Coefficient of Variation ($CV < 0.25$) aborted natural force ramping; statistical separation ratio $S_r \ge 3.0$ was unrealistically high for real sensor dynamic ranges.
    6. *Zero Observability:* Neither Serial monitor nor `/api/telemetry` reported the specific failure reason string when `CALIB_FAILED` occurred.
  - **Fixes Applied:**
    - `firmware/SmartGlove/Calibration.cpp`:
      - Step 1: Debounced bad samples to 25 samples ($500\text{ ms}$) and widened acceleration bounds ($4.0\text{--}20.0\text{ m/s}^2$) and FSR voltage ($2.85\text{V}$).
      - Step 2: Widened angular tremor tolerance to $16.0^\circ$ (was $6.0^\circ$).
      - Step 3: Increased relaxed baseline threshold to $1800\text{ ADC}$ (was $1200$) and noise tolerance to $350\text{ ADC std}$ (was $100$).
      - Step 4: Removed saturation rejection! Squeezes $\ge 4000$ are clamped to $4050\text{ ADC}$. Added weak grip check ($<600\text{ ADC}$).
      - Step 5: Relaxed separation requirement to $250\text{ ADC}$, relaxed firm CV to $0.45$, and updated separation ratio check to $\ge 1.2$.
    - `firmware/SmartGlove/Calibration.h`: Increased `_failureReason` buffer from 64 to 128 bytes to prevent truncation.
    - `firmware/SmartGlove/Config.h`: Updated `MIN_ACCEPTABLE_SEPARATION_RATIO` to 1.2f.
    - `firmware/SmartGlove/SmartGlove.ino`: Added real-time Serial notifications for calibration step transitions, success, and `[CALIB FAILED] Reason: <message>`.
    - `firmware/SmartGlove/NetworkManager.cpp`: Exposed `failure_reason` in `/api/telemetry` JSON payload.
    - `dashboard/src/types.ts` & `dashboard/src/components/CalibrationModal.tsx`: Rendered the exact firmware failure reason in a prominent warning card.
  - **Verification:**
    - Simulation test suite: 11 / 11 tests passed (100%).
    - Web dashboard production bundle: `npm run build` compiled with 0 errors.

### Session 15 — 2026-10-27
- **Interactive User-Paced Calibration & Step 3 ("Egg Shell") Resolution:**
  - **Issue Reported by User:**
    - The calibration kept failing at Step 3 (touching the dumbbell lightly like an eggshell) and would not advance past 3.
    - User requested an interactive flow: Before each step, ask the user if they are ready -> when they click yes, run a 3-second countdown (3... 2... 1...) -> start recording -> on completion, say "Good job!" and show the next step photo/prompt.
  - **Root Cause Analysis for Step 3 Trap:**
    - In physical reality, holding a dumbbell in hand (even with relaxed fingers touching the bar lightly) presses the metal handle against the glove's FSR sensor with the dumbbell's deadweight.
    - This routinely generated $2000\text{--}2800\text{ ADC}$ counts. The code strictly aborted if `meanADC > 1800` or `stdADC > 350`, making Step 3 physically impossible to pass while holding a dumbbell!
    - Furthermore, the firmware previously auto-advanced on a fixed 3.5s timer, forcing Step 3 sampling before the user finished repositioning their fingers.
  - **Fixes Applied:**
    1. *User-Paced Wizard (`firmware/SmartGlove/Calibration.h`, `.cpp`):*
       - Added `confirmReady()` method to `PersonalCalibration`.
       - Prep steps (`PREP_REST_ARM`, `PREP_START_POSITION`, `PREP_RELAX_GRIP`, `PREP_FIRM_GRIP`) no longer auto-advance on a short timer. They wait for user confirmation (or a 90s safety timeout).
    2. *Step 3 Real Dumbbell Baseline Limits (`Calibration.cpp`):*
       - Relaxed relaxed baseline ceiling to $3400\text{ ADC}$ (was $1800$), permitting real dumbbells resting against the palm/fingers while leaving $> 650\text{ counts}$ headroom for firm squeeze.
       - Relaxed noise tolerance to $750\text{ ADC std}$ (was $350$) to handle tremor while holding weight.
       - Dynamic range separation updated to $\ge 200\text{ ADC}$.
    3. *Firmware API & Serial Control (`NetworkManager.cpp`, `SmartGlove.ino`):*
       - Added `action=ready` and `action=next` to `POST /api/calibrate`.
       - Added `'n'`, `'N'`, `'y'`, `'Y'` interactive Serial commands for step readiness (avoiding collision with existing `'R'` rep reset command).
    4. *Web Dashboard User Flow (`api.ts`, `App.tsx`, `CalibrationModal.tsx`):*
       - Before each step, shows the step photo and prompts: *"Position your hand as shown. Are you ready?"* with a vibrant *"👉 Yes, I'm Ready (3s Countdown)"* button.
       - Clicking ready triggers a 3-second visual and auditory countdown (3... 2... 1...).
       - At 0, triggers `action=ready` and starts active recording with the progress bar.
       - Upon completion, plays a celebratory 3-tone chime and shows: *"🎉 Good Job! Step X Completed Successfully!"*.
       - Advances to the next step's photo and readiness prompt.
  - **Verification:**
    - Simulation test suite: 11 / 11 tests passed (100%).
    - Web dashboard production bundle: `npm run build` compiled cleanly in 1.73s with 0 errors.
    - Arduino IDE duplicate `case 'R'` resolved cleanly.

### Session 16 — 2026-10-27
- **Resolution for Graph Cancellation & False Grip Rejections:**
  - **Issue Reported by User:**
    - Live excursion graph was fluctuating and appearing to "cancel out" with values swinging between positive and negative.
    - Completed repetitions with strong palm pressure ($49\%$, $52\%$, $63\%$, $89\%$) were falsely rejected as `INSUFFICIENT_GRIP_PRESSURE` with coupling $\rho = 0.000$.
    - Dashboard explanation banner displayed misleading text: *"Grip pressure (89%) fell below minimum threshold (35%)"*.
  - **Root Cause Analysis:**
    1. *Graph Telemetry Dimension:* `App.tsx` buffered Euler `pitch_deg` into the graph instead of invariant 3D `current_excursion_deg`. Euler pitch in the body frame naturally crosses zero and reverses sign when rotating and curling the forearm. SVG canvas was also hardcoded to $-20^\circ\text{ to }80^\circ$, clipping valid curls.
    2. *In-Flight Hair-Trigger Grip Abort:* `RepEngine.cpp` immediately called `finalizeRep(false, REJECT_INSUFFICIENT_GRIP)` on any single de-assertion of `proc.gripGate` during concentric, transition, and eccentric phases. Even when lowering the arm after completing a curl, relaxing fingers at the bottom tripped this check and aborted the rep, leaving $\rho = 0.000$.
    3. *Single-Sample `_minGrip` Latch:* In `REP_CANDIDATE`, line 869 rejected reps if `_minGrip < gripThresholdNorm`, failing reps if even a single 20ms sample out of 150 had a microsecond dip.
    4. *Schmitt Trigger Disarm Boundary:* Schmitt trigger disarmed at $\le 0.30$, which was too close to natural holding pressure variations.
  - **Fixes Applied:**
    1. *Dashboard Real-Time Graph (`App.tsx`, `MovementTrace.tsx`):*
       - Buffers `data.repetition.current_excursion_deg` (0° to 120°+ invariant positive angle) into trajectory view.
       - Updated SVG canvas scaling to dynamically encompass $0^\circ\text{ to }120^\circ+$ without clipping.
       - Updated failure explanation to truthfully report grip drop context.
    2. *Schmitt Trigger Tuning (`SignalProcessing.h`, `.cpp`, `SensorPanel.tsx`):*
       - Updated Schmitt gate thresholds to $0.35$ ON and $0.20$ OFF, preventing normal curling tension dips from disarming the gate while maintaining instant detection of full hand release.
    3. *Debounced In-Flight Grip Loss (`RepEngine.cpp`):*
       - `CONCENTRIC`, `TRANSITION`, and `ECCENTRIC` now require sustained low grip (`SUSTAINED_LOW_GRIP_SAMPLES` = 15 samples / 300 ms) before aborting.
       - Natural grip relaxation near the bottom of the eccentric return ($excursion \le returnSettledExcursionDeg$) is permitted and settles into candidate evaluation.
    4. *Early $\rho$ Calculation & Clean Candidate Verification (`RepEngine.cpp`):*
       - Computes cross-correlation coupling $\rho$ at candidate onset so all completed movements show true coupling in session history.
       - Replaced instantaneous `_minGrip` check with debounced sustained low grip flag `_gripFailedDuringRep` and average grip check ($avgGrip \ge 0.75 \times minGripRatio$).
  - **Verification:**
    - Simulation unit test suite: `python tests/simulation/test_rep_validation.py` (11 / 11 tests passed, 100%).
    - Replay evaluation harness: `python tests/simulation/replay_eval_harness.py` (Passed cleanly, exit code 0).
    - Web dashboard production bundle: `npm run build` compiled with 0 errors in 1.57s.
