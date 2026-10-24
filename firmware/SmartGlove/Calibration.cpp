#include "Calibration.h"
#include <math.h>

static constexpr uint32_t MAGIC_HEADER = 0x47524950; // "GRIP"
static constexpr uint16_t SCHEMA_VERSION = 1;

PersonalCalibration::PersonalCalibration()
    : _currentStep(CALIB_IDLE),
      _stepStartTimeMs(0),
      _sampleCount(0),
      _sumPitch(0.0f),
      _sumPitchSq(0.0f),
      _sumRoll(0.0f),
      _sumRollSq(0.0f),
      _sumADC(0),
      _sumADCSq(0) {
    memset(_failureReason, 0, sizeof(_failureReason));
    resetToDefaults();
}

uint32_t PersonalCalibration::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

void PersonalCalibration::resetToDefaults() {
    _profile.magicHeader = MAGIC_HEADER;
    _profile.schemaVersion = SCHEMA_VERSION;
    _profile.calibratedTimeMs = 0;

    _profile.neutralPitchDeg = Config::CalibrationDefaults::FALLBACK_PITCH_NEUTRAL_DEG;
    _profile.neutralRollDeg = Config::CalibrationDefaults::FALLBACK_ROLL_NEUTRAL_DEG;
    _profile.postureStabilityStd = 1.0f;

    _profile.fsrRelaxedMeanADC = Config::CalibrationDefaults::FALLBACK_FSR_RELAXED_ADC;
    _profile.fsrRelaxedStdDev = 30.0f;
    _profile.fsrFirmMeanADC = Config::CalibrationDefaults::FALLBACK_FSR_FIRM_ADC;
    _profile.fsrFirmStdDev = 60.0f;
    _profile.gripSeparationRatio = 20.0f;

    _profile.gripEligibilityADC = _profile.fsrRelaxedMeanADC + 
        (uint16_t)((_profile.fsrFirmMeanADC - _profile.fsrRelaxedMeanADC) * Config::RepEngineDefaults::MIN_GRIP_ELIGIBILITY_RATIO);
    _profile.postureToleranceDeg = 15.0f;

    _profile.isValid = false; // Flagged false until personalized calibration is performed
    _profile.crc32 = 0;
}

bool PersonalCalibration::init() {
    bool loaded = loadFromNVS();
    if (!loaded) {
        resetToDefaults();
    }
    return loaded;
}

const char* PersonalCalibration::getStepName() const {
    switch (_currentStep) {
        case CALIB_IDLE:               return "IDLE";
        case CALIB_STEP1_PREFLIGHT:     return "PREFLIGHT_CHECK";
        case CALIB_STEP2_NEUTRAL_WRIST: return "NEUTRAL_WRIST_HOLD";
        case CALIB_STEP3_RELAXED_GRIP:  return "RELAXED_GRIP_BASELINE";
        case CALIB_STEP4_FIRM_GRIP:     return "FIRM_GRIP_CAPTURE";
        case CALIB_STEP5_VALIDATION:    return "STATISTICAL_VALIDATION";
        case CALIB_COMPLETED:           return "COMPLETED";
        case CALIB_FAILED:              return "FAILED";
        default:                        return "UNKNOWN";
    }
}

uint8_t PersonalCalibration::getStepProgressPct() const {
    if (_currentStep == CALIB_COMPLETED) return 100;
    if (_currentStep == CALIB_IDLE || _currentStep == CALIB_FAILED) return 0;

    uint32_t elapsed = millis() - _stepStartTimeMs;
    uint32_t targetDuration = 1000;

    if (_currentStep == CALIB_STEP2_NEUTRAL_WRIST) {
        targetDuration = Config::CalibrationDefaults::NEUTRAL_RECORD_DURATION_MS;
    } else if (_currentStep == CALIB_STEP3_RELAXED_GRIP || _currentStep == CALIB_STEP4_FIRM_GRIP) {
        targetDuration = Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS;
    }

    uint32_t pct = (elapsed * 100) / targetDuration;
    return (uint8_t)constrain(pct, 0, 100);
}

void PersonalCalibration::resetAccumulators() {
    _stepStartTimeMs = millis();
    _sampleCount = 0;
    _sumPitch = 0.0f;
    _sumPitchSq = 0.0f;
    _sumRoll = 0.0f;
    _sumRollSq = 0.0f;
    _sumADC = 0;
    _sumADCSq = 0;
}

