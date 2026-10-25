#include "Config.h"
#include "RepEngine.h"

RepetitionEngine::RepetitionEngine()
    : _currentState(REP_STATE_IDLE),
      _repStartTimeMs(0),
      _concentricStartTimeMs(0),
      _transitionStartTimeMs(0),
      _lastRepEndTimeMs(0),
      _startPitchDeg(0.0f),
      _startRollDeg(0.0f),
      _peakExcursionDeg(0.0f),
      _gripAccumulator(0.0f),
      _gripSampleCount(0),
      _sustainedLowGripSamples(0),
      _gripFailedDuringConcentric(false),
      _sensorFaultDuringRep(false) {
    resetCounters();
    setExerciseProfile(EXERCISE_BICEP_CURL);
}

void RepetitionEngine::init() {
    _currentState = REP_STATE_IDLE;
    _sustainedLowGripSamples = 0;
    resetCounters();
    setExerciseProfile(EXERCISE_BICEP_CURL);
}

void RepetitionEngine::resetCounters() {
    _stats.totalValidReps = 0;
    _stats.totalRejectedReps = 0;
    _stats.acceptanceRatioPct = 100.0f;
    _stats.lastRepDurationMs = 0;
    _stats.lastRepPeakRomDeg = 0.0f;
    _stats.lastRepAvgGripRatio = 0.0f;
    _stats.lastRejectionReason = REJECT_NONE;
    _stats.lastRepTimestampMs = 0;
    _stats.currentExcursionDeg = 0.0f;
    _stats.currentRepElapsedMs = 0;
    _stats.currentRepMeanGrip = 0.0f;
}

