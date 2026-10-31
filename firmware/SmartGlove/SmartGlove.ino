/**
 * ============================================================================
 * GRIPGUARD — SMART WEIGHTLIFTING GLOVE
 * ============================================================================
 * Milestone 6: Asynchronous Network Manager & Versioned Telemetry API
 * 
 * Hardware Connections:
 * - MPU6050 VCC -> ESP32 3V3
 * - MPU6050 GND -> ESP32 GND
 * - MPU6050 SDA -> ESP32 GPIO 21
 * - MPU6050 SCL -> ESP32 GPIO 22
 * - FSR Terminal 1 -> ESP32 3V3
 * - FSR Terminal 2 -> ESP32 GPIO 34 (ADC1_CH6)
 * - 10 kOhm Resistor: GPIO 34 to GND
 * 
 * Target Baud: 115200
 */

#include "Config.h"
#include "SensorAcquisition.h"
#include "SignalProcessing.h"
#include "Calibration.h"
#include "RepEngine.h"
#include "FaultDetection.h"
#include "NetworkManager.h"

// Subsystem Instances
SensorAcquisition sensorAcq;
SignalProcessing signalProc;
PersonalCalibration personalCal;
RepetitionEngine repEngine;
FaultDetectionEngine faultEngine;
GripNetworkManager netManager;

RawSensorData currentSample;
ProcessedSensorData processedSample;

// Non-blocking Timing Control
uint32_t lastAcquisitionTimeUs = 0;
uint32_t lastSerialReportTimeMs = 0;
CalibrationStep lastObservedCalStep = CALIB_IDLE;
RepState lastObservedRepState = REP_STATE_IDLE;
SensorHealthState lastObservedHealth = HEALTH_STATE_HEALTHY;
bool csvLoggingActive = false;

void printSystemBanner() {
    Serial.println();
    Serial.println(F("================================================================"));
    Serial.println(F("  GRIPGUARD — SMART WEIGHTLIFTING GLOVE (Milestone 6 Mode)     "));
    Serial.println(F("  Asynchronous Dual-WiFi & Versioned JSON Telemetry REST API    "));
    Serial.println(F("================================================================"));
    Serial.println(F("Subsystems Active:"));
    Serial.println(F("  [Acquisition] 50 Hz Hardware Sampling (MPU6050 I2C + FSR ADC) "));
    Serial.println(F("  [Processing]  Stationary Detector + Zero-Rate Gyro Bias FSM   "));
    Serial.println(F("  [Orientation] Drift-Aware Complementary Filter (Pitch & Roll) "));
    Serial.println(F("  [Calibration] 5-Step Guided Wizard + NVS Preferences + CRC32  "));
    Serial.println(F("  [Rep Engine]  8-State FSM, ROM/Duration Bounds, Grip Gating   "));
    Serial.println(F("  [Reliability] 6-State Health Gate, Dropout/Drift/Stuck-At Det "));
    Serial.println(F("  [Network API] Asynchronous REST Server & Dual Wi-Fi AP/STA    "));
    Serial.println(F("Type '?' in Serial Monitor for interactive commands."));
    Serial.println(F("================================================================"));
}

void printNetworkStatus() {
    Serial.println(F("\n--- Wi-Fi & REST API Status ---"));
    Serial.print(F("  Access Point SSID: ")); Serial.println(F(WIFI_AP_SSID));
    Serial.print(F("  Access Point IP:   http://")); Serial.println(netManager.getAccessPointIP());
    if (netManager.isConnectedSTA()) {
        Serial.print(F("  Station Mode:      CONNECTED (http://"));
        Serial.print(netManager.getStationIP());
        Serial.println(F(")"));
    } else {
        Serial.println(F("  Station Mode:      NOT CONNECTED (Standalone AP active)"));
    }
    Serial.println(F("  Available Endpoints:"));
    Serial.println(F("    GET  /api/telemetry    - Live synchronized sensor telemetry"));
    Serial.println(F("    GET  /api/calibration  - Current calibration profile"));
    Serial.println(F("    POST /api/calibrate    - Trigger calibration action (start/cancel/reset)"));
    Serial.println(F("    GET  /api/diagnostics  - Sensor reliability & health report"));
    Serial.println(F("    POST /api/exercise     - Switch profile (bicep_curl, etc.)"));
    Serial.println(F("    POST /api/reset_reps   - Reset rep counts to 0\n"));
}

