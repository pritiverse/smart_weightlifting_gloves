#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include "Config.h"
#include "SensorAcquisition.h"

/**
 * Filtered, bias-compensated, and fused sensor metrics
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

    // Estimated Gyroscope Zero-Rate Biases (deg/s)
    float gyroBiasX_dps;
    float gyroBiasY_dps;
    float gyroBiasZ_dps;

    // Accelerometer-derived gravity tilt angles (deg)
    float accelPitchDeg;
    float accelRollDeg;

    // Fused Orientation via Complementary Filter (deg)
    float pitchDeg;
    float rollDeg;

    // Filtered FSR & Relative Grip
    float filteredFsrADC;
    float filteredFsrVoltage;
    float normalizedGrip;     // 0.0 (fully relaxed) to 1.0 (firm grip)

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
     * Initialize filter states and orientation tracking
     */
    void init();

    /**
     * Process raw sensor sample: performs stationary detection, bias correction,
     * complementary orientation fusion, and FSR smoothing.
     */
    void process(const RawSensorData& raw, ProcessedSensorData& out);

    /**
     * Reset orientation estimate to current gravity vector
     */
    void resetOrientation(float initialPitch = 0.0f, float initialRoll = 0.0f);

    /**
     * Set active grip reference points (used for normalized grip calculation)
     */
    void setGripCalibration(uint16_t relaxedADC, uint16_t firmADC);

    /**
     * Retrieve current stationary state
     */
    bool isStationary() const { return _isStationary; }
    uint32_t getStationaryDurationMs() const { return _stationaryDurationMs; }

private:
    // Filter & State Memory
    bool _initialized;
    float _pitchDeg;
    float _rollDeg;

    // Running Gyro Biases
    float _gyroBiasX;
    float _gyroBiasY;
    float _gyroBiasZ;
    uint32_t _stationarySamplesCount;

    // FSR EWMA State
    float _smoothedFsrADC;

    // Grip baseline thresholds (defaults from Config)
    uint16_t _calibratedRelaxedADC;
    uint16_t _calibratedFirmADC;

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
};

#endif // SIGNAL_PROCESSING_H
