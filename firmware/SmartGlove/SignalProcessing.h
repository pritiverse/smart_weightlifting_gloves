#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <math.h>
#include "Config.h"
#include "SensorAcquisition.h"

/**
 * Filtered, bias-compensated, and fused sensor metrics (Schema v2)
 */
struct ProcessedSensorData {
    // Filtered Acceleration (m/s^2)
    float accelX_mps2;
    float accelY_mps2;
    float accelZ_mps2;
    float accelMagnitude_mps2;

    // Bias-Compensated Gyroscope (deg/s)
    float gyroX_dps;
    float gyroY_dps;
    float gyroZ_dps;
    float gyroMagnitude_dps;
    float filteredGyroMag_dps;  // ~5 Hz LPF

    // Estimated Gyroscope Zero-Rate Biases (deg/s)
    float gyroBiasX_dps;
    float gyroBiasY_dps;
    float gyroBiasZ_dps;

    // 6-DoF Orientation: Gravity unit vector in sensor coordinates (|g| = 1.0)
    float gravityVector[3];

    // Tilt angle relative to active reference gravity vector (deg)
    float tiltDeg;              // Instantaneous tilt vs reference
    float filteredTiltDeg;      // ~4 Hz LPF tilt

    // Legacy Pitch and Roll angles (deg)
    float pitchDeg;
    float rollDeg;

    // Filtered FSR & Relative Grip
    float filteredFsrADC;
    float filteredFsrVoltage;
    float normalizedGrip;       // 0.0 (relaxed) to 1.0 (firm) via Median-of-5 + EMA
    bool gripGate;              // Schmitt trigger gate: ON >= 0.35, OFF <= 0.20, 160ms debounced

    // Motion & Signal Quality Flags
    bool isStationary;
    uint32_t stationaryDurationMs;
    float accelVariance;
    float gyroVariance;
    float signalNoiseEstimate;
};

class SignalProcessing {
public:
    SignalProcessing();

    /**
     * Initialize filter states, quaternion orientation, and buffers
     */
    void init();

    /**
     * Process raw sensor sample: performs stationary detection, bias correction,
     * 6-DoF Mahony gravity vector fusion, LPF filtering, median-5 grip, and Schmitt gate.
     */
    void process(const RawSensorData& raw, ProcessedSensorData& out);

    /**
     * Reset orientation estimate to match current accelerometer gravity vector
     */
    void resetOrientation(float ax = 0.0f, float ay = 0.0f, float az = 9.80665f);

    /**
     * Set active grip reference points (used for normalized grip calculation)
     */
    void setGripCalibration(uint16_t relaxedADC, uint16_t firmADC);

    /**
     * Set reference gravity vector used for real-time tiltDeg calculation
     */
    void setReferenceGravity(const float refG[3]);

    /**
     * Compute angular tilt in degrees between estimated gravity vector and arbitrary reference vector
     */
    static float computeTiltAngleDeg(const float g[3], const float refG[3]);

    /**
     * Getters
     */
    bool isStationary() const { return _isStationary; }
    uint32_t getStationaryDurationMs() const { return _stationaryDurationMs; }
    bool isGripGateActive() const { return _gripGate; }
    void getGravityVector(float outG[3]) const {
        outG[0] = _gravityVector[0];
        outG[1] = _gravityVector[1];
        outG[2] = _gravityVector[2];
    }

private:
    bool _initialized;

    // 6-DoF Mahony Filter State (Unit Quaternion)
    float _q[4];                // (q0, q1, q2, q3)
    float _gravityVector[3];    // Estimated unit gravity vector in sensor coordinates
    float _refGravity[3];       // Active reference gravity vector for tilt calculation

    // Running Low-Pass Filter States
    float _filteredTiltDeg;     // ~4 Hz LPF state
    float _filteredGyroMag;     // ~5 Hz LPF state

    // Running Gyro Biases
    float _gyroBiasX;
    float _gyroBiasY;
    float _gyroBiasZ;
    uint32_t _stationarySamplesCount;

    // FSR Median-of-5 Ring Buffer & EMA State
    uint16_t _fsrWindow[5];
    uint8_t _fsrWindowHead;
    uint8_t _fsrWindowCount;
    float _smoothedFsrADC;

    // Grip baseline thresholds (calibrated)
    uint16_t _calibratedRelaxedADC;
    uint16_t _calibratedFirmADC;

    // Schmitt Trigger Grip Gate State & 160 ms Debounce Counter (8 samples at 50 Hz)
    bool _gripGate;
    uint8_t _gripGateDebounceCount;

    // Circular Ring Buffer for Stationary Variance Calculation (50 samples = 1.0 s)
    float _accelMagHistory[Config::Filtering::STATIONARY_WINDOW_SIZE];
    float _gyroMagHistory[Config::Filtering::STATIONARY_WINDOW_SIZE];
    uint8_t _ringBufferHead;
    uint8_t _ringBufferCount;

    // Stationary Tracker
    bool _isStationary;
    uint32_t _stationaryStartTimeMs;
    uint32_t _stationaryDurationMs;

    void updateStationaryDetector(float accelMag, float gyroMag, uint32_t nowMs, float& outAccelVar, float& outGyroVar);
    void updateGyroBias(float rawGx, float rawGy, float rawGz);
    void updateMahony(float ax, float ay, float az, float gx, float gy, float gz, float dt);
    float getMedianFSR(uint16_t newSample);
};

#endif // SIGNAL_PROCESSING_H
