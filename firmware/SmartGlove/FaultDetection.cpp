#include "FaultDetection.h"
#include <math.h>

FaultDetectionEngine::FaultDetectionEngine()
    : _recoveryStartTimeMs(0),
      _isRecovering(false),
      _stuckBufferIndex(0),
      _stuckBufferCount(0),
      _injectedFault(FAULT_NONE) {
    init();
}

void FaultDetectionEngine::init() {
    _report.overallHealth = HEALTH_STATE_HEALTHY;
    _report.mpuHealth = HEALTH_STATE_HEALTHY;
    _report.fsrHealth = HEALTH_STATE_HEALTHY;
    _report.primaryFault = FAULT_NONE;
    strncpy(_report.faultMessage, "All sensors nominal", sizeof(_report.faultMessage));
    _report.faultOnsetTimestampMs = 0;
    _report.timeInFaultMs = 0;
    _report.isAssessmentReliable = true;
    _report.repCountingPermitted = true;
    _report.assessmentConfidencePct = 100.0f;
    _report.gyroBiasMagnitude_dps = 0.0f;
    _report.fsrBaselineDeltaADC = 0.0f;
    _report.consecutiveI2cErrors = 0;
    _report.recoveryStabilityElapsedMs = 0;

    _recoveryStartTimeMs = 0;
    _isRecovering = false;
    _stuckBufferIndex = 0;
    _stuckBufferCount = 0;
    _injectedFault = FAULT_NONE;

    for (uint8_t i = 0; i < STUCK_BUFFER_SIZE; i++) {
        _lastPitchSamples[i] = 0.0f;
        _lastFsrSamples[i] = 0;
    }
}

const char* FaultDetectionEngine::getHealthStateName(SensorHealthState state) const {
    switch (state) {
        case HEALTH_STATE_HEALTHY:              return "HEALTHY";
        case HEALTH_STATE_DEGRADED:             return "DEGRADED";
        case HEALTH_STATE_SUSPECTED_FAULT:      return "SUSPECTED_FAULT";
        case HEALTH_STATE_SENSOR_UNAVAILABLE:   return "SENSOR_UNAVAILABLE";
        case HEALTH_STATE_RECOVERING:           return "RECOVERING";
        case HEALTH_STATE_CALIBRATION_REQUIRED: return "CALIBRATION_REQUIRED";
        default:                                return "UNKNOWN";
    }
}

const char* FaultDetectionEngine::getFaultTypeName(FaultType fault) const {
    switch (fault) {
        case FAULT_NONE:                  return "NONE";
        case FAULT_DROPOUT_MPU_I2C:       return "DROPOUT_MPU_I2C";
        case FAULT_DROPOUT_FSR_ADC:       return "DROPOUT_FSR_ADC";
        case FAULT_DRIFT_GYRO_BIAS:       return "DRIFT_GYRO_BIAS";
        case FAULT_DRIFT_FSR_BASELINE:    return "DRIFT_FSR_BASELINE";
        case FAULT_STUCK_AT_IMU:          return "STUCK_AT_IMU";
        case FAULT_STUCK_AT_FSR:          return "STUCK_AT_FSR";
        case FAULT_GAIN_LOSS_SUSPECTED:   return "GAIN_LOSS_SUSPECTED";
        default:                          return "UNKNOWN";
    }
}

void FaultDetectionEngine::injectFault(FaultType fault) {
    _injectedFault = fault;
}

void FaultDetectionEngine::clearInjectedFaults() {
    _injectedFault = FAULT_NONE;
}

void FaultDetectionEngine::evaluateMPUDropout(const AcquisitionHealth& acqHealth, uint32_t nowMs) {
    _report.consecutiveI2cErrors = acqHealth.consecutiveI2cFailures;

    bool i2cTimeout = (nowMs - acqHealth.lastSuccessfulReadMs > Config::Health::I2C_DROPOUT_TIMEOUT_MS);
    bool i2cBlackout = (!acqHealth.mpuResponding || acqHealth.consecutiveI2cFailures >= Config::Health::MAX_CONSECUTIVE_I2C_ERRORS);

    if (i2cTimeout || i2cBlackout || _injectedFault == FAULT_DROPOUT_MPU_I2C) {
        _report.mpuHealth = HEALTH_STATE_SENSOR_UNAVAILABLE;
        _report.primaryFault = FAULT_DROPOUT_MPU_I2C;
        strncpy(_report.faultMessage, "MPU6050 I2C communication lost", sizeof(_report.faultMessage));
    } else {
        _report.mpuHealth = HEALTH_STATE_HEALTHY;
    }
}