void handleSerialCommands() {
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        switch (cmd) {
            case '?':
            case 'h':
                Serial.println(F("\n--- GRIPGUARD Interactive Commands ---"));
                Serial.println(F("  h / ? : Display this help menu"));
                Serial.println(F("  w     : Print Wi-Fi & REST API network status"));
                Serial.println(F("  f     : Print sensor reliability diagnostics report"));
                Serial.println(F("  m     : Display repetition session metrics"));
                Serial.println(F("  R     : Reset repetition counters to 0"));
                Serial.println(F("  1     : Select Bicep Curl exercise profile"));
                Serial.println(F("  2     : Select Wrist Extension exercise profile"));
                Serial.println(F("  3     : Select Dumbbell Press exercise profile"));
                Serial.println(F("  c     : Start 5-step guided calibration wizard"));
                Serial.println(F("  n / y : Confirm ready for current calibration step"));
                Serial.println(F("  p     : Display current calibration profile"));
                Serial.println(F("  i     : Inject simulated I2C Dropout fault (Test)"));
                Serial.println(F("  k     : Clear injected simulated faults"));
                Serial.println(F("  s     : Run manual I2C bus scan"));
                Serial.println(F("  l     : Toggle lightweight CSV telemetry streaming"));
                Serial.println(F("--------------------------------------\n"));
                break;
            case 'w':
                printNetworkStatus();
                break;
            case 'f': {
                const DiagnosticReport& rep = faultEngine.getReport();
                Serial.println(F("\n--- Sensor Health Status ---"));
                Serial.print(F("  State: ")); Serial.println(faultEngine.getHealthStateName(rep.overallHealth));
                Serial.print(F("  Fault: ")); Serial.println(faultEngine.getFaultTypeName(rep.primaryFault));
                Serial.print(F("  Confidence: ")); Serial.print(rep.assessmentConfidencePct, 1); Serial.println(F("%\n"));
                break;
            }
            case 'i':
                Serial.println(F("[FAULT] Injecting simulated MPU6050 I2C dropout fault..."));
                faultEngine.injectFault(FAULT_DROPOUT_MPU_I2C);
                break;
            case 'k':
                Serial.println(F("[FAULT] Injected faults cleared."));
                faultEngine.clearInjectedFaults();
                break;
            case 'm': {
                const RepetitionStats& stats = repEngine.getStats();
                Serial.println(F("\n--- Repetition Metrics ---"));
                Serial.print(F("  Valid Reps: ")); Serial.println(stats.totalValidReps);
                Serial.print(F("  Rejected Attempts: ")); Serial.println(stats.totalRejectedReps);
                Serial.print(F("  Acceptance Ratio: ")); Serial.print(stats.acceptanceRatioPct, 1); Serial.println(F("%\n"));
                break;
            }
            case 'R':
                repEngine.resetCounters();
                Serial.println(F("[REP] Repetition counters reset."));
                break;
            case '1':
                repEngine.setExerciseProfile(EXERCISE_BICEP_CURL);
                Serial.println(F("[REP] Profile set to: BICEP CURL"));
                break;
            case '2':
                repEngine.setExerciseProfile(EXERCISE_WRIST_EXTENSION);
                Serial.println(F("[REP] Profile set to: WRIST EXTENSION"));
                break;
            case '3':
                repEngine.setExerciseProfile(EXERCISE_DUMBBELL_PRESS);
                Serial.println(F("[REP] Profile set to: DUMBBELL PRESS"));
                break;
            case 'c':
                Serial.println(F("\n[CALIB] Starting 5-Step Guided Calibration Wizard..."));
                personalCal.startWizard();
                break;
            case 'n':
            case 'N':
            case 'y':
            case 'Y':
                Serial.println(F("[CALIB] Ready confirmed. Beginning step recording..."));
                personalCal.confirmReady();
                break;
            case 'p': {
                const CalibrationProfile& prof = personalCal.getProfile();
                Serial.println(F("\n--- Active Calibration Profile ---"));
                Serial.print(F("  Status: ")); Serial.println(prof.isValid ? F("CALIBRATED") : F("DEFAULT FALLBACK"));
                Serial.print(F("  Separation Ratio: ")); Serial.println(prof.gripSeparationRatio, 2);
                Serial.println();
                break;
            }
            case 's': {
                uint8_t foundAddr = sensorAcq.scanI2C();
                if (foundAddr != 0) {
                    Serial.print(F("[CMD] MPU6050 at: 0x")); Serial.println(foundAddr, HEX);
                } else {
                    Serial.println(F("[ERROR] MPU6050 not found on I2C bus."));
                }
                break;
            }
            case 'l':
            case 'L':
                csvLoggingActive = !csvLoggingActive;
                if (csvLoggingActive) {
                    Serial.println(F("\n--- RAW 50 Hz CSV TELEMETRY ACTIVE ---"));
                    Serial.println(F("timestamp_ms,ax,ay,az,gx,gy,gz,fsr_adc,fsr_norm,tilt_deg,state,valid_reps,rejected_reps,reason"));
                } else {
                    Serial.println(F("[LOG] 50 Hz CSV streaming disabled. Restored JSON streaming."));
                }
                break;
            default:
                break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2500) {
        ; // Brief delay for native USB CDC
    }

    printSystemBanner();

    Serial.print(F("[INIT] Initializing Hardware Acquisition... "));
    if (sensorAcq.init()) {
        Serial.println(F("SUCCESS."));
    } else {
        Serial.println(F("FAILED / DEGRADED."));
    }

    Serial.print(F("[INIT] Initializing Signal Processing engine... "));
    signalProc.init();
    Serial.println(F("READY."));

    Serial.print(F("[INIT] Loading Calibration Profile from NVS... "));
    if (personalCal.init()) {
        Serial.println(F("LOADED VALID PROFILE."));
        const CalibrationProfile& prof = personalCal.getProfile();
        signalProc.setGripCalibration(prof.fsrRelaxedMeanADC, prof.fsrFirmMeanADC);
        signalProc.setReferenceGravity(prof.neutralG);
    } else {
        Serial.println(F("NO STORED PROFILE (Using Safe Defaults)."));
    }

    Serial.print(F("[INIT] Initializing Repetition Engine... "));
    repEngine.init();
    Serial.println(F("READY."));

    Serial.print(F("[INIT] Initializing Fault Detection Engine... "));
    faultEngine.init();
    Serial.println(F("READY."));

    Serial.print(F("[INIT] Starting Network Manager & REST API Server... "));
    netManager.init(&personalCal, &repEngine, &faultEngine);
    Serial.println(F("ONLINE."));

    printNetworkStatus();

#if STREAM_RAW_TELEMETRY_CSV
    csvLoggingActive = true;
    Serial.println(F("\n--- RAW 50 Hz CSV TELEMETRY ACTIVE (COMPILE-TIME) ---"));
    Serial.println(F("timestamp_ms,ax,ay,az,gx,gy,gz,fsr_adc,fsr_norm,tilt_deg,state,valid_reps,rejected_reps,reason"));
#endif

    lastAcquisitionTimeUs = micros();
    lastSerialReportTimeMs = millis();
}

