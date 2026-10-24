#include "SignalProcessing.h"
#include <math.h>

SignalProcessing::SignalProcessing()
    : _initialized(false),
      _pitchDeg(0.0f),
      _rollDeg(0.0f),
      _gyroBiasX(0.0f),
      _gyroBiasY(0.0f),
      _gyroBiasZ(0.0f),
      _stationarySamplesCount(0),
      _smoothedFsrADC(0.0f),
      _calibratedRelaxedADC(Config::CalibrationDefaults::FALLBACK_FSR_RELAXED_ADC),
      _calibratedFirmADC(Config::CalibrationDefaults::FALLBACK_FSR_FIRM_ADC),
      _ringBufferHead(0),
      _ringBufferCount(0),
      _isStationary(false),
      _stationaryStartTimeMs(0),
      _stationaryDurationMs(0) {
    for (uint8_t i = 0; i < Config::Filtering::STATIONARY_WINDOW_SIZE; i++) {
        _accelMagHistory[i] = 9.80665f;
        _gyroMagHistory[i] = 0.0f;
    }
}

void SignalProcessing::init() {
    _initialized = false;
    _pitchDeg = 0.0f;
    _rollDeg = 0.0f;
    _gyroBiasX = 0.0f;
    _gyroBiasY = 0.0f;
    _gyroBiasZ = 0.0f;
    _stationarySamplesCount = 0;
    _smoothedFsrADC = 0.0f;
    _ringBufferHead = 0;
    _ringBufferCount = 0;
    _isStationary = false;
    _stationaryStartTimeMs = 0;
    _stationaryDurationMs = 0;
}

void SignalProcessing::setGripCalibration(uint16_t relaxedADC, uint16_t firmADC) {
    if (firmADC > relaxedADC + 100) {
        _calibratedRelaxedADC = relaxedADC;
        _calibratedFirmADC = firmADC;
    }
}

void SignalProcessing::resetOrientation(float initialPitch, float initialRoll) {
    _pitchDeg = initialPitch;
    _rollDeg = initialRoll;
    _initialized = true;
}

void SignalProcessing::updateStationaryDetector(float accelMag, float gyroMag, uint32_t nowMs, 
                                               float& outAccelVar, float& outGyroVar) {
    // 1. Insert into circular ring buffer
    _accelMagHistory[_ringBufferHead] = accelMag;
    _gyroMagHistory[_ringBufferHead] = gyroMag;
    _ringBufferHead = (_ringBufferHead + 1) % Config::Filtering::STATIONARY_WINDOW_SIZE;
    if (_ringBufferCount < Config::Filtering::STATIONARY_WINDOW_SIZE) {
        _ringBufferCount++;
    }

    // 2. Compute means
    float sumAccel = 0.0f;
    float sumGyro = 0.0f;
    for (uint8_t i = 0; i < _ringBufferCount; i++) {
        sumAccel += _accelMagHistory[i];
        sumGyro += _gyroMagHistory[i];
    }
    float meanAccel = sumAccel / _ringBufferCount;
    float meanGyro = sumGyro / _ringBufferCount;

    // 3. Compute variances
    float varAccelSum = 0.0f;
    float varGyroSum = 0.0f;
    for (uint8_t i = 0; i < _ringBufferCount; i++) {
        float diffA = _accelMagHistory[i] - meanAccel;
        float diffG = _gyroMagHistory[i] - meanGyro;
        varAccelSum += diffA * diffA;
        varGyroSum += diffG * diffG;
    }
    outAccelVar = varAccelSum / _ringBufferCount;
    outGyroVar = varGyroSum / _ringBufferCount;

    // 4. Evaluate stationary criteria
    bool stationaryNow = (outAccelVar < Config::Filtering::STATIONARY_ACCEL_VAR_THRESH) &&
                         (meanGyro < Config::Filtering::STATIONARY_GYRO_THRESH_DPS);

    if (stationaryNow) {
        if (!_isStationary) {
            _isStationary = true;
            _stationaryStartTimeMs = nowMs;
            _stationaryDurationMs = 0;
        } else {
            _stationaryDurationMs = nowMs - _stationaryStartTimeMs;
        }
    } else {
        _isStationary = false;
        _stationaryStartTimeMs = nowMs;
        _stationaryDurationMs = 0;
    }
}

void SignalProcessing::updateGyroBias(float rawGx, float rawGy, float rawGz) {
    // Only adapt bias when the device has been reliably stationary for > 300 ms
    if (_isStationary && _stationaryDurationMs > 300) {
        constexpr float BETA = 0.015f; // Slow leaky learning rate
        _gyroBiasX = (1.0f - BETA) * _gyroBiasX + BETA * rawGx;
        _gyroBiasY = (1.0f - BETA) * _gyroBiasY + BETA * rawGy;
        _gyroBiasZ = (1.0f - BETA) * _gyroBiasZ + BETA * rawGz;

        // Clamp to prevent runaway during accidental undetected motion
        constexpr float MAX_BIAS = 15.0f; // +/- 15 deg/s
        _gyroBiasX = constrain(_gyroBiasX, -MAX_BIAS, MAX_BIAS);
        _gyroBiasY = constrain(_gyroBiasY, -MAX_BIAS, MAX_BIAS);
        _gyroBiasZ = constrain(_gyroBiasZ, -MAX_BIAS, MAX_BIAS);

        _stationarySamplesCount++;
    }
}