void FaultDetectionEngine::evaluateFSRDropout(const RawSensorData& raw, uint32_t nowMs) {
    if (_injectedFault == FAULT_DROPOUT_FSR_ADC) {
        _report.fsrHealth = HEALTH_STATE_SENSOR_UNAVAILABLE;
        _report.primaryFault = FAULT_DROPOUT_FSR_ADC;
        strncpy(_report.faultMessage, "FSR ADC hardware disconnect (simulated)", sizeof(_report.faultMessage));
        return;
    }

    // Detect saturation to 3.3V rail (indicates short circuit across FSR terminals)
    if (raw.fsrVoltage >= (Config::Hardware::VCC_VOLTS - 0.02f)) {
        _report.fsrHealth = HEALTH_STATE_SUSPECTED_FAULT;
        _report.primaryFault = FAULT_DROPOUT_FSR_ADC;
        strncpy(_report.faultMessage, "FSR shorted to 3.3V rail", sizeof(_report.faultMessage));
    } else {
        _report.fsrHealth = HEALTH_STATE_HEALTHY;
    }
}

void FaultDetectionEngine::evaluateDrift(const ProcessedSensorData& proc, const CalibrationProfile& calib, uint32_t nowMs) {
    // 1. Gyroscope Zero-Rate Bias Drift Evaluation
    float biasMag = sqrtf(proc.gyroBiasX_dps * proc.gyroBiasX_dps +
                          proc.gyroBiasY_dps * proc.gyroBiasY_dps +
                          proc.gyroBiasZ_dps * proc.gyroBiasZ_dps);
    _report.gyroBiasMagnitude_dps = biasMag;

    if (biasMag > Config::Health::MAX_PERMISSIBLE_BIAS_DRIFT_DPS || _injectedFault == FAULT_DRIFT_GYRO_BIAS) {
        if (_report.mpuHealth == HEALTH_STATE_HEALTHY) {
            _report.mpuHealth = HEALTH_STATE_DEGRADED;
            _report.primaryFault = FAULT_DRIFT_GYRO_BIAS;
            strncpy(_report.faultMessage, "Excessive gyro zero-rate drift (>12 deg/s)", sizeof(_report.faultMessage));
        }
    }

    // 2. FSR Resting Baseline Drift Evaluation
    if (calib.isValid && proc.isStationary && proc.stationaryDurationMs > 2000) {
        float deltaADC = fabsf(proc.filteredFsrADC - (float)calib.fsrRelaxedMeanADC);
        _report.fsrBaselineDeltaADC = deltaADC;

        if (deltaADC > 450.0f || _injectedFault == FAULT_DRIFT_FSR_BASELINE) {
            _report.fsrHealth = HEALTH_STATE_CALIBRATION_REQUIRED;
            _report.primaryFault = FAULT_DRIFT_FSR_BASELINE;
            strncpy(_report.faultMessage, "FSR resting baseline shifted > 450 counts. Recalibrate.", sizeof(_report.faultMessage));
        }
    }
}

void FaultDetectionEngine::evaluateStuckAt(const RawSensorData& raw, const ProcessedSensorData& proc, uint32_t nowMs) {
    // Insert into rolling 1-second buffers
    _lastPitchSamples[_stuckBufferIndex] = proc.pitchDeg;
    _lastFsrSamples[_stuckBufferIndex] = raw.fsrRawADC;
    _stuckBufferIndex = (_stuckBufferIndex + 1) % STUCK_BUFFER_SIZE;
    if (_stuckBufferCount < STUCK_BUFFER_SIZE) {
        _stuckBufferCount++;
        return; // Need full buffer before variance calculation
    }

    // Compute pitch variance and FSR variance over the 1.0 s window
    float sumPitch = 0.0f, sumPitchSq = 0.0f;
    float sumFsr = 0.0f, sumFsrSq = 0.0f;
    for (uint8_t i = 0; i < STUCK_BUFFER_SIZE; i++) {
        sumPitch += _lastPitchSamples[i];
        sumPitchSq += (_lastPitchSamples[i] * _lastPitchSamples[i]);
        sumFsr += (float)_lastFsrSamples[i];
        sumFsrSq += ((float)_lastFsrSamples[i] * (float)_lastFsrSamples[i]);
    }

    float meanPitch = sumPitch / STUCK_BUFFER_SIZE;
    float varPitch = (sumPitchSq / STUCK_BUFFER_SIZE) - (meanPitch * meanPitch);

    float meanFsr = sumFsr / STUCK_BUFFER_SIZE;
    float varFsr = (sumFsrSq / STUCK_BUFFER_SIZE) - (meanFsr * meanFsr);

    // Contextual Rule A: FSR is actively oscillating (varFsr > 400), but IMU pitch variance is frozen (< 0.0001)
    if ((varPitch < 0.0001f && varFsr > 400.0f) || _injectedFault == FAULT_STUCK_AT_IMU) {
        _report.mpuHealth = HEALTH_STATE_SUSPECTED_FAULT;
        _report.primaryFault = FAULT_STUCK_AT_IMU;
        strncpy(_report.faultMessage, "IMU pitch output is frozen during grip activity", sizeof(_report.faultMessage));
    }

    // Contextual Rule B: Wrist is executing dynamic repetitions (varPitch > 35 deg^2), but FSR variance is frozen (< 0.05)
    if ((varFsr < 0.05f && varPitch > 35.0f && meanFsr > 200.0f) || _injectedFault == FAULT_STUCK_AT_FSR) {
        _report.fsrHealth = HEALTH_STATE_SUSPECTED_FAULT;
        _report.primaryFault = FAULT_STUCK_AT_FSR;
        strncpy(_report.faultMessage, "FSR output is invariant during physical motion", sizeof(_report.faultMessage));
    }
}

