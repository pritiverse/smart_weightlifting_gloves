#include "Config.h"
#include "Calibration.h"

static constexpr uint32_t MAGIC_HEADER = 0x47524950; // "GRIP"
static constexpr uint16_t SCHEMA_VERSION = 2;        // Bumped to v2 for gravity vector schema

PersonalCalibration::PersonalCalibration()
    : _currentStep(CALIB_IDLE),
      _stepStartTimeMs(0),
      _consecutiveBadSamples(0) {
    memset(_failureReason, 0, sizeof(_failureReason));
    resetAccumulators();
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

    // Default neutral gravity reference vector points along +Z in sensor coordinates
    _profile.neutralG[0] = 0.0f;
    _profile.neutralG[1] = 0.0f;
    _profile.neutralG[2] = 1.0f;
    _profile.angularSpreadDeg = 1.0f;

    _profile.fsrRelaxedMeanADC = Config::CalibrationDefaults::FALLBACK_FSR_RELAXED_ADC;
    _profile.fsrRelaxedStdDev = 30.0f;
    _profile.fsrFirmMeanADC = Config::CalibrationDefaults::FALLBACK_FSR_FIRM_ADC;
    _profile.fsrFirmStdDev = 60.0f;
    _profile.gripSeparationRatio = 20.0f;

    _profile.gripEligibilityADC = _profile.fsrRelaxedMeanADC + 
        (uint16_t)((_profile.fsrFirmMeanADC - _profile.fsrRelaxedMeanADC) * Config::RepEngineDefaults::MIN_GRIP_ELIGIBILITY_RATIO);
    _profile.postureToleranceDeg = 15.0f;

    _profile.isValid = false;
    _profile.crc32 = 0;

    _stagingProfile = _profile;
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
        case CALIB_IDLE:                return "IDLE";
        case CALIB_PREP_STEP1_REST:     return "PREP_REST_ARM";
        case CALIB_STEP1_PREFLIGHT:     return "PREFLIGHT_CHECK";
        case CALIB_PREP_STEP2_POSTURE:  return "PREP_START_POSITION";
        case CALIB_STEP2_NEUTRAL_WRIST: return "NEUTRAL_WRIST_HOLD";
        case CALIB_PREP_STEP3_RELAXED:  return "PREP_RELAX_GRIP";
        case CALIB_STEP3_RELAXED_GRIP:  return "RELAXED_GRIP_BASELINE";
        case CALIB_PREP_STEP4_FIRM:     return "PREP_FIRM_GRIP";
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

    if (_currentStep == CALIB_PREP_STEP1_REST) {
        targetDuration = Config::CalibrationDefaults::PREP_STEP1_REST_MS;
    } else if (_currentStep == CALIB_PREP_STEP2_POSTURE) {
        targetDuration = Config::CalibrationDefaults::PREP_STEP2_POSTURE_MS;
    } else if (_currentStep == CALIB_PREP_STEP3_RELAXED) {
        targetDuration = Config::CalibrationDefaults::PREP_STEP3_RELAXED_MS;
    } else if (_currentStep == CALIB_PREP_STEP4_FIRM) {
        targetDuration = Config::CalibrationDefaults::PREP_STEP4_FIRM_MS;
    } else if (_currentStep == CALIB_STEP1_PREFLIGHT) {
        targetDuration = Config::CalibrationDefaults::PREFLIGHT_RECORD_MS;
    } else if (_currentStep == CALIB_STEP2_NEUTRAL_WRIST) {
        targetDuration = Config::CalibrationDefaults::NEUTRAL_RECORD_DURATION_MS;
    } else if (_currentStep == CALIB_STEP3_RELAXED_GRIP || _currentStep == CALIB_STEP4_FIRM_GRIP) {
        targetDuration = Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS;
    }

    uint32_t pct = (elapsed * 100) / targetDuration;
    return (uint8_t)constrain(pct, 0, 100);
}

void PersonalCalibration::resetAccumulators() {
    _stepStartTimeMs = millis();
    _consecutiveBadSamples = 0;
    _gxAccumulator.reset();
    _gyAccumulator.reset();
    _gzAccumulator.reset();
    _adcAccumulator.reset();
}

void PersonalCalibration::startWizard() {
    _profile.isValid = false;
    _stagingProfile.isValid = false;
    resetAccumulators();
    memset(_failureReason, 0, sizeof(_failureReason));
    _currentStep = CALIB_PREP_STEP1_REST;
}

