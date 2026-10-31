#include "Config.h"
#include "SignalProcessing.h"

SignalProcessing::SignalProcessing()
    : _initialized(false),
      _filteredTiltDeg(0.0f),
      _filteredGyroMag(0.0f),
      _gyroBiasX(0.0f),
      _gyroBiasY(0.0f),
      _gyroBiasZ(0.0f),
      _stationarySamplesCount(0),
      _fsrWindowHead(0),
      _fsrWindowCount(0),
      _smoothedFsrADC(0.0f),
      _calibratedRelaxedADC(Config::CalibrationDefaults::FALLBACK_FSR_RELAXED_ADC),
      _calibratedFirmADC(Config::CalibrationDefaults::FALLBACK_FSR_FIRM_ADC),
      _gripGate(false),
      _gripGateDebounceCount(0),
      _ringBufferHead(0),
      _ringBufferCount(0),
      _isStationary(false),
      _stationaryStartTimeMs(0),
      _stationaryDurationMs(0) {
    _q[0] = 1.0f;
    _q[1] = 0.0f;
    _q[2] = 0.0f;
    _q[3] = 0.0f;

    _gravityVector[0] = 0.0f;
    _gravityVector[1] = 0.0f;
    _gravityVector[2] = 1.0f;

    _refGravity[0] = 0.0f;
    _refGravity[1] = 0.0f;
    _refGravity[2] = 1.0f;

    for (uint8_t i = 0; i < 5; i++) {
        _fsrWindow[i] = _calibratedRelaxedADC;
    }

    for (uint8_t i = 0; i < Config::Filtering::STATIONARY_WINDOW_SIZE; i++) {
        _accelMagHistory[i] = 9.80665f;
        _gyroMagHistory[i] = 0.0f;
    }
}