void SignalProcessing::process(const RawSensorData& raw, ProcessedSensorData& out) {
    uint32_t nowMs = raw.timestampMs;
    float dt = (raw.dtUs > 0 && raw.dtUs < 100000) ? (raw.dtUs * 1e-6f) : Config::Timing::DT_SECONDS;

    // 1. Calculate Magnitudes
    float accelMag = sqrtf(raw.accelX_mps2 * raw.accelX_mps2 +
                           raw.accelY_mps2 * raw.accelY_mps2 +
                           raw.accelZ_mps2 * raw.accelZ_mps2);

    float gyroMag = sqrtf(raw.gyroX_dps * raw.gyroX_dps +
                          raw.gyroY_dps * raw.gyroY_dps +
                          raw.gyroZ_dps * raw.gyroZ_dps);

    // 2. Stationary Detection
    float accelVar = 0.0f;
    float gyroVar = 0.0f;
    updateStationaryDetector(accelMag, gyroMag, nowMs, accelVar, gyroVar);

    // 3. Dynamic Gyroscope Bias Compensation
    updateGyroBias(raw.gyroX_dps, raw.gyroY_dps, raw.gyroZ_dps);

    // Subtract estimated zero-rate bias
    float compGx = raw.gyroX_dps - _gyroBiasX;
    float compGy = raw.gyroY_dps - _gyroBiasY;
    float compGz = raw.gyroZ_dps - _gyroBiasZ;

    float compGyroMag = sqrtf(compGx * compGx + compGy * compGy + compGz * compGz);

    // 4. Gravity-based Orientation Reference (Accelerometer)
    float accelPitch = 0.0f;
    float accelRoll = 0.0f;
    bool accelValid = (accelMag > 2.0f && accelMag < 18.0f); // Reject extreme flight/impact states

    if (accelValid) {
        // Pitch: Rotation around transverse Y axis (forearm pitch up/down)
        // Roll:  Rotation around longitudinal X axis (pronation/supination)
        accelPitch = atan2f(raw.accelX_mps2, sqrtf(raw.accelY_mps2 * raw.accelY_mps2 + 
                                                   raw.accelZ_mps2 * raw.accelZ_mps2)) * (180.0f / M_PI);
        accelRoll  = atan2f(raw.accelY_mps2, raw.accelZ_mps2) * (180.0f / M_PI);
    }

    // 5. Complementary Filter Orientation Fusion
    if (!_initialized) {
        if (accelValid) {
            _pitchDeg = accelPitch;
            _rollDeg = accelRoll;
            _initialized = true;
        }
    } else {
        // Gyro integration step
        // In glove coordinate frame: Pitch rate aligns with Gyro Y, Roll rate aligns with Gyro X
        float predPitch = _pitchDeg + compGy * dt;
        float predRoll  = _rollDeg  + compGx * dt;

        if (accelValid) {
            // Adaptive alpha: If stationary, give slightly more weight to accelerometer to zero out drift
            float alpha = _isStationary ? 0.90f : Config::Filtering::COMPLEMENTARY_ALPHA;
            _pitchDeg = alpha * predPitch + (1.0f - alpha) * accelPitch;
            _rollDeg  = alpha * predRoll  + (1.0f - alpha) * accelRoll;
        } else {
            // High dynamic acceleration -> purely trust gyro integration to avoid centrifugal acceleration distortion
            _pitchDeg = predPitch;
            _rollDeg  = predRoll;
        }
    }

    // 6. FSR Asymmetric Smoothing & Grip Normalization
    float targetADC = (float)raw.fsrRawADC;
    if (_smoothedFsrADC < 1.0f) {
        _smoothedFsrADC = targetADC;
    } else {
        // Fast rise (attack) to capture grip onset, slower decay to reject grip slippage ripple
        float alpha = (targetADC > _smoothedFsrADC) ? 0.45f : 0.25f;
        _smoothedFsrADC = alpha * targetADC + (1.0f - alpha) * _smoothedFsrADC;
    }

    float normGrip = 0.0f;
    if (_calibratedFirmADC > _calibratedRelaxedADC + 50) {
        normGrip = (_smoothedFsrADC - _calibratedRelaxedADC) / 
                   (float)(_calibratedFirmADC - _calibratedRelaxedADC);
        normGrip = constrain(normGrip, 0.0f, 1.0f);
    }

    // 7. Populate Output Structure
    out.accelX_mps2 = raw.accelX_mps2;
    out.accelY_mps2 = raw.accelY_mps2;
    out.accelZ_mps2 = raw.accelZ_mps2;
    out.accelMagnitude_mps2 = accelMag;

    out.gyroX_dps = compGx;
    out.gyroY_dps = compGy;
    out.gyroZ_dps = compGz;
    out.gyroMagnitude_dps = compGyroMag;

    out.gyroBiasX_dps = _gyroBiasX;
    out.gyroBiasY_dps = _gyroBiasY;
    out.gyroBiasZ_dps = _gyroBiasZ;

    out.accelPitchDeg = accelPitch;
    out.accelRollDeg = accelRoll;
    out.pitchDeg = _pitchDeg;
    out.rollDeg = _rollDeg;

    out.filteredFsrADC = _smoothedFsrADC;
    out.filteredFsrVoltage = (_smoothedFsrADC / (float)Config::Hardware::ADC_MAX_COUNT) * Config::Hardware::VCC_VOLTS;
    out.normalizedGrip = normGrip;

    out.isStationary = _isStationary;
    out.stationaryDurationMs = _stationaryDurationMs;
    out.accelVariance = accelVar;
    out.gyroVariance = gyroVar;
    out.signalNoiseEstimate = sqrtf(accelVar);
}