void PersonalCalibration::advanceStep() {
    resetAccumulators();
    switch (_currentStep) {
        case CALIB_PREP_STEP1_REST:
            _currentStep = CALIB_STEP1_PREFLIGHT;
            break;
        case CALIB_STEP1_PREFLIGHT:
            _currentStep = CALIB_PREP_STEP2_POSTURE;
            break;
        case CALIB_PREP_STEP2_POSTURE:
            _currentStep = CALIB_STEP2_NEUTRAL_WRIST;
            break;
        case CALIB_STEP2_NEUTRAL_WRIST:
            _currentStep = CALIB_PREP_STEP3_RELAXED;
            break;
        case CALIB_PREP_STEP3_RELAXED:
            _currentStep = CALIB_STEP3_RELAXED_GRIP;
            break;
        case CALIB_STEP3_RELAXED_GRIP:
            _currentStep = CALIB_PREP_STEP4_FIRM;
            break;
        case CALIB_PREP_STEP4_FIRM:
            _currentStep = CALIB_STEP4_FIRM_GRIP;
            break;
        case CALIB_STEP4_FIRM_GRIP:
            _currentStep = CALIB_STEP5_VALIDATION;
            break;
        default:
            break;
    }
}

void PersonalCalibration::confirmReady() {
    // Only advance if currently in a PREP state waiting for user
    if (_currentStep == CALIB_PREP_STEP1_REST ||
        _currentStep == CALIB_PREP_STEP2_POSTURE ||
        _currentStep == CALIB_PREP_STEP3_RELAXED ||
        _currentStep == CALIB_PREP_STEP4_FIRM) {
        advanceStep();
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
        case CALIB_PREP_STEP1_REST: {
            // User-paced: wait for explicit user confirmation (90s safety timeout)
            if (elapsed >= 90000) {
                failCalibration("Preflight: Timed out waiting for ready confirmation.");
            }
            break;
        }

        case CALIB_PREP_STEP2_POSTURE: {
            // User-paced: wait for explicit user confirmation (90s safety timeout)
            if (elapsed >= 90000) {
                failCalibration("Neutral hold: Timed out waiting for ready confirmation.");
            }
            break;
        }

        case CALIB_PREP_STEP3_RELAXED: {
            // User-paced: wait for explicit user confirmation (90s safety timeout)
            if (elapsed >= 90000) {
                failCalibration("Relaxed grip: Timed out waiting for ready confirmation.");
            }
            break;
        }

        case CALIB_PREP_STEP4_FIRM: {
            // User-paced: wait for explicit user confirmation (90s safety timeout)
            if (elapsed >= 90000) {
                failCalibration("Firm grip: Timed out waiting for ready confirmation.");
            }
            break;
        }

        case CALIB_STEP1_PREFLIGHT: {
            // Broadened tolerance to permit natural arm placement without hair-trigger abort
            bool isBad = (raw.fsrVoltage > 2.85f) || 
                         (proc.accelMagnitude_mps2 < 4.0f || proc.accelMagnitude_mps2 > 20.0f);

            if (isBad) {
                _consecutiveBadSamples++;
                // Debounce to 25 samples (500 ms at 50 Hz) instead of hair-trigger 5 samples (100 ms)
                if (_consecutiveBadSamples >= 25) {
                    if (raw.fsrVoltage > 2.85f) {
                        failCalibration("Preflight: FSR under heavy pressure (>2.85V). Keep arm relaxed.");
                    } else {
                        failCalibration("Preflight: Excessive acceleration or freefall detected.");
                    }
                    return;
                }
            } else {
                _consecutiveBadSamples = 0;
            }

            if (elapsed >= Config::CalibrationDefaults::PREFLIGHT_RECORD_MS) {
                advanceStep();
            }
            break;
        }

        case CALIB_STEP2_NEUTRAL_WRIST: {
            // Accumulate 3D gravity vector components from Mahony filter
            _gxAccumulator.update(proc.gravityVector[0]);
            _gyAccumulator.update(proc.gravityVector[1]);
            _gzAccumulator.update(proc.gravityVector[2]);

            if (elapsed >= Config::CalibrationDefaults::NEUTRAL_RECORD_DURATION_MS) {
                if (_gxAccumulator.count < 50) {
                    failCalibration("Neutral hold: Insufficient samples collected.");
                    return;
                }
                float meanGx = (float)_gxAccumulator.getMean();
                float meanGy = (float)_gyAccumulator.getMean();
                float meanGz = (float)_gzAccumulator.getMean();

                float mag = sqrtf(meanGx * meanGx + meanGy * meanGy + meanGz * meanGz);
                if (mag < 0.1f) {
                    failCalibration("Neutral hold: Degenerate gravity vector.");
                    return;
                }

                // Compute angular spread / stability in degrees
                float stdGx = (float)_gxAccumulator.getStdDev();
                float stdGy = (float)_gyAccumulator.getStdDev();
                float stdGz = (float)_gzAccumulator.getStdDev();
                float angularSpreadDeg = sqrtf(stdGx * stdGx + stdGy * stdGy + stdGz * stdGz) * (180.0f / M_PI);

                // Allow realistic human postural tremor (up to 16 deg spread over 3s hold with weight)
                if (angularSpreadDeg > 16.0f) {
                    failCalibration("Neutral hold: Excessive wrist movement (>16 deg spread).");
                    return;
                }

                // Normalize neutral gravity reference vector
                _stagingProfile.neutralG[0] = meanGx / mag;
                _stagingProfile.neutralG[1] = meanGy / mag;
                _stagingProfile.neutralG[2] = meanGz / mag;
                _stagingProfile.angularSpreadDeg = angularSpreadDeg;

                advanceStep(); // Move to relaxed grip prep
            }
            break;
        }

        case CALIB_STEP3_RELAXED_GRIP: {
            _adcAccumulator.update((double)raw.fsrRawADC);

            if (elapsed >= Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS) {
                if (_adcAccumulator.count < 40) {
                    failCalibration("Relaxed grip: Insufficient samples collected.");
                    return;
                }
                float meanADC = (float)_adcAccumulator.getMean();
                float stdADC  = (float)_adcAccumulator.getStdDev();

                // Tolerant baseline for snug glove fit or dumbbell resting in palm (permits real dumbbell hold)
                if (meanADC > 3400.0f) {
                    failCalibration("Relaxed grip: Baseline too high (>3400 ADC). Rest fingers lightly.");
                    return;
                }

                // Account for ESP32 ADC noise (~30-60 counts) plus physiological tremor holding dumbbell
                if (stdADC > 750.0f) {
                    failCalibration("Relaxed grip: Excessive variance (>750 ADC std). Keep fingers steady.");
                    return;
                }

                _stagingProfile.fsrRelaxedMeanADC = (uint16_t)meanADC;
                _stagingProfile.fsrRelaxedStdDev = stdADC;

                advanceStep(); // Move to firm grip prep
            }
            break;
        }

        case CALIB_STEP4_FIRM_GRIP: {
            _adcAccumulator.update((double)raw.fsrRawADC);

            if (elapsed >= Config::CalibrationDefaults::GRIP_RECORD_DURATION_MS) {
                if (_adcAccumulator.count < 40) {
                    failCalibration("Firm grip: Insufficient samples collected.");
                    return;
                }
                float meanADC = (float)_adcAccumulator.getMean();
                float stdADC  = (float)_adcAccumulator.getStdDev();

                // Check minimum effort
                if (meanADC < 600.0f) {
                    failCalibration("Firm grip: Squeeze too weak (<600 ADC). Squeeze firmly.");
                    return;
                }

                // Saturation at full voluntary contraction is NORMAL - clamp rather than fail!
                if (meanADC > 4050.0f) {
                    meanADC = 4050.0f;
                }

                _stagingProfile.fsrFirmMeanADC = (uint16_t)meanADC;
                _stagingProfile.fsrFirmStdDev = stdADC;

                advanceStep(); // Move to statistical validation
            }
            break;
        }

        case CALIB_STEP5_VALIDATION: {
            // 1. Dynamic range check (require at least 200 ADC separation)
            if (_stagingProfile.fsrFirmMeanADC <= _stagingProfile.fsrRelaxedMeanADC + 200) {
                failCalibration("Validation: Insufficient grip dynamic range (<200 ADC separation).");
                return;
            }

            // 2. Firm-hold Coefficient of Variation (CV = std / mean < 0.45)
            // Allows natural grip force ramping without falsely rejecting
            float firmCV = (_stagingProfile.fsrFirmMeanADC > 0) ? 
                           (_stagingProfile.fsrFirmStdDev / (float)_stagingProfile.fsrFirmMeanADC) : 1.0f;
            if (firmCV >= 0.45f) {
                failCalibration("Validation: Squeeze force erratic (CV >= 0.45). Maintain steady hold.");
                return;
            }

            // 3. Statistical separation ratio
            float totalSpread = _stagingProfile.fsrFirmStdDev + _stagingProfile.fsrRelaxedStdDev;
            if (totalSpread < 1.0f) totalSpread = 1.0f;
            float separationRatio = (float)(_stagingProfile.fsrFirmMeanADC - _stagingProfile.fsrRelaxedMeanADC) / totalSpread;
            _stagingProfile.gripSeparationRatio = separationRatio;

            if (separationRatio < Config::CalibrationDefaults::MIN_ACCEPTABLE_SEPARATION_RATIO) {
                failCalibration("Validation: Grip distributions overlap excessively. Squeeze firmer.");
                return;
            }

            // 4. Derive personalized rep thresholds
            _stagingProfile.gripEligibilityADC = _stagingProfile.fsrRelaxedMeanADC + 
                (uint16_t)((_stagingProfile.fsrFirmMeanADC - _stagingProfile.fsrRelaxedMeanADC) * Config::RepEngineDefaults::MIN_GRIP_ELIGIBILITY_RATIO);
            _stagingProfile.postureToleranceDeg = 15.0f;
            _stagingProfile.calibratedTimeMs = millis();
            _stagingProfile.isValid = true;
            _stagingProfile.magicHeader = MAGIC_HEADER;
            _stagingProfile.schemaVersion = SCHEMA_VERSION;

            size_t payloadSize = sizeof(CalibrationProfile) - sizeof(uint32_t);
            _stagingProfile.crc32 = calculateCRC32((const uint8_t*)&_stagingProfile, payloadSize);

            // 5. Commit to active profile ONLY after persistent NVS save succeeds
            bool nvsSuccess = false;
#if defined(ARDUINO_ARCH_ESP32)
            if (_prefs.begin("glove_cal", false)) {
                size_t bytesWritten = _prefs.putBytes("profile", &_stagingProfile, sizeof(CalibrationProfile));
                _prefs.end();
                nvsSuccess = (bytesWritten == sizeof(CalibrationProfile));
            }
#else
            nvsSuccess = true;
#endif

            if (nvsSuccess) {
                _profile = _stagingProfile;
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
#if defined(ARDUINO_ARCH_ESP32)
    _profile.magicHeader = MAGIC_HEADER;
    _profile.schemaVersion = SCHEMA_VERSION;

    size_t payloadSize = sizeof(CalibrationProfile) - sizeof(uint32_t);
    _profile.crc32 = calculateCRC32((const uint8_t*)&_profile, payloadSize);

    if (!_prefs.begin("glove_cal", false)) {
        return false;
    }

    size_t bytesWritten = _prefs.putBytes("profile", &_profile, sizeof(CalibrationProfile));
    _prefs.end();

    return (bytesWritten == sizeof(CalibrationProfile));
#else
    return true;
#endif
}

bool PersonalCalibration::loadFromNVS() {
#if defined(ARDUINO_ARCH_ESP32)
    if (!_prefs.begin("glove_cal", true)) {
        return false;
    }

    CalibrationProfile tempProfile;
    size_t bytesRead = _prefs.getBytes("profile", &tempProfile, sizeof(CalibrationProfile));
    _prefs.end();

    if (bytesRead != sizeof(CalibrationProfile)) {
        return false;
    }

    // Verify Magic and Version (Schema Version must be strictly 2)
    if (tempProfile.magicHeader != MAGIC_HEADER || tempProfile.schemaVersion != SCHEMA_VERSION) {
        return false; // Rejects old schema v1 blobs cleanly
    }

    // Verify CRC32
    size_t payloadSize = sizeof(CalibrationProfile) - sizeof(uint32_t);
    uint32_t calculatedCrc = calculateCRC32((const uint8_t*)&tempProfile, payloadSize);
    if (calculatedCrc != tempProfile.crc32) {
        return false;
    }

    _profile = tempProfile;
    return _profile.isValid;
#else
    return false;
#endif
}