void loop() {
    uint32_t currentMicros = micros();

    // 1. Non-blocking 50 Hz Hardware Acquisition (every 20,000 microseconds)
    if (currentMicros - lastAcquisitionTimeUs >= Config::Timing::SAMPLE_INTERVAL_US) {
        lastAcquisitionTimeUs += Config::Timing::SAMPLE_INTERVAL_US;

        if (currentMicros - lastAcquisitionTimeUs > Config::Timing::SAMPLE_INTERVAL_US * 2) {
            lastAcquisitionTimeUs = currentMicros;
        }

        // Step A: Acquire raw physical data
        sensorAcq.readSensors(currentSample);

        // Step B: Process signal (stationary detection, bias update, complementary filter)
        signalProc.process(currentSample, processedSample);

        // Step C: Sensor Reliability & Fault Detection Evaluation
        faultEngine.update(currentSample, processedSample, sensorAcq.getHealth(), personalCal.getProfile());

        // Check if health state transitioned
        SensorHealthState currentHealth = faultEngine.getOverallHealth();
        if (currentHealth != lastObservedHealth) {
            lastObservedHealth = currentHealth;
            Serial.print(F("[HEALTH] "));
            Serial.print(faultEngine.getHealthStateName(currentHealth));
            Serial.print(F(" | Gated: "));
            Serial.println(faultEngine.isRepCountingPermitted() ? F("NO") : F("YES"));
        }

        // Step D: Feed into Personal Calibration Wizard
        personalCal.processSample(currentSample, processedSample);

        CalibrationStep currentCalStep = personalCal.getActiveStep();
        if (currentCalStep != lastObservedCalStep) {
            if (currentCalStep == CALIB_COMPLETED) {
                const CalibrationProfile& prof = personalCal.getProfile();
                signalProc.setGripCalibration(prof.fsrRelaxedMeanADC, prof.fsrFirmMeanADC);
                signalProc.setReferenceGravity(prof.neutralG);
                Serial.println(F("\n>>> [CALIB SUCCESS] Valid personal profile validated and committed to NVS! <<<"));
            } else if (currentCalStep == CALIB_FAILED) {
                Serial.print(F("\n>>> [CALIB FAILED] Reason: "));
                Serial.print(personalCal.getFailureReason());
                Serial.println(F(" <<<"));
            } else if (currentCalStep != CALIB_IDLE) {
                Serial.print(F("[CALIB STEP] "));
                Serial.println(personalCal.getStepName());
            }
            lastObservedCalStep = currentCalStep;
        }

        // Step E: Feed into Repetition Engine (gated by sensor reliability)
        if (personalCal.getActiveStep() == CALIB_IDLE || personalCal.getActiveStep() == CALIB_COMPLETED) {
            repEngine.update(processedSample, personalCal.getProfile(), faultEngine.isRepCountingPermitted());
        }

        // Rep event notifications
        RepState currentRepState = repEngine.getState();
        if (currentRepState != lastObservedRepState) {
            lastObservedRepState = currentRepState;
            if (!csvLoggingActive && !STREAM_RAW_TELEMETRY_CSV) {
                if (currentRepState == REP_STATE_VALID_REP) {
                    const RepetitionStats& stats = repEngine.getStats();
                    Serial.print(F(">>> VALID REP COUNTED! Total: "));
                    Serial.print(stats.totalValidReps);
                    Serial.print(F(" (ROM: ")); Serial.print(stats.lastRepPeakRomDeg, 1);
                    Serial.println(F(" deg) <<<"));
                } else if (currentRepState == REP_STATE_REJECTED_REP) {
                    const RepetitionStats& stats = repEngine.getStats();
                    Serial.print(F("[REJECT] Reason: "));
                    Serial.println(repEngine.getRejectionReasonName(stats.lastRejectionReason));
                }
            }
        }

        // Phase 5: Raw 50 Hz CSV Telemetry Streaming
        if (csvLoggingActive || STREAM_RAW_TELEMETRY_CSV) {
            const RepetitionStats& repStats = repEngine.getStats();
            Serial.print(currentSample.timestampMs); Serial.print(',');
            Serial.print(currentSample.accelX_mps2, 3); Serial.print(',');
            Serial.print(currentSample.accelY_mps2, 3); Serial.print(',');
            Serial.print(currentSample.accelZ_mps2, 3); Serial.print(',');
            Serial.print(currentSample.gyroX_dps, 2); Serial.print(',');
            Serial.print(currentSample.gyroY_dps, 2); Serial.print(',');
            Serial.print(currentSample.gyroZ_dps, 2); Serial.print(',');
            Serial.print(currentSample.fsrRawADC); Serial.print(',');
            Serial.print(processedSample.normalizedGrip, 3); Serial.print(',');
            Serial.print(repStats.currentExcursionDeg, 2); Serial.print(',');
            Serial.print(repEngine.getStateName()); Serial.print(',');
            Serial.print(repStats.totalValidReps); Serial.print(',');
            Serial.print(repStats.totalRejectedReps); Serial.print(',');
            Serial.println(repEngine.getRejectionReasonName(repStats.lastRejectionReason));
        }
    }

    // 2. Non-blocking Asynchronous Network Servicing
    // Decoupled from 50 Hz timer: handles incoming browser REST queries with zero loop starvation
    netManager.update(currentSample, processedSample);

    // 3. Periodic Serial Telemetry Streaming (10 Hz = every 100 ms)
    uint32_t currentMillis = millis();
    if (!csvLoggingActive && !STREAM_RAW_TELEMETRY_CSV && (currentMillis - lastSerialReportTimeMs >= Config::Timing::SERIAL_DEBUG_INTERVAL_MS)) {
        lastSerialReportTimeMs = currentMillis;

        const DiagnosticReport& diag = faultEngine.getReport();
        const RepetitionStats& repStats = repEngine.getStats();

        // Output structured, clean JSON line
        Serial.print(F("{\"seq\":"));
        Serial.print(currentSample.sequenceId);
        Serial.print(F(",\"t_ms\":"));
        Serial.print(currentSample.timestampMs);
        Serial.print(F(",\"pitch\":"));
        Serial.print(processedSample.pitchDeg, 1);
        Serial.print(F(",\"roll\":"));
        Serial.print(processedSample.rollDeg, 1);
        Serial.print(F(",\"grip_pct\":"));
        Serial.print((int)(processedSample.normalizedGrip * 100.0f));
        Serial.print(F(",\"rep_state\":\""));
        Serial.print(repEngine.getStateName());
        Serial.print(F("\",\"reps_valid\":"));
        Serial.print(repStats.totalValidReps);
        Serial.print(F(",\"reps_rej\":"));
        Serial.print(repStats.totalRejectedReps);
        Serial.print(F(",\"health\":\""));
        Serial.print(faultEngine.getHealthStateName(diag.overallHealth));
        Serial.print(F("\",\"conf_pct\":"));
        Serial.print((int)diag.assessmentConfidencePct);
        Serial.println(F("}"));
    }

    // 4. Process Serial Interactive Commands
    handleSerialCommands();
}