void RepetitionEngine::setExerciseProfile(ExerciseType type) {
    _activeProfile.type = type;
    switch (type) {
        case EXERCISE_BICEP_CURL:
            strncpy(_activeProfile.name, "Bicep Curl", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 40.0f;          // 40 degrees elbow flexion
            _activeProfile.minDurationMs = 700;        // Minimum 700 ms (rejects ballistic jolts)
            _activeProfile.maxDurationMs = 5000;       // Maximum 5.0 seconds
            _activeProfile.minGripRatio = 0.35f;       // 35% calibrated firm grip required
            _activeProfile.returnToleranceDeg = 14.0f; // Must return within 14 deg of starting posture
            break;

        case EXERCISE_WRIST_EXTENSION:
            strncpy(_activeProfile.name, "Wrist Extension", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 25.0f;          // 25 degrees wrist extension
            _activeProfile.minDurationMs = 500;
            _activeProfile.maxDurationMs = 4000;
            _activeProfile.minGripRatio = 0.40f;
            _activeProfile.returnToleranceDeg = 10.0f;
            break;

        case EXERCISE_DUMBBELL_PRESS:
            strncpy(_activeProfile.name, "Dumbbell Press", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 45.0f;          // 45 degrees vertical travel
            _activeProfile.minDurationMs = 800;
            _activeProfile.maxDurationMs = 6000;
            _activeProfile.minGripRatio = 0.45f;
            _activeProfile.returnToleranceDeg = 15.0f;
            break;
    }
}

const char* RepetitionEngine::getStateName() const {
    switch (_currentState) {
        case REP_STATE_IDLE:                   return "IDLE";
        case REP_STATE_READY:                  return "READY";
        case REP_STATE_CONCENTRIC_OR_OUTBOUND: return "CONCENTRIC";
        case REP_STATE_TRANSITION:             return "TRANSITION";
        case REP_STATE_ECCENTRIC_OR_RETURN:    return "ECCENTRIC";
        case REP_STATE_REP_CANDIDATE:          return "EVALUATING";
        case REP_STATE_VALID_REP:              return "VALID_REP";
        case REP_STATE_REJECTED_REP:           return "REJECTED_REP";
        default:                               return "UNKNOWN";
    }
}

const char* RepetitionEngine::getRejectionReasonName(RejectionReason reason) const {
    switch (reason) {
        case REJECT_NONE:               return "NONE (VALID)";
        case REJECT_INSUFFICIENT_ROM:   return "INSUFFICIENT_RANGE_OF_MOTION";
        case REJECT_DURATION_TOO_SHORT: return "DURATION_TOO_SHORT (BALLISTIC_SPIKE)";
        case REJECT_DURATION_TOO_LONG:  return "DURATION_TOO_LONG (STALLED_REP)";
        case REJECT_INSUFFICIENT_GRIP:  return "INSUFFICIENT_GRIP_PRESSURE";
        case REJECT_INCOMPLETE_RETURN:  return "INCOMPLETE_RETURN_TO_START";
        case REJECT_SENSOR_UNRELIABLE:  return "SENSOR_UNRELIABLE_DURING_REP";
        default:                        return "UNKNOWN";
    }
}

float RepetitionEngine::calculateExcursion(const ProcessedSensorData& proc, const CalibrationProfile& calib) const {
    // Primary exercise axis excursion relative to calibrated neutral posture
    float refPitch = calib.isValid ? calib.neutralPitchDeg : 0.0f;
    float refRoll  = calib.isValid ? calib.neutralRollDeg : 0.0f;

    if (_activeProfile.type == EXERCISE_BICEP_CURL) {
        // Forearm pitch upward excursion
        return fabsf(proc.pitchDeg - refPitch);
    } else if (_activeProfile.type == EXERCISE_WRIST_EXTENSION) {
        return fabsf(proc.pitchDeg - refPitch);
    } else {
        // Multi-axis compound distance
        float dp = proc.pitchDeg - refPitch;
        float dr = proc.rollDeg - refRoll;
        return sqrtf(dp * dp + dr * dr);
    }
}

void RepetitionEngine::transitionTo(RepState nextState) {
    _currentState = nextState;
}

void RepetitionEngine::finalizeRep(bool isValid, RejectionReason reason, uint32_t durationMs, float peakRom, float avgGrip) {
    _stats.lastRepDurationMs = durationMs;
    _stats.lastRepPeakRomDeg = peakRom;
    _stats.lastRepAvgGripRatio = avgGrip;
    _stats.lastRejectionReason = reason;
    _stats.lastRepTimestampMs = millis();
    _lastRepEndTimeMs = millis();

    if (isValid) {
        _stats.totalValidReps++;
        transitionTo(REP_STATE_VALID_REP);
    } else {
        _stats.totalRejectedReps++;
        transitionTo(REP_STATE_REJECTED_REP);
    }

    uint32_t totalAttempts = _stats.totalValidReps + _stats.totalRejectedReps;
    if (totalAttempts > 0) {
        _stats.acceptanceRatioPct = ((float)_stats.totalValidReps / totalAttempts) * 100.0f;
    }
}

void RepetitionEngine::update(const ProcessedSensorData& proc, const CalibrationProfile& calib, bool sensorHealthOk) {
    uint32_t nowMs = millis();
    float excursion = calculateExcursion(proc, calib);
    _stats.currentExcursionDeg = excursion;

    if (!sensorHealthOk) {
        _sensorFaultDuringRep = true;
    }

    // Handle momentary latch states
    if (_currentState == REP_STATE_VALID_REP || _currentState == REP_STATE_REJECTED_REP) {
        transitionTo(REP_STATE_IDLE);
        return;
    }

    // Refractory dead-time protection
    if (nowMs - _lastRepEndTimeMs < Config::RepEngineDefaults::REFRACTORY_PERIOD_MS) {
        transitionTo(REP_STATE_IDLE);
        return;
    }

    switch (_currentState) {
        case REP_STATE_IDLE: {
            _stats.currentRepElapsedMs = 0;
            _stats.currentRepMeanGrip = 0.0f;

            // Check if user is within starting posture tolerance
            if (excursion <= _activeProfile.returnToleranceDeg) {
                transitionTo(REP_STATE_READY);
            }
            break;
        }

        case REP_STATE_READY: {
            // Movement Onset Detection: Excursion begins expanding beyond threshold with angular velocity
            if (excursion > 12.0f && proc.gyroMagnitude_dps > 18.0f) {
                _repStartTimeMs = nowMs;
                _concentricStartTimeMs = nowMs;
                _startPitchDeg = proc.pitchDeg;
                _startRollDeg = proc.rollDeg;
                _peakExcursionDeg = excursion;

                _gripAccumulator = proc.normalizedGrip;
                _gripSampleCount = 1;
                _sustainedLowGripSamples = (proc.normalizedGrip < _activeProfile.minGripRatio) ? 1 : 0;
                _gripFailedDuringConcentric = false;
                _sensorFaultDuringRep = !sensorHealthOk;

                transitionTo(REP_STATE_CONCENTRIC_OR_OUTBOUND);
            } else if (excursion > _activeProfile.returnToleranceDeg + 8.0f) {
                // User drifted away without moving dynamically
                transitionTo(REP_STATE_IDLE);
            }
            break;
        }

        case REP_STATE_CONCENTRIC_OR_OUTBOUND: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            if (excursion > _peakExcursionDeg) {
                _peakExcursionDeg = excursion;
            }

            // Accumulate grip metrics
            _gripAccumulator += proc.normalizedGrip;
            _gripSampleCount++;
            _stats.currentRepMeanGrip = _gripAccumulator / _gripSampleCount;

            // Monitor grip eligibility during concentric phase with debounce confirmation window (15 samples = 300 ms)
            if (proc.normalizedGrip < _activeProfile.minGripRatio) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    _gripFailedDuringConcentric = true;
                }
            } else {
                // Reset debounce counter if grip returns above threshold
                _sustainedLowGripSamples = 0;
            }

            // Detect peak transition: Gyro velocity drops below threshold or reverses
            if (excursion >= 22.0f && proc.gyroMagnitude_dps < 12.0f) {
                _transitionStartTimeMs = nowMs;
                transitionTo(REP_STATE_TRANSITION);
            }

            // Timeout check
            if (elapsed > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _peakExcursionDeg, _stats.currentRepMeanGrip);
            }
            break;
        }

        case REP_STATE_TRANSITION: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            if (excursion > _peakExcursionDeg) {
                _peakExcursionDeg = excursion;
            }

            // Detect return onset: excursion drops at least 4 degrees below peak
            if (excursion < (_peakExcursionDeg - 4.0f)) {
                transitionTo(REP_STATE_ECCENTRIC_OR_RETURN);
            }

            // Stalled in transition check
            if (nowMs - _transitionStartTimeMs > 2500 || elapsed > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _peakExcursionDeg, _stats.currentRepMeanGrip);
            }
            break;
        }

        case REP_STATE_ECCENTRIC_OR_RETURN: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            // Accumulate grip during eccentric phase
            _gripAccumulator += proc.normalizedGrip;
            _gripSampleCount++;
            _stats.currentRepMeanGrip = _gripAccumulator / _gripSampleCount;

            // Return to neutral starting region
            if (excursion <= _activeProfile.returnToleranceDeg) {
                transitionTo(REP_STATE_REP_CANDIDATE);
            }

            // Timeout check
            if (elapsed > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _peakExcursionDeg, _stats.currentRepMeanGrip);
            }
            break;
        }

        case REP_STATE_REP_CANDIDATE: {
            uint32_t totalDuration = nowMs - _repStartTimeMs;
            float avgGrip = (_gripSampleCount > 0) ? (_gripAccumulator / _gripSampleCount) : 0.0f;

            // 1. Sensor Reliability Rule
            if (_sensorFaultDuringRep) {
                finalizeRep(false, REJECT_SENSOR_UNRELIABLE, totalDuration, _peakExcursionDeg, avgGrip);
                return;
            }

            // 2. Minimum Duration Rule (Ballistic filter)
            if (totalDuration < _activeProfile.minDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_SHORT, totalDuration, _peakExcursionDeg, avgGrip);
                return;
            }

            // 3. Maximum Duration Rule (Stalled rep)
            if (totalDuration > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, totalDuration, _peakExcursionDeg, avgGrip);
                return;
            }

            // 4. Range of Motion Rule (Incomplete rep)
            if (_peakExcursionDeg < _activeProfile.minRomDeg) {
                finalizeRep(false, REJECT_INSUFFICIENT_ROM, totalDuration, _peakExcursionDeg, avgGrip);
                return;
            }

            // 5. Grip Eligibility Rule (Unloaded / dropped bar)
            // Rejects if grip was sustained low (>= 300 ms) during concentric ascent, or mean rep grip is severely deficient
            if (_gripFailedDuringConcentric || avgGrip < (_activeProfile.minGripRatio * 0.70f)) {
                finalizeRep(false, REJECT_INSUFFICIENT_GRIP, totalDuration, _peakExcursionDeg, avgGrip);
                return;
            }

            // 6. Complete Valid Repetition!
            finalizeRep(true, REJECT_NONE, totalDuration, _peakExcursionDeg, avgGrip);
            break;
        }

        default:
            break;
    }
}
