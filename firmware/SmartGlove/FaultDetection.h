#ifndef FAULT_DETECTION_H
#define FAULT_DETECTION_H

#include <Arduino.h>
#include "Config.h"
#include "SensorAcquisition.h"
#include "SignalProcessing.h"
#include "Calibration.h"

/**
 * 6-State Sensor Health & Reliability Model
 */
enum SensorHealthState {
    HEALTH_STATE_HEALTHY = 0,             // All sensors within nominal tolerances
    HEALTH_STATE_DEGRADED,                // Minor drift or elevated noise; qualified assessment
    HEALTH_STATE_SUSPECTED_FAULT,         // Anomaly detected; rep counting paused
    HEALTH_STATE_SENSOR_UNAVAILABLE,      // Physical disconnect, bus blackout, or hardware lockup
    HEALTH_STATE_RECOVERING,              // Signal restored; awaiting 1500 ms stability gate
    HEALTH_STATE_CALIBRATION_REQUIRED     // Baseline shifted beyond recovery bounds
};

/**
 * Specific Sensor Fault Classifications
 */
enum FaultType {
    FAULT_NONE = 0,
    FAULT_DROPOUT_MPU_I2C,               // I2C communication timeout / consecutive NACKs
    FAULT_DROPOUT_FSR_ADC,               // ADC pin saturation or open-circuit rail
    FAULT_DRIFT_GYRO_BIAS,               // Zero-rate bias drift exceeding 12 deg/s
    FAULT_DRIFT_FSR_BASELINE,            // Resting baseline shifted > 400 counts from calibration
    FAULT_STUCK_AT_IMU,                  // Invariant IMU readings during dynamic grip/motion
    FAULT_STUCK_AT_FSR,                  // Invariant FSR ADC during active physical repetitions
    FAULT_GAIN_LOSS_SUSPECTED            // Diminished dynamic response relative to calibration
};

/**
 * Diagnostic Telemetry Container
 */
struct DiagnosticReport {
    SensorHealthState overallHealth;
    SensorHealthState mpuHealth;
    SensorHealthState fsrHealth;

    FaultType primaryFault;
    char faultMessage[64];
    uint32_t faultOnsetTimestampMs;
    uint32_t timeInFaultMs;

    // Reliability & Assessment Gating
    bool isAssessmentReliable;            // True if HEALTHY or DEGRADED
    bool repCountingPermitted;            // False if SUSPECTED_FAULT, UNAVAILABLE, CALIBRATION_REQUIRED
    float assessmentConfidencePct;        // 0.0% to 100.0%

    // Diagnostic Metrics
    float gyroBiasMagnitude_dps;
    float fsrBaselineDeltaADC;
    uint32_t consecutiveI2cErrors;
    uint32_t recoveryStabilityElapsedMs;
};

class FaultDetectionEngine {
public:
    FaultDetectionEngine();

    /**
     * Initialize fault detection state machines
     */
    void init();

    /**
     * Evaluate 50 Hz frames across Dropout, Drift, Stuck-At, and Gain Loss detectors.
     */
    void update(const RawSensorData& raw, 
                const ProcessedSensorData& proc, 
                const AcquisitionHealth& acqHealth, 
                const CalibrationProfile& calib);

    /**
     * Telemetry and State Getters
     */
    const DiagnosticReport& getReport() const { return _report; }
    SensorHealthState getOverallHealth() const { return _report.overallHealth; }
    bool isRepCountingPermitted() const { return _report.repCountingPermitted; }
    const char* getHealthStateName(SensorHealthState state) const;
    const char* getFaultTypeName(FaultType fault) const;

    /**
     * Simulation / Fault-Injection hooks for testing
     */
    void injectFault(FaultType fault);
    void clearInjectedFaults();

private:
    DiagnosticReport _report;

    // Recovery Timing
    uint32_t _recoveryStartTimeMs;
    bool _isRecovering;

    // Stuck-At Contextual Detection Buffers
    static constexpr uint8_t STUCK_BUFFER_SIZE = 50; // 1 second rolling window
    float _lastPitchSamples[STUCK_BUFFER_SIZE];
    uint16_t _lastFsrSamples[STUCK_BUFFER_SIZE];
    uint8_t _stuckBufferIndex;
    uint8_t _stuckBufferCount;

    // Injected simulated faults for testing
    FaultType _injectedFault;

    void evaluateMPUDropout(const AcquisitionHealth& acqHealth, uint32_t nowMs);
    void evaluateFSRDropout(const RawSensorData& raw, uint32_t nowMs);
    void evaluateDrift(const ProcessedSensorData& proc, const CalibrationProfile& calib, uint32_t nowMs);
    void evaluateStuckAt(const RawSensorData& raw, const ProcessedSensorData& proc, uint32_t nowMs);
    void updateHealthState(uint32_t nowMs);
};

#endif // FAULT_DETECTION_H