void PersonalCalibration::startWizard() {
    resetAccumulators();
    memset(_failureReason, 0, sizeof(_failureReason));
    _currentStep = CALIB_STEP1_PREFLIGHT;
}

void PersonalCalibration::advanceStep() {
    resetAccumulators();
    switch (_currentStep) {
        case CALIB_STEP1_PREFLIGHT:
            _currentStep = CALIB_STEP2_NEUTRAL_WRIST;
            break;
        case CALIB_STEP2_NEUTRAL_WRIST:
            _currentStep = CALIB_STEP3_RELAXED_GRIP;
            break;
        case CALIB_STEP3_RELAXED_GRIP:
            _currentStep = CALIB_STEP4_FIRM_GRIP;
            break;
        case CALIB_STEP4_FIRM_GRIP:
            _currentStep = CALIB_STEP5_VALIDATION;
            break;
        default:
            break;
    }
}

void PersonalCalibration::abortWizard(const char* reason) {
    failCalibration(reason);
}

void PersonalCalibration::failCalibration(const char* reason) {
    _currentStep = CALIB_FAILED;
    strncpy(_failureReason, reason, sizeof(_failureReason) - 1);
    _failureReason[sizeof(_failureReason) - 1] = '\0';
}

void PersonalCalibration::processSample(const RawSensorData& raw, const ProcessedSensorData& proc) {
    if (_currentStep == CALIB_IDLE || _currentStep == CALIB_COMPLETED || _currentStep == CALIB_FAILED) {
        return;
    }

    uint32_t elapsed = millis() - _stepStartTimeMs;

    switch (_currentStep) {
        case CALIB_STEP1_PREFLIGHT: {
            // Verify sensors are operational and resting
            if (raw.fsrVoltage > 2.5f) {
                failCalibration("Preflight: FSR is under heavy pressure. Relax hand.");
                return;
            }
            if (proc.accelMagnitude_mps2 < 6.0f || proc.accelMagnitude_mps2 > 14.0f) {
                failCalibration("Preflight: Excessive acceleration or freefall detected.");
                return;
            }
            // Require 800 ms of preflight stability before advancing
            if (elapsed >= 800) {
                advanceStep();
            }
            break;
        }

        case CALIB_STEP2_NEUTRAL_WRIST: {
            // Accumulate posture angles
            _sumPitch += proc.pitchDeg;
            _sumPitchSq += (proc.pitchDeg * proc.pitchDeg);
            _sumRoll += proc.rollDeg;
            _sumRollSq += (proc.rollDeg * proc.rollDeg);
            _sampleCount++;

            if (elapsed >= Config::CalibrationDefaults::NEUTRAL_RECORD_DURATION_MS) {
                if (_sampleCount < 50) {
                    failCalibration("Neutral hold: Insufficient samples collected.");
                    return;
                }
                float meanPitch = _sumPitch / _sampleCount;
                float varPitch = (_sumPitchSq / _sampleCount) - (meanPitch * meanPitch);
                float stdPitch = sqrtf(max(0.0f, varPitch));

                float meanRoll = _sumRoll / _sampleCount;
                float varRoll = (_sumRollSq / _sampleCount) - (meanRoll * meanRoll);
                float stdRoll = sqrtf(max(0.0f, varRoll));

                float combinedStd = sqrtf(stdPitch * stdPitch + stdRoll * stdRoll);

                if (combinedStd > 6.0f) {
                    failCalibration("Neutral hold: Excessive wrist movement during hold.");
                    return;
                }

                _profile.neutralPitchDeg = meanPitch;
                _profile.neutralRollDeg = meanRoll;
                _profile.postureStabilityStd = combinedStd;

                advanceStep(); // Move to relaxed grip
            }
            break;
        }

        case CALIB_STEP3_RELAXED_GRIP: {
            _sumADC += raw.fsrRawADC;
            _sumADCSq += (uint64_t)raw.fsrRawADC * raw.fsrRawADC;
            _sampleCount++;

            if (elapsed >= Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS) {
                if (_sampleCount < 40) {
                    failCalibration("Relaxed grip: Insufficient samples collected.");
                    return;
                }
                float meanADC = (float)_sumADC / _sampleCount;
                float varADC = ((float)_sumADCSq / _sampleCount) - (meanADC * meanADC);
                float stdADC = sqrtf(max(0.0f, varADC));

                if (meanADC > 1200.0f) {
                    failCalibration("Relaxed grip: Baseline too high (>1200 ADC). Release grip.");
                    return;
                }

                _profile.fsrRelaxedMeanADC = (uint16_t)meanADC;
                _profile.fsrRelaxedStdDev = stdADC;

                advanceStep(); // Move to firm grip
            }
            break;
        }

        case CALIB_STEP4_FIRM_GRIP: {
            _sumADC += raw.fsrRawADC;
            _sumADCSq += (uint64_t)raw.fsrRawADC * raw.fsrRawADC;
            _sampleCount++;

            if (elapsed >= Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS) {
                if (_sampleCount < 40) {
                    failCalibration("Firm grip: Insufficient samples collected.");
                    return;
                }
                float meanADC = (float)_sumADC / _sampleCount;
                float varADC = ((float)_sumADCSq / _sampleCount) - (meanADC * meanADC);
                float stdADC = sqrtf(max(0.0f, varADC));

                _profile.fsrFirmMeanADC = (uint16_t)meanADC;
                _profile.fsrFirmStdDev = stdADC;

                advanceStep(); // Move to validation
            }
            break;
        }

        case CALIB_STEP5_VALIDATION: {
            // Check dynamic range
            if (_profile.fsrFirmMeanADC <= _profile.fsrRelaxedMeanADC + 350) {
                failCalibration("Validation: Insufficient grip dynamic range (<350 ADC separation).");
                return;
            }

            // Check statistical separation ratio:
            // Sep = (Firm - Relaxed) / (StdFirm + StdRelaxed)
            float totalSpread = _profile.fsrFirmStdDev + _profile.fsrRelaxedStdDev;
            if (totalSpread < 1.0f) totalSpread = 1.0f; // Prevent div by zero
            float separationRatio = (float)(_profile.fsrFirmMeanADC - _profile.fsrRelaxedMeanADC) / totalSpread;
            _profile.gripSeparationRatio = separationRatio;

            if (separationRatio < Config::CalibrationDefaults::MIN_ACCEPTABLE_SEPARATION_RATIO) {
                failCalibration("Validation: Grip distributions overlap excessively. Squeeze firmer.");
                return;
            }

            // Derive personalized rep thresholds
            _profile.gripEligibilityADC = _profile.fsrRelaxedMeanADC + 
                (uint16_t)((_profile.fsrFirmMeanADC - _profile.fsrRelaxedMeanADC) * Config::RepEngineDefaults::MIN_GRIP_ELIGIBILITY_RATIO);
            _profile.postureToleranceDeg = 15.0f;
            _profile.calibratedTimeMs = millis();
            _profile.isValid = true;

            // Save to persistent storage
            if (saveToNVS()) {
                _currentStep = CALIB_COMPLETED;
            } else {
                failCalibration("Validation: Failed to write calibration profile to NVS.");
            }
            break;
        }

        default:
            break;
    }
}