void SignalProcessing::init() {
    _initialized = false;
    _q[0] = 1.0f;
    _q[1] = 0.0f;
    _q[2] = 0.0f;
    _q[3] = 0.0f;

    _gravityVector[0] = 0.0f;
    _gravityVector[1] = 0.0f;
    _gravityVector[2] = 1.0f;

    _refGravity[0] = 0.0f;
    _refGravity[1] = 0.0f;
    _refGravity[2] = 1.0f;

    _filteredTiltDeg = 0.0f;
    _filteredGyroMag = 0.0f;

    _gyroBiasX = 0.0f;
    _gyroBiasY = 0.0f;
    _gyroBiasZ = 0.0f;
    _stationarySamplesCount = 0;

    _fsrWindowHead = 0;
    _fsrWindowCount = 0;
    _smoothedFsrADC = 0.0f;

    _gripGate = false;
    _gripGateDebounceCount = 0;

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

void SignalProcessing::setReferenceGravity(const float refG[3]) {
    float mag = sqrtf(refG[0] * refG[0] + refG[1] * refG[1] + refG[2] * refG[2]);
    if (mag > 0.001f) {
        _refGravity[0] = refG[0] / mag;
        _refGravity[1] = refG[1] / mag;
        _refGravity[2] = refG[2] / mag;
    }
}

float SignalProcessing::computeTiltAngleDeg(const float g[3], const float refG[3]) {
    float dot = g[0] * refG[0] + g[1] * refG[1] + g[2] * refG[2];
    dot = constrain(dot, -1.0f, 1.0f);
    return acosf(dot) * (180.0f / M_PI);
}

void SignalProcessing::resetOrientation(float ax, float ay, float az) {
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    if (mag > 0.001f) {
        ax /= mag;
        ay /= mag;
        az /= mag;

        // Initialize quaternion aligning world Z (0, 0, 1) with measured gravity vector (ax, ay, az)
        // Vector cross product (0, 0, 1) x (ax, ay, az) = (-ay, ax, 0)
        // Half angle formula
        float cosTheta = az;
        if (cosTheta > 0.9999f) {
            _q[0] = 1.0f; _q[1] = 0.0f; _q[2] = 0.0f; _q[3] = 0.0f;
        } else if (cosTheta < -0.9999f) {
            _q[0] = 0.0f; _q[1] = 1.0f; _q[2] = 0.0f; _q[3] = 0.0f;
        } else {
            float s = sqrtf(2.0f * (1.0f + cosTheta));
            _q[0] = 0.5f * s;
            _q[1] = -ay / s;
            _q[2] =  ax / s;
            _q[3] = 0.0f;
        }

        _gravityVector[0] = ax;
        _gravityVector[1] = ay;
        _gravityVector[2] = az;
    }
    _initialized = true;
}

void SignalProcessing::updateStationaryDetector(float accelMag, float gyroMag, uint32_t nowMs, 
                                                float& outAccelVar, float& outGyroVar) {
    _accelMagHistory[_ringBufferHead] = accelMag;
    _gyroMagHistory[_ringBufferHead] = gyroMag;
    _ringBufferHead = (_ringBufferHead + 1) % Config::Filtering::STATIONARY_WINDOW_SIZE;
    if (_ringBufferCount < Config::Filtering::STATIONARY_WINDOW_SIZE) {
        _ringBufferCount++;
    }

    float sumAccel = 0.0f;
    float sumGyro = 0.0f;
    for (uint8_t i = 0; i < _ringBufferCount; i++) {
        sumAccel += _accelMagHistory[i];
        sumGyro += _gyroMagHistory[i];
    }
    float meanAccel = sumAccel / _ringBufferCount;
    float meanGyro = sumGyro / _ringBufferCount;

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
    if (_isStationary && _stationaryDurationMs > 300) {
        constexpr float BETA = 0.015f;
        _gyroBiasX = (1.0f - BETA) * _gyroBiasX + BETA * rawGx;
        _gyroBiasY = (1.0f - BETA) * _gyroBiasY + BETA * rawGy;
        _gyroBiasZ = (1.0f - BETA) * _gyroBiasZ + BETA * rawGz;

        constexpr float MAX_BIAS = 15.0f;
        _gyroBiasX = constrain(_gyroBiasX, -MAX_BIAS, MAX_BIAS);
        _gyroBiasY = constrain(_gyroBiasY, -MAX_BIAS, MAX_BIAS);
        _gyroBiasZ = constrain(_gyroBiasZ, -MAX_BIAS, MAX_BIAS);

        _stationarySamplesCount++;
    }
}

float SignalProcessing::getMedianFSR(uint16_t newSample) {
    _fsrWindow[_fsrWindowHead] = newSample;
    _fsrWindowHead = (_fsrWindowHead + 1) % 5;
    if (_fsrWindowCount < 5) _fsrWindowCount++;

    uint16_t temp[5];
    for (uint8_t i = 0; i < _fsrWindowCount; i++) {
        temp[i] = _fsrWindow[i];
    }

    // In-place insertion sort (max 5 items, O(1))
    for (uint8_t i = 1; i < _fsrWindowCount; i++) {
        uint16_t key = temp[i];
        int8_t j = i - 1;
        while (j >= 0 && temp[j] > key) {
            temp[j + 1] = temp[j];
            j--;
        }
        temp[j + 1] = key;
    }

    return (float)temp[_fsrWindowCount / 2];
}

void SignalProcessing::updateMahony(float ax, float ay, float az, float gx, float gy, float gz, float dt) {
    float accelMag = sqrtf(ax * ax + ay * ay + az * az);

    // Trust accelerometer ONLY when | |a| - 1g | is small (< 1.5 m/s^2 deviation from 9.80665 m/s^2)
    bool accelTrustworthy = fabsf(accelMag - 9.80665f) < 1.5f;

    float ex = 0.0f;
    float ey = 0.0f;
    float ez = 0.0f;

    if (accelTrustworthy && accelMag > 0.001f) {
        float normA = 1.0f / accelMag;
        ax *= normA;
        ay *= normA;
        az *= normA;

        // Estimated gravity direction in sensor coordinate frame from current quaternion
        float vx = 2.0f * (_q[1] * _q[3] - _q[0] * _q[2]);
        float vy = 2.0f * (_q[0] * _q[1] + _q[2] * _q[3]);
        float vz = _q[0] * _q[0] - _q[1] * _q[1] - _q[2] * _q[2] + _q[3] * _q[3];

        // Error vector is the cross product between measured gravity and estimated gravity
        ex = ay * vz - az * vy;
        ey = az * vx - ax * vz;
        ez = ax * vy - ay * vx;
    }

    // Proportional feedback gain Kp (0.8f provides rapid attitude convergence without noise jitter)
    constexpr float KP = 0.8f;
    float compGx = gx + KP * ex;
    float compGy = gy + KP * ey;
    float compGz = gz + KP * ez;

    // Convert deg/s to rad/s for quaternion integration
    float gxRad = compGx * (M_PI / 180.0f);
    float gyRad = compGy * (M_PI / 180.0f);
    float gzRad = compGz * (M_PI / 180.0f);

    // Quaternion derivative
    float qDot0 = 0.5f * (-_q[1] * gxRad - _q[2] * gyRad - _q[3] * gzRad);
    float qDot1 = 0.5f * ( _q[0] * gxRad + _q[2] * gzRad - _q[3] * gyRad);
    float qDot2 = 0.5f * ( _q[0] * gyRad - _q[1] * gzRad + _q[3] * gxRad);
    float qDot3 = 0.5f * ( _q[0] * gzRad + _q[1] * gyRad - _q[2] * gxRad);

    _q[0] += qDot0 * dt;
    _q[1] += qDot1 * dt;
    _q[2] += qDot2 * dt;
    _q[3] += qDot3 * dt;

    // Normalize quaternion
    float qNorm = sqrtf(_q[0] * _q[0] + _q[1] * _q[1] + _q[2] * _q[2] + _q[3] * _q[3]);
    if (qNorm > 0.0001f) {
        float invNorm = 1.0f / qNorm;
        _q[0] *= invNorm;
        _q[1] *= invNorm;
        _q[2] *= invNorm;
        _q[3] *= invNorm;
    }

    // Extract unit gravity vector in sensor coordinates
    _gravityVector[0] = 2.0f * (_q[1] * _q[3] - _q[0] * _q[2]);
    _gravityVector[1] = 2.0f * (_q[0] * _q[1] + _q[2] * _q[3]);
    _gravityVector[2] = _q[0] * _q[0] - _q[1] * _q[1] - _q[2] * _q[2] + _q[3] * _q[3];
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

    float compGx = raw.gyroX_dps - _gyroBiasX;
    float compGy = raw.gyroY_dps - _gyroBiasY;
    float compGz = raw.gyroZ_dps - _gyroBiasZ;
    float compGyroMag = sqrtf(compGx * compGx + compGy * compGy + compGz * compGz);

    // 4. 6-DoF Mahony Gravity Vector Filter
    bool wasNotInit = !_initialized;
    if (wasNotInit) {
        resetOrientation(raw.accelX_mps2, raw.accelY_mps2, raw.accelZ_mps2);
    } else {
        updateMahony(raw.accelX_mps2, raw.accelY_mps2, raw.accelZ_mps2, compGx, compGy, compGz, dt);
    }

    // 5. Angular Tilt vs Reference Gravity Vector & Low-Pass Filtering
    float rawTiltDeg = computeTiltAngleDeg(_gravityVector, _refGravity);

    // Tilt LPF (~4 Hz cutoff at 50 Hz: alpha ~ 0.334)
    constexpr float ALPHA_TILT = 0.334f;
    constexpr float ALPHA_GYRO = 0.386f;
    if (wasNotInit) {
        _filteredTiltDeg = rawTiltDeg;
        _filteredGyroMag = compGyroMag;
    } else {
        _filteredTiltDeg = (1.0f - ALPHA_TILT) * _filteredTiltDeg + ALPHA_TILT * rawTiltDeg;
        _filteredGyroMag = (1.0f - ALPHA_GYRO) * _filteredGyroMag + ALPHA_GYRO * compGyroMag;
    }

    // 6. Grip Processing: Median-of-5 + EMA
    float medianADC = getMedianFSR(raw.fsrRawADC);
    if (_smoothedFsrADC < 1.0f) {
        _smoothedFsrADC = medianADC;
    } else {
        constexpr float ALPHA_GRIP = 0.35f;
        _smoothedFsrADC = ALPHA_GRIP * medianADC + (1.0f - ALPHA_GRIP) * _smoothedFsrADC;
    }

    float normGrip = 0.0f;
    if (_calibratedFirmADC > _calibratedRelaxedADC + 50) {
        normGrip = (_smoothedFsrADC - _calibratedRelaxedADC) / 
                   (float)(_calibratedFirmADC - _calibratedRelaxedADC);
        normGrip = constrain(normGrip, 0.0f, 1.0f);
    }

    // 7. Grip Gate: Schmitt Trigger (ON >= 0.35, OFF <= 0.20, debounced ~160 ms / 8 samples at 50 Hz)
    if (!_gripGate) {
        if (normGrip >= 0.35f) {
            _gripGateDebounceCount++;
            if (_gripGateDebounceCount >= 8) {
                _gripGate = true;
                _gripGateDebounceCount = 0;
            }
        } else {
            _gripGateDebounceCount = 0;
        }
    } else {
        if (normGrip <= 0.20f) {
            _gripGateDebounceCount++;
            if (_gripGateDebounceCount >= 8) {
                _gripGate = false;
                _gripGateDebounceCount = 0;
            }
        } else {
            _gripGateDebounceCount = 0;
        }
    }

    // 8. Legacy pitch and roll for telemetry/dashboard display
    float pitch = atan2f(-_gravityVector[0], sqrtf(_gravityVector[1] * _gravityVector[1] + _gravityVector[2] * _gravityVector[2])) * (180.0f / M_PI);
    float roll  = atan2f(_gravityVector[1], _gravityVector[2]) * (180.0f / M_PI);

    // 9. Populate Processed Output
    out.accelX_mps2 = raw.accelX_mps2;
    out.accelY_mps2 = raw.accelY_mps2;
    out.accelZ_mps2 = raw.accelZ_mps2;
    out.accelMagnitude_mps2 = accelMag;

    out.gyroX_dps = compGx;
    out.gyroY_dps = compGy;
    out.gyroZ_dps = compGz;
    out.gyroMagnitude_dps = compGyroMag;
    out.filteredGyroMag_dps = _filteredGyroMag;

    out.gyroBiasX_dps = _gyroBiasX;
    out.gyroBiasY_dps = _gyroBiasY;
    out.gyroBiasZ_dps = _gyroBiasZ;

    out.gravityVector[0] = _gravityVector[0];
    out.gravityVector[1] = _gravityVector[1];
    out.gravityVector[2] = _gravityVector[2];

    out.tiltDeg = rawTiltDeg;
    out.filteredTiltDeg = _filteredTiltDeg;

    out.pitchDeg = pitch;
    out.rollDeg = roll;

    out.filteredFsrADC = _smoothedFsrADC;
    out.filteredFsrVoltage = (_smoothedFsrADC / (float)Config::Hardware::ADC_MAX_COUNT) * Config::Hardware::VCC_VOLTS;
    out.normalizedGrip = normGrip;
    out.gripGate = _gripGate;

    out.isStationary = _isStationary;
    out.stationaryDurationMs = _stationaryDurationMs;
    out.accelVariance = accelVar;
    out.gyroVariance = gyroVar;
    out.signalNoiseEstimate = sqrtf(accelVar);
}