void FaultDetectionEngine::updateHealthState(uint32_t nowMs) {
    // Determine raw composite health state (worst-case of both sensors)
    SensorHealthState worstSensor = _report.mpuHealth;
    if (_report.fsrHealth > worstSensor) {
        worstSensor = _report.fsrHealth;
    }

    // Handle recovery state transitions
    if (_report.overallHealth == HEALTH_STATE_SENSOR_UNAVAILABLE || 
        _report.overallHealth == HEALTH_STATE_SUSPECTED_FAULT) {
        if (worstSensor == HEALTH_STATE_HEALTHY) {
            // Signal has just returned to nominal! Enter RECOVERING state
            _isRecovering = true;
            _recoveryStartTimeMs = nowMs;
            _report.overallHealth = HEALTH_STATE_RECOVERING;
            strncpy(_report.faultMessage, "Signal restored. Verifying 1.5s stability...", sizeof(_report.faultMessage));
        }
    } else if (_isRecovering) {
        if (worstSensor != HEALTH_STATE_HEALTHY) {
            // Fault reoccurred during recovery window
            _isRecovering = false;
            _report.overallHealth = worstSensor;
        } else {
            uint32_t recoveryElapsed = nowMs - _recoveryStartTimeMs;
            _report.recoveryStabilityElapsedMs = recoveryElapsed;
            if (recoveryElapsed >= Config::Health::RECOVERY_STABILITY_TIME_MS) {
                // Completed recovery gate!
                _isRecovering = false;
                _report.overallHealth = HEALTH_STATE_HEALTHY;
                _report.primaryFault = FAULT_NONE;
                strncpy(_report.faultMessage, "Recovered to nominal operation", sizeof(_report.faultMessage));
            }
        }
    } else {
        _report.overallHealth = worstSensor;
        if (worstSensor == HEALTH_STATE_HEALTHY) {
            _report.primaryFault = FAULT_NONE;
            strncpy(_report.faultMessage, "All sensors nominal", sizeof(_report.faultMessage));
        }
    }

    // Assessment Gating & Confidence Score
    switch (_report.overallHealth) {
        case HEALTH_STATE_HEALTHY:
            _report.isAssessmentReliable = true;
            _report.repCountingPermitted = true;
            _report.assessmentConfidencePct = 100.0f;
            break;
        case HEALTH_STATE_DEGRADED:
            _report.isAssessmentReliable = true; // Permitted with lower confidence
            _report.repCountingPermitted = true;
            _report.assessmentConfidencePct = 70.0f;
            break;
        case HEALTH_STATE_RECOVERING:
            _report.isAssessmentReliable = false;
            _report.repCountingPermitted = false; // Hold reps until fully stable
            _report.assessmentConfidencePct = 50.0f;
            break;
        case HEALTH_STATE_CALIBRATION_REQUIRED:
            _report.isAssessmentReliable = false;
            _report.repCountingPermitted = false;
            _report.assessmentConfidencePct = 30.0f;
            break;
        case HEALTH_STATE_SUSPECTED_FAULT:
            _report.isAssessmentReliable = false;
            _report.repCountingPermitted = false;
            _report.assessmentConfidencePct = 20.0f;
            break;
        case HEALTH_STATE_SENSOR_UNAVAILABLE:
        default:
            _report.isAssessmentReliable = false;
            _report.repCountingPermitted = false;
            _report.assessmentConfidencePct = 0.0f;
            break;
    }
}

void FaultDetectionEngine::update(const RawSensorData& raw, 
                                 const ProcessedSensorData& proc, 
                                 const AcquisitionHealth& acqHealth, 
                                 const CalibrationProfile& calib) {
    uint32_t nowMs = millis();

    // 1. Evaluate Individual Fault Classes
    evaluateMPUDropout(acqHealth, nowMs);
    evaluateFSRDropout(raw, nowMs);
    evaluateDrift(proc, calib, nowMs);
    evaluateStuckAt(raw, proc, nowMs);

    // 2. Synthesize Composite 6-State Health & Recovery Gate
    updateHealthState(nowMs);
}