bool PersonalCalibration::saveToNVS() {
    _profile.magicHeader = MAGIC_HEADER;
    _profile.schemaVersion = SCHEMA_VERSION;

    // Calculate CRC32 excluding the crc32 field itself
    size_t payloadSize = sizeof(CalibrationProfile) - sizeof(uint32_t);
    _profile.crc32 = calculateCRC32((const uint8_t*)&_profile, payloadSize);

    if (!_prefs.begin("glove_cal", false)) {
        return false;
    }

    size_t bytesWritten = _prefs.putBytes("profile", &_profile, sizeof(CalibrationProfile));
    _prefs.end();

    return (bytesWritten == sizeof(CalibrationProfile));
}

bool PersonalCalibration::loadFromNVS() {
    if (!_prefs.begin("glove_cal", true)) { // Read-only
        return false;
    }

    CalibrationProfile tempProfile;
    size_t bytesRead = _prefs.getBytes("profile", &tempProfile, sizeof(CalibrationProfile));
    _prefs.end();

    if (bytesRead != sizeof(CalibrationProfile)) {
        return false;
    }

    // Verify Magic and Version
    if (tempProfile.magicHeader != MAGIC_HEADER || tempProfile.schemaVersion != SCHEMA_VERSION) {
        return false;
    }

    // Verify CRC32
    size_t payloadSize = sizeof(CalibrationProfile) - sizeof(uint32_t);
    uint32_t calculatedCrc = calculateCRC32((const uint8_t*)&tempProfile, payloadSize);
    if (calculatedCrc != tempProfile.crc32) {
        return false;
    }

    _profile = tempProfile;
    return _profile.isValid;
}
