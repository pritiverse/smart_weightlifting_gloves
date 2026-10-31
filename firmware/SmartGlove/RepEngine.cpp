#include "Config.h"
#include "RepEngine.h"

RepetitionEngine::RepetitionEngine()
    : _currentState(REP_STATE_IDLE),
      _bottomValleyDeg(0.0f),
      _returnValleyDeg(0.0f),
      _peakExcursionDeg(0.0f),
      _ascentProminence(0.0f),
      _descentProminence(0.0f),
      _dominantAxisIndex(1), // Default to Y axis for elbow flexion
      _dominantSign(1),
      _dominantAxisLocked(false),
      _minGrip(1.0f),
      _gripAccumulator(0.0f),
      _gripSampleCount(0),
      _sustainedLowGripSamples(0),
      _gripFailedDuringRep(false),
      _sensorFaultDuringRep(false),
      _repStartTimeMs(0),
      _concentricStartTimeMs(0),
      _transitionStartTimeMs(0),
      _lastRepEndTimeMs(0),
      _disarmedSamplesCount(0),
      _confirmedSetStarted(false),
      _uncertainCount(0),
      _recentCount(0),
      _recentHead(0),
      _repHistorySampleCount(0),
      _fsrSlowEMA(0.0f),
      _prevFilteredGyroMag(0.0f),
      _concentricPeakGyro(0.0f),
      _concentricPeakGyroTimeMs(0),
      _initialPeakGyro(0.0f),
      _initialRepDuration(0),
      _initialMeanGrip(0.0f),
      _fatigueBaselineSet(false),
      _onsetDebounceCount(0),
      _peakDebounceCount(0),
      _returnOnsetDebounceCount(0),
      _returnDebounceCount(0) {
    _gyroEnergy[0] = 0.0f;
    _gyroEnergy[1] = 0.0f;
    _gyroEnergy[2] = 0.0f;

    _pendingRep1.populated = false;
    _pendingRep1.durationMs = 0;
    _pendingRep1.peakRomDeg = 0.0f;
    _pendingRep1.avgGrip = 0.0f;
    _pendingRep1.couplingRho = 0.0f;
    _pendingRep1.confidencePct = 0.0f;
    _pendingRep1.timestampMs = 0;

    for (uint8_t i = 0; i < 2; i++) {
        _uncertainReps[i].populated = false;
        _uncertainReps[i].durationMs = 0;
        _uncertainReps[i].peakRomDeg = 0.0f;
        _uncertainReps[i].avgGrip = 0.0f;
        _uncertainReps[i].couplingRho = 0.0f;
        _uncertainReps[i].confidencePct = 0.0f;
        _uncertainReps[i].timestampMs = 0;
    }

    for (uint8_t i = 0; i < 5; i++) {
        _recentPeakRom[i] = 0.0f;
        _recentDuration[i] = 0;
    }

    resetCounters();
    setExerciseProfile(EXERCISE_BICEP_CURL);
}

void RepetitionEngine::resetInFlightState() {
    _repStartTimeMs = 0;
    _concentricStartTimeMs = 0;
    _transitionStartTimeMs = 0;
    _peakExcursionDeg = _bottomValleyDeg;
    _ascentProminence = 0.0f;
    _descentProminence = 0.0f;
    _returnValleyDeg = _bottomValleyDeg;
    _minGrip = 1.0f;
    _gripAccumulator = 0.0f;
    _gripSampleCount = 0;
    _sustainedLowGripSamples = 0;
    _gripFailedDuringRep = false;
    _sensorFaultDuringRep = false;
    _repHistorySampleCount = 0;
    _concentricPeakGyro = 0.0f;
    _concentricPeakGyroTimeMs = 0;
    _onsetDebounceCount = 0;
    _peakDebounceCount = 0;
    _returnOnsetDebounceCount = 0;
    _returnDebounceCount = 0;
}

void RepetitionEngine::init() {
    resetCounters();
    resetInFlightState();
    _confirmedSetStarted = false;
    _pendingRep1.populated = false;
    _uncertainCount = 0;
    _disarmedSamplesCount = 0;
    _recentCount = 0;
    _recentHead = 0;
    _dominantAxisLocked = false;
    _fatigueBaselineSet = false;
    _gyroEnergy[0] = 0.0f;
    _gyroEnergy[1] = 0.0f;
    _gyroEnergy[2] = 0.0f;

    setExerciseProfile(_activeProfile.type);
    transitionTo(REP_STATE_IDLE);
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
    _stats.adaptiveMinRomDeg = _activeProfile.minRomDeg;
    _stats.adaptiveMinDurationMs = _activeProfile.minDurationMs;
    _stats.adaptiveMaxDurationMs = _activeProfile.maxDurationMs;
    _stats.pendingCandidateCount = 0;
    _stats.isSetArmed = false;

    _stats.lastRepCouplingRho = 0.0f;
    _stats.lastRepConfidencePct = 100.0f;
    _stats.lastRepWasUncertain = false;
    _stats.isFatigued = false;
    _stats.fatigueIndexPct = 0.0f;
}

void RepetitionEngine::setExerciseProfile(ExerciseType type) {
    _activeProfile.type = type;
    switch (type) {
        case EXERCISE_BICEP_CURL:
            strncpy(_activeProfile.name, "Bicep Curl", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 40.0f;                   // 40 degrees elbow flexion
            _activeProfile.minDurationMs = 700;                 // Minimum 700 ms (rejects ballistic jolts)
            _activeProfile.maxDurationMs = 5000;                // Maximum 5.0 seconds
            _activeProfile.minGripRatio = 0.35f;                // Fallback grip ratio
            _activeProfile.returnToleranceDeg = 22.0f;          // Return tolerance window
            _activeProfile.onsetExcursionDeg = 12.0f;
            _activeProfile.onsetGyroDps = 18.0f;
            _activeProfile.peakTransExcursionDeg = 22.0f;
            _activeProfile.peakTransGyroDps = 14.0f;
            _activeProfile.returnOnsetDeltaDeg = 4.0f;
            _activeProfile.concentricAbortExcursionDeg = 8.0f;
            _activeProfile.concentricAbortPeakDeg = 22.0f;
            _activeProfile.transitionStallGyroDps = 14.0f;
            _activeProfile.sustainedGripMinExcursionDeg = 20.0f;
            _activeProfile.returnSettledExcursionDeg = 30.0f;
            _activeProfile.returnSettledGyroDps = 12.0f;
            _dominantAxisIndex = 1;                             // Gyro Y axis
            break;

        case EXERCISE_WRIST_EXTENSION:
            strncpy(_activeProfile.name, "Wrist Extension", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 25.0f;                   // 25 degrees wrist extension
            _activeProfile.minDurationMs = 500;
            _activeProfile.maxDurationMs = 4000;
            _activeProfile.minGripRatio = 0.40f;
            _activeProfile.returnToleranceDeg = 16.0f;
            _activeProfile.onsetExcursionDeg = 8.0f;
            _activeProfile.onsetGyroDps = 15.0f;
            _activeProfile.peakTransExcursionDeg = 15.0f;
            _activeProfile.peakTransGyroDps = 12.0f;
            _activeProfile.returnOnsetDeltaDeg = 3.0f;
            _activeProfile.concentricAbortExcursionDeg = 5.0f;
            _activeProfile.concentricAbortPeakDeg = 15.0f;
            _activeProfile.transitionStallGyroDps = 12.0f;
            _activeProfile.sustainedGripMinExcursionDeg = 12.0f;
            _activeProfile.returnSettledExcursionDeg = 18.0f;
            _activeProfile.returnSettledGyroDps = 10.0f;
            _dominantAxisIndex = 0;                             // Gyro X axis
            break;

        case EXERCISE_DUMBBELL_PRESS:
            strncpy(_activeProfile.name, "Dumbbell Press", sizeof(_activeProfile.name));
            _activeProfile.minRomDeg = 45.0f;                   // 45 degrees vertical travel
            _activeProfile.minDurationMs = 800;
            _activeProfile.maxDurationMs = 6000;
            _activeProfile.minGripRatio = 0.45f;
            _activeProfile.returnToleranceDeg = 22.0f;
            _activeProfile.onsetExcursionDeg = 15.0f;
            _activeProfile.onsetGyroDps = 18.0f;
            _activeProfile.peakTransExcursionDeg = 25.0f;
            _activeProfile.peakTransGyroDps = 15.0f;
            _activeProfile.returnOnsetDeltaDeg = 5.0f;
            _activeProfile.concentricAbortExcursionDeg = 10.0f;
            _activeProfile.concentricAbortPeakDeg = 25.0f;
            _activeProfile.transitionStallGyroDps = 15.0f;
            _activeProfile.sustainedGripMinExcursionDeg = 20.0f;
            _activeProfile.returnSettledExcursionDeg = 32.0f;
            _activeProfile.returnSettledGyroDps = 12.0f;
            _dominantAxisIndex = 1;                             // Gyro Y axis
            break;
    }

    _stats.adaptiveMinRomDeg = _activeProfile.minRomDeg;
    _stats.adaptiveMinDurationMs = _activeProfile.minDurationMs;
    _stats.adaptiveMaxDurationMs = _activeProfile.maxDurationMs;

    resetInFlightState();
    transitionTo(REP_STATE_IDLE);
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
        case REJECT_UNLOADED:           return "UNLOADED_MOVEMENT (MIMED_OR_DROPPED_WEIGHT)";
        case REJECT_MOMENTUM_SWING:     return "MOMENTUM_SWING (EXCESSIVE_BODY_MOMENTUM)";
        default:                        return "UNKNOWN";
    }
}

float RepetitionEngine::calculateExcursion(const ProcessedSensorData& proc, const CalibrationProfile& calib) {
    float ngx = calib.neutralG[0];
    float ngy = calib.neutralG[1];
    float ngz = calib.neutralG[2];
    float magSq = ngx * ngx + ngy * ngy + ngz * ngz;
    if (magSq < 0.1f) {
        ngx = 0.0f; ngy = 0.0f; ngz = 1.0f;
    }
    float dot = proc.gravityVector[0] * ngx +
                proc.gravityVector[1] * ngy +
                proc.gravityVector[2] * ngz;
    dot = constrain(dot, -1.0f, 1.0f);
    float unsignedTilt = acosf(dot) * (180.0f / M_PI);

    if (_dominantAxisLocked) {
        return (float)_dominantSign * unsignedTilt;
    }
    return unsignedTilt;
}

void RepetitionEngine::transitionTo(RepState nextState) {
    _currentState = nextState;
}

float RepetitionEngine::calculateGripLoadCoupling(uint16_t sampleCount, uint8_t& outBestLag) {
    outBestLag = 0;
    if (sampleCount > MAX_REP_SAMPLES) sampleCount = MAX_REP_SAMPLES;
    if (sampleCount < 20) return 0.0f; // Minimum 400 ms of data needed

    float sumFSR = 0.0f;
    float sumAccel = 0.0f;
    for (uint16_t i = 0; i < sampleCount; i++) {
        sumFSR += _fsrHighPassHistory[i];
        sumAccel += _gyroAccelHistory[i];
    }
    float meanFSR = sumFSR / (float)sampleCount;
    float meanAccel = sumAccel / (float)sampleCount;

    float varFSR = 0.0f;
    float varAccel = 0.0f;
    for (uint16_t i = 0; i < sampleCount; i++) {
        float df = _fsrHighPassHistory[i] - meanFSR;
        float da = _gyroAccelHistory[i] - meanAccel;
        varFSR += df * df;
        varAccel += da * da;
    }
    float denom = sqrtf(varFSR * varAccel);
    if (denom < 1e-4f) return 0.0f;

    // Cross-correlation over lag in [0, 7] samples (0 to 140 ms at 50 Hz)
    constexpr uint8_t MAX_LAG = 8;
    float maxRho = -1.0f;
    for (uint8_t tau = 0; tau < MAX_LAG && tau < sampleCount / 2; tau++) {
        float crossSum = 0.0f;
        uint16_t count = sampleCount - tau;
        for (uint16_t i = 0; i < count; i++) {
            float df = _fsrHighPassHistory[i + tau] - meanFSR;
            float da = _gyroAccelHistory[i] - meanAccel;
            crossSum += df * da;
        }
        float r = crossSum / denom;
        if (r > maxRho) {
            maxRho = r;
            outBestLag = tau;
        }
    }
    return constrain(maxRho, -1.0f, 1.0f);
}

float RepetitionEngine::evaluateRepConfidence(float ascentRom, uint32_t durationMs, float rho, float signalNoise, uint8_t bestLag) {
    // 1. Prominence score
    float romDelta = ascentRom - _activeProfile.minRomDeg;
    float zRom = constrain(0.5f + (romDelta / 30.0f), 0.0f, 1.0f);

    // 2. Duration score
    float durDelta = fabsf((float)durationMs - (float)(_activeProfile.minDurationMs + _activeProfile.maxDurationMs) * 0.5f);
    float zDur = constrain(1.0f - (durDelta / 3500.0f), 0.0f, 1.0f);

    // 3. Coupling score
    float zRho = constrain((rho + 0.15f) / 0.85f, 0.0f, 1.0f);

    // 4. Signal noise score
    float zNoise = 1.0f - constrain(signalNoise / 1.5f, 0.0f, 0.4f);

    // 5. Phase agreement: best lag in [1, 5] samples (20 - 100 ms physiological latency)
    float zPhase = (bestLag >= 1 && bestLag <= 5) ? 1.0f : 0.65f;

    // Fused confidence score [0 - 100%]
    float confidence = (0.30f * zRho + 0.25f * zRom + 0.20f * zDur + 0.15f * zPhase + 0.10f * zNoise) * 100.0f;
    return constrain(confidence, 0.0f, 100.0f);
}

void RepetitionEngine::updateFatigueMetrics(float peakGyro, uint32_t durationMs, float avgGrip) {
    if (!_fatigueBaselineSet) {
        _initialPeakGyro = peakGyro;
        _initialRepDuration = durationMs;
        _initialMeanGrip = avgGrip;
        _fatigueBaselineSet = true;
        _stats.isFatigued = false;
        _stats.fatigueIndexPct = 0.0f;
        return;
    }

    // 1. Velocity loss: peak gyro drops relative to rep 1
    float velocityLossPct = 0.0f;
    if (_initialPeakGyro > 10.0f && peakGyro < _initialPeakGyro) {
        velocityLossPct = ((_initialPeakGyro - peakGyro) / _initialPeakGyro) * 100.0f;
    }

    // 2. Tempo drift: duration increases relative to rep 1
    float tempoDriftPct = 0.0f;
    if (_initialRepDuration > 0 && durationMs > _initialRepDuration) {
        tempoDriftPct = ((float)(durationMs - _initialRepDuration) / (float)_initialRepDuration) * 100.0f;
    }

    // 3. Grip decay: mean grip drops relative to rep 1
    float gripDecayPct = 0.0f;
    if (_initialMeanGrip > 0.1f && avgGrip < _initialMeanGrip) {
        gripDecayPct = ((_initialMeanGrip - avgGrip) / _initialMeanGrip) * 100.0f;
    }

    // Weighted fatigue index
    float compositeFatigue = 0.45f * velocityLossPct + 0.35f * tempoDriftPct + 0.20f * gripDecayPct;
    _stats.fatigueIndexPct = constrain(compositeFatigue, 0.0f, 100.0f);

    // Flag fatigue if composite index >= 35% or velocity loss >= 30%
    _stats.isFatigued = (_stats.fatigueIndexPct >= 35.0f || velocityLossPct >= 30.0f);
}

void RepetitionEngine::reconcileUncertainReps() {
    if (_uncertainCount == 0) return;

    if (_stats.totalValidReps == 0) {
        // Without confirmed valid reps in the set, uncertain candidates cannot be cadence-verified
        for (uint8_t i = 0; i < _uncertainCount; i++) {
            if (_uncertainReps[i].populated) {
                _stats.totalRejectedReps++;
                _uncertainReps[i].populated = false;
            }
        }
        _uncertainCount = 0;
        uint32_t totalAttempts = _stats.totalValidReps + _stats.totalRejectedReps;
        if (totalAttempts > 0) {
            _stats.acceptanceRatioPct = ((float)_stats.totalValidReps / totalAttempts) * 100.0f;
        }
        return;
    }

    // Average tempo of confirmed valid reps
    float meanTempo = (float)_stats.lastRepDurationMs;
    if (meanTempo < 500.0f) meanTempo = 1500.0f;

    for (uint8_t i = 0; i < _uncertainCount; i++) {
        if (_uncertainReps[i].populated) {
            float deltaTempo = fabsf((float)_uncertainReps[i].durationMs - meanTempo);
            // Reconcile: If uncertain rep's duration is within 25% of set cadence, count it as valid
            if (deltaTempo <= 0.25f * meanTempo) {
                _stats.totalValidReps++;
                _stats.lastRepWasUncertain = true;
            } else {
                _stats.totalRejectedReps++;
            }
            _uncertainReps[i].populated = false;
        }
    }
    _uncertainCount = 0;

    uint32_t totalAttempts = _stats.totalValidReps + _stats.totalRejectedReps;
    if (totalAttempts > 0) {
        _stats.acceptanceRatioPct = ((float)_stats.totalValidReps / totalAttempts) * 100.0f;
    }
}

void RepetitionEngine::updateAdaptiveThresholds(float peakRom, uint32_t durationMs) {
    _recentPeakRom[_recentHead] = peakRom;
    _recentDuration[_recentHead] = durationMs;
    _recentHead = (_recentHead + 1) % 5;
    if (_recentCount < 5) _recentCount++;

    if (_recentCount >= 3) {
        float sortedRom[5];
        uint32_t sortedDur[5];
        for (uint8_t i = 0; i < _recentCount; i++) {
            sortedRom[i] = _recentPeakRom[i];
            sortedDur[i] = _recentDuration[i];
        }

        for (uint8_t i = 1; i < _recentCount; i++) {
            float keyRom = sortedRom[i];
            uint32_t keyDur = sortedDur[i];
            int8_t j = i - 1;
            while (j >= 0 && sortedRom[j] > keyRom) {
                sortedRom[j + 1] = sortedRom[j];
                j--;
            }
            sortedRom[j + 1] = keyRom;

            j = i - 1;
            while (j >= 0 && sortedDur[j] > keyDur) {
                sortedDur[j + 1] = sortedDur[j];
                j--;
            }
            sortedDur[j + 1] = keyDur;
        }

        float medianRom = sortedRom[_recentCount / 2];
        uint32_t medianDur = sortedDur[_recentCount / 2];

        float adaptiveRom = 0.65f * medianRom;
        _stats.adaptiveMinRomDeg = constrain(adaptiveRom, _activeProfile.minRomDeg * 0.5f, _activeProfile.minRomDeg * 1.15f);

        double sumDur = 0.0;
        for (uint8_t i = 0; i < _recentCount; i++) {
            sumDur += sortedDur[i];
        }
        double meanDur = sumDur / (double)_recentCount;

        double sumSq = 0.0;
        for (uint8_t i = 0; i < _recentCount; i++) {
            double diff = sortedDur[i] - meanDur;
            sumSq += diff * diff;
        }
        float sigmaDur = sqrtf((float)(sumSq / (double)_recentCount));
        if (sigmaDur < 250.0f) sigmaDur = 250.0f; // Minimum 250 ms physiological tempo tolerance

        float lowerBound = (float)medianDur - 2.0f * sigmaDur;
        float upperBound = (float)medianDur + 2.0f * sigmaDur;
        _stats.adaptiveMinDurationMs = (uint32_t)constrain(lowerBound, (float)_activeProfile.minDurationMs, (float)_activeProfile.maxDurationMs);
        _stats.adaptiveMaxDurationMs = (uint32_t)constrain(upperBound, (float)_activeProfile.minDurationMs, (float)_activeProfile.maxDurationMs);
    }
}

void RepetitionEngine::finalizeRep(bool isValid, RejectionReason reason, uint32_t durationMs, float peakRom, float avgGrip, float rho, float confidence) {
    _stats.lastRepDurationMs = durationMs;
    _stats.lastRepPeakRomDeg = peakRom;
    _stats.lastRepAvgGripRatio = avgGrip;
    _stats.lastRejectionReason = reason;
    _stats.lastRepCouplingRho = rho;
    _stats.lastRepConfidencePct = confidence;
    _stats.lastRepWasUncertain = false;
    _stats.lastRepTimestampMs = millis();
    _lastRepEndTimeMs = millis();

    if (isValid) {
        transitionTo(REP_STATE_VALID_REP);
    } else {
        if (reason != REJECT_NONE) {
            _stats.totalRejectedReps++;
        }
        transitionTo(REP_STATE_REJECTED_REP);
    }

    uint32_t totalAttempts = _stats.totalValidReps + _stats.totalRejectedReps;
    if (totalAttempts > 0) {
        _stats.acceptanceRatioPct = ((float)_stats.totalValidReps / totalAttempts) * 100.0f;
    }

    resetInFlightState();
}

void RepetitionEngine::update(const ProcessedSensorData& proc, const CalibrationProfile& calib, bool sensorHealthOk) {
    uint32_t nowMs = millis();
    float dt = Config::Timing::DT_SECONDS;
    float excursion = calculateExcursion(proc, calib);
    _stats.currentExcursionDeg = excursion;
    _stats.isSetArmed = proc.gripGate;

    uint32_t refractoryMs = (_activeProfile.minDurationMs > 600) ? (_activeProfile.minDurationMs / 2) : 300;

    // Single grip threshold normalized from calib.gripEligibilityADC
    float gripThresholdNorm = 0.35f;
    if (calib.fsrFirmMeanADC > calib.fsrRelaxedMeanADC) {
        gripThresholdNorm = (float)(calib.gripEligibilityADC - calib.fsrRelaxedMeanADC) /
                            (float)(calib.fsrFirmMeanADC - calib.fsrRelaxedMeanADC);
    } else {
        gripThresholdNorm = _activeProfile.minGripRatio;
    }
    if (gripThresholdNorm < 0.05f) gripThresholdNorm = 0.05f;
    if (gripThresholdNorm > 0.95f) gripThresholdNorm = 0.95f;

    // Phase 4: Track continuous grip disarm for set-end reconciliation (> 1.5s / 75 samples)
    if (!proc.gripGate) {
        _disarmedSamplesCount++;
        if (_disarmedSamplesCount == 75) {
            reconcileUncertainReps();
        }
    } else {
        _disarmedSamplesCount = 0;
    }

    // Immediate sensor fault check in flight
    if (!sensorHealthOk) {
        _sensorFaultDuringRep = true;
        if (_currentState == REP_STATE_CONCENTRIC_OR_OUTBOUND ||
            _currentState == REP_STATE_TRANSITION ||
            _currentState == REP_STATE_ECCENTRIC_OR_RETURN) {
            finalizeRep(false, REJECT_SENSOR_UNRELIABLE, nowMs - _repStartTimeMs, _ascentProminence, _stats.currentRepMeanGrip);
            return;
        }
    }

    if (_currentState == REP_STATE_VALID_REP || _currentState == REP_STATE_REJECTED_REP) {
        transitionTo(REP_STATE_IDLE);
        return;
    }

    if (nowMs - _lastRepEndTimeMs < refractoryMs) {
        transitionTo(REP_STATE_IDLE);
        return;
    }

    switch (_currentState) {
        case REP_STATE_IDLE: {
            _stats.currentRepElapsedMs = 0;
            _stats.currentRepMeanGrip = 0.0f;
            _onsetDebounceCount = 0;
            _peakDebounceCount = 0;
            _returnOnsetDebounceCount = 0;
            _returnDebounceCount = 0;

            if (proc.isStationary || (proc.filteredGyroMag_dps < _activeProfile.returnSettledGyroDps && excursion < 25.0f)) {
                _bottomValleyDeg = excursion;
            }

            if (excursion <= _bottomValleyDeg + _activeProfile.returnToleranceDeg) {
                transitionTo(REP_STATE_READY);
            }
            break;
        }

        case REP_STATE_READY: {
            // Expire pending candidate #1 if second rep is not initiated within 4500 ms (regardless of grip)
            if (_pendingRep1.populated && (nowMs - _pendingRep1.timestampMs > 4500)) {
                _pendingRep1.populated = false;
                _stats.pendingCandidateCount = 0;
            }

            if (!proc.gripGate) {
                _onsetDebounceCount = 0;
                break;
            }

            if (proc.isStationary || (proc.filteredGyroMag_dps < _activeProfile.returnSettledGyroDps && excursion < 25.0f)) {
                _bottomValleyDeg = excursion;
            }

            float prominenceAboveValley = excursion - _bottomValleyDeg;
            if (prominenceAboveValley > _activeProfile.onsetExcursionDeg && proc.filteredGyroMag_dps > _activeProfile.onsetGyroDps) {
                _onsetDebounceCount++;
                if (_onsetDebounceCount >= 3) {
                    _onsetDebounceCount = 0;
                    _repStartTimeMs = nowMs;
                    _concentricStartTimeMs = nowMs;
                    _peakExcursionDeg = excursion;
                    _ascentProminence = prominenceAboveValley;
                    _returnValleyDeg = excursion;
                    _minGrip = proc.normalizedGrip;
                    _gripAccumulator = proc.normalizedGrip;
                    _gripSampleCount = 1;
                    _sustainedLowGripSamples = 0;
                    _gripFailedDuringRep = false;
                    _sensorFaultDuringRep = !sensorHealthOk;
                    _repHistorySampleCount = 0;
                    _fsrSlowEMA = proc.filteredFsrADC;
                    _prevFilteredGyroMag = proc.filteredGyroMag_dps;
                    _concentricPeakGyro = proc.filteredGyroMag_dps;
                    _concentricPeakGyroTimeMs = 0;
                    _peakDebounceCount = 0;

                    transitionTo(REP_STATE_CONCENTRIC_OR_OUTBOUND);
                }
            } else {
                _onsetDebounceCount = 0;
                if (excursion > _bottomValleyDeg + _activeProfile.returnToleranceDeg + 10.0f) {
                    transitionTo(REP_STATE_IDLE);
                }
            }
            break;
        }

        case REP_STATE_CONCENTRIC_OR_OUTBOUND: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            if (!proc.gripGate) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    finalizeRep(false, REJECT_INSUFFICIENT_GRIP, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
                    return;
                }
            }

            // Phase 4: Buffer High-Pass FSR and Angular Acceleration
            if (_repHistorySampleCount < MAX_REP_SAMPLES) {
                _fsrSlowEMA = 0.95f * _fsrSlowEMA + 0.05f * proc.filteredFsrADC;
                float fsrHP = proc.filteredFsrADC - _fsrSlowEMA;
                float gyroAccel = fabsf(proc.filteredGyroMag_dps - _prevFilteredGyroMag) / dt;
                _prevFilteredGyroMag = proc.filteredGyroMag_dps;

                _fsrHighPassHistory[_repHistorySampleCount] = fsrHP;
                _gyroAccelHistory[_repHistorySampleCount] = gyroAccel;
                _repHistorySampleCount++;
            }

            // Track early concentric peak gyro for momentum swing detection
            if (proc.filteredGyroMag_dps > _concentricPeakGyro) {
                _concentricPeakGyro = proc.filteredGyroMag_dps;
                _concentricPeakGyroTimeMs = elapsed;
            }

            if (!_dominantAxisLocked) {
                _gyroEnergy[0] += proc.gyroX_dps * proc.gyroX_dps;
                _gyroEnergy[1] += proc.gyroY_dps * proc.gyroY_dps;
                _gyroEnergy[2] += proc.gyroZ_dps * proc.gyroZ_dps;
            }

            if (excursion > _peakExcursionDeg) {
                _peakExcursionDeg = excursion;
            }
            _ascentProminence = _peakExcursionDeg - _bottomValleyDeg;

            _gripAccumulator += proc.normalizedGrip;
            _gripSampleCount++;
            if (proc.normalizedGrip < _minGrip) {
                _minGrip = proc.normalizedGrip;
            }
            _stats.currentRepMeanGrip = _gripAccumulator / _gripSampleCount;

            if (excursion >= _activeProfile.sustainedGripMinExcursionDeg) {
                if (proc.normalizedGrip < gripThresholdNorm) {
                    _sustainedLowGripSamples++;
                    if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                        _gripFailedDuringRep = true;
                    }
                } else {
                    _sustainedLowGripSamples = 0;
                }
            }

            float currentProminence = excursion - _bottomValleyDeg;
            if (currentProminence < _activeProfile.concentricAbortExcursionDeg && _ascentProminence < _activeProfile.concentricAbortPeakDeg) {
                _peakDebounceCount = 0;
                _onsetDebounceCount = 0;
                resetInFlightState();
                transitionTo(REP_STATE_READY);
                return;
            }

            if (_ascentProminence >= _activeProfile.peakTransExcursionDeg && proc.filteredGyroMag_dps < _activeProfile.peakTransGyroDps) {
                _peakDebounceCount++;
                if (_peakDebounceCount >= 3) {
                    _peakDebounceCount = 0;
                    _transitionStartTimeMs = nowMs;
                    _returnOnsetDebounceCount = 0;
                    transitionTo(REP_STATE_TRANSITION);
                }
            } else {
                _peakDebounceCount = 0;
            }

            if (elapsed > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
            }
            break;
        }

        case REP_STATE_TRANSITION: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            if (!proc.gripGate) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    finalizeRep(false, REJECT_INSUFFICIENT_GRIP, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
                    return;
                }
            }

            if (_repHistorySampleCount < MAX_REP_SAMPLES) {
                _fsrSlowEMA = 0.95f * _fsrSlowEMA + 0.05f * proc.filteredFsrADC;
                float fsrHP = proc.filteredFsrADC - _fsrSlowEMA;
                float gyroAccel = fabsf(proc.filteredGyroMag_dps - _prevFilteredGyroMag) / dt;
                _prevFilteredGyroMag = proc.filteredGyroMag_dps;

                _fsrHighPassHistory[_repHistorySampleCount] = fsrHP;
                _gyroAccelHistory[_repHistorySampleCount] = gyroAccel;
                _repHistorySampleCount++;
            }

            bool peakRose = false;
            if (excursion > _peakExcursionDeg) {
                _peakExcursionDeg = excursion;
                peakRose = true;
            }
            _ascentProminence = _peakExcursionDeg - _bottomValleyDeg;

            _gripAccumulator += proc.normalizedGrip;
            _gripSampleCount++;
            if (proc.normalizedGrip < _minGrip) {
                _minGrip = proc.normalizedGrip;
            }
            _stats.currentRepMeanGrip = _gripAccumulator / _gripSampleCount;

            if (proc.normalizedGrip < gripThresholdNorm) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    _gripFailedDuringRep = true;
                }
            } else {
                _sustainedLowGripSamples = 0;
            }

            if (proc.filteredGyroMag_dps > _activeProfile.transitionStallGyroDps || peakRose) {
                _transitionStartTimeMs = nowMs;
            }

            if (excursion < (_peakExcursionDeg - _activeProfile.returnOnsetDeltaDeg)) {
                _returnOnsetDebounceCount++;
                if (_returnOnsetDebounceCount >= 3) {
                    _returnOnsetDebounceCount = 0;
                    _returnDebounceCount = 0;
                    _returnValleyDeg = excursion;
                    transitionTo(REP_STATE_ECCENTRIC_OR_RETURN);
                }
            } else {
                _returnOnsetDebounceCount = 0;
            }

            if (nowMs - _transitionStartTimeMs > 3000 || elapsed > _activeProfile.maxDurationMs) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
            }
            break;
        }

        case REP_STATE_ECCENTRIC_OR_RETURN: {
            uint32_t elapsed = nowMs - _repStartTimeMs;
            _stats.currentRepElapsedMs = elapsed;

            if (!proc.gripGate && excursion > (_bottomValleyDeg + _activeProfile.returnSettledExcursionDeg)) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    finalizeRep(false, REJECT_INSUFFICIENT_GRIP, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
                    return;
                }
            }

            if (_repHistorySampleCount < MAX_REP_SAMPLES) {
                _fsrSlowEMA = 0.95f * _fsrSlowEMA + 0.05f * proc.filteredFsrADC;
                float fsrHP = proc.filteredFsrADC - _fsrSlowEMA;
                float gyroAccel = fabsf(proc.filteredGyroMag_dps - _prevFilteredGyroMag) / dt;
                _prevFilteredGyroMag = proc.filteredGyroMag_dps;

                _fsrHighPassHistory[_repHistorySampleCount] = fsrHP;
                _gyroAccelHistory[_repHistorySampleCount] = gyroAccel;
                _repHistorySampleCount++;
            }

            if (excursion < _returnValleyDeg) {
                _returnValleyDeg = excursion;
            }
            _descentProminence = _peakExcursionDeg - _returnValleyDeg;

            _gripAccumulator += proc.normalizedGrip;
            _gripSampleCount++;
            if (proc.normalizedGrip < _minGrip) {
                _minGrip = proc.normalizedGrip;
            }
            _stats.currentRepMeanGrip = _gripAccumulator / _gripSampleCount;

            if (proc.normalizedGrip < gripThresholdNorm) {
                _sustainedLowGripSamples++;
                if (_sustainedLowGripSamples >= Config::RepEngineDefaults::SUSTAINED_LOW_GRIP_SAMPLES) {
                    _gripFailedDuringRep = true;
                }
            } else {
                _sustainedLowGripSamples = 0;
            }

            bool returnNearBottom = (excursion <= _bottomValleyDeg + 10.0f);
            bool belowReturnThreshold = (excursion <= _bottomValleyDeg + _activeProfile.returnSettledExcursionDeg);
            bool isAtRest = (proc.filteredGyroMag_dps < _activeProfile.returnSettledGyroDps) || proc.isStationary;
            bool stationaryCompletion = (belowReturnThreshold && isAtRest);

            if (returnNearBottom || stationaryCompletion) {
                _returnDebounceCount++;
                if (_returnDebounceCount >= 3) {
                    _returnDebounceCount = 0;
                    transitionTo(REP_STATE_REP_CANDIDATE);
                }
            } else {
                _returnDebounceCount = 0;
            }

            if (elapsed > _activeProfile.maxDurationMs) {
                if (_ascentProminence >= _activeProfile.minRomDeg) {
                    finalizeRep(false, REJECT_INCOMPLETE_RETURN, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
                } else {
                    finalizeRep(false, REJECT_DURATION_TOO_LONG, elapsed, _ascentProminence, _stats.currentRepMeanGrip);
                }
            }
            break;
        }

        case REP_STATE_REP_CANDIDATE: {
            uint32_t totalDuration = nowMs - _repStartTimeMs;
            float avgGrip = (_gripSampleCount > 0) ? (_gripAccumulator / _gripSampleCount) : 0.0f;

            // Phase 4: Compute Grip-Load Coupling Score (rho) & Optimal Lag early so all candidates have rho
            uint8_t bestLag = 0;
            float rho = calculateGripLoadCoupling(_repHistorySampleCount, bestLag);

            if (_sensorFaultDuringRep) {
                finalizeRep(false, REJECT_SENSOR_UNRELIABLE, totalDuration, _ascentProminence, avgGrip, rho);
                return;
            }

            // Duration Checks
            uint32_t effectiveMinDur = (_recentCount >= 3) ? _stats.adaptiveMinDurationMs : _activeProfile.minDurationMs;
            uint32_t effectiveMaxDur = (_recentCount >= 3) ? _stats.adaptiveMaxDurationMs : _activeProfile.maxDurationMs;

            if (totalDuration < effectiveMinDur) {
                finalizeRep(false, REJECT_DURATION_TOO_SHORT, totalDuration, _ascentProminence, avgGrip, rho);
                return;
            }

            if (totalDuration > effectiveMaxDur) {
                finalizeRep(false, REJECT_DURATION_TOO_LONG, totalDuration, _ascentProminence, avgGrip, rho);
                return;
            }

            // Prominence Requirement on Both Legs
            float effectiveMinRom = (_recentCount >= 3) ? _stats.adaptiveMinRomDeg : _activeProfile.minRomDeg;
            if (_ascentProminence < effectiveMinRom || _descentProminence < effectiveMinRom) {
                finalizeRep(false, REJECT_INSUFFICIENT_ROM, totalDuration, _ascentProminence, avgGrip, rho);
                return;
            }

            // Grip Eligibility Check: verify sustained low grip flag wasn't tripped and average grip is adequate
            if (_gripFailedDuringRep || avgGrip < (gripThresholdNorm * 0.75f)) {
                finalizeRep(false, REJECT_INSUFFICIENT_GRIP, totalDuration, _ascentProminence, avgGrip, rho);
                return;
            }

            // Phase 4 Rule 2: Loadedness check -> REJECT_UNLOADED (Mimed or dropped weight)
            float loadedness = 0.60f * rho + 0.40f * avgGrip;
            if ((rho < 0.12f && avgGrip < 0.42f) || (loadedness < 0.18f)) {
                finalizeRep(false, REJECT_UNLOADED, totalDuration, _ascentProminence, avgGrip, rho, 25.0f);
                return;
            }

            // Phase 4 Rule 4: Momentum Swing Cheat Detection
            uint32_t concentricDuration = (_transitionStartTimeMs > _concentricStartTimeMs) ? 
                                          (_transitionStartTimeMs - _concentricStartTimeMs) : (totalDuration / 2);
            bool isMomentumSwing = (concentricDuration < 450) && 
                                   (_concentricPeakGyroTimeMs < 250) && 
                                   (_concentricPeakGyro > 110.0f) && 
                                   (rho < 0.22f);
            if (isMomentumSwing) {
                finalizeRep(false, REJECT_MOMENTUM_SWING, totalDuration, _ascentProminence, avgGrip, rho, 30.0f);
                return;
            }

            // Phase 4 Rule 3: Multi-Factor Per-Rep Confidence Scoring
            float confidence = evaluateRepConfidence(_ascentProminence, totalDuration, rho, proc.signalNoiseEstimate, bestLag);

            // Confidence Gate Classification:
            // High (>= 60%): Count now
            // Mid (40% - 60%): Uncertain (pool for set-end reconciliation)
            // Low (< 40%): Reject
            if (confidence < 40.0f) {
                finalizeRep(false, REJECT_INSUFFICIENT_ROM, totalDuration, _ascentProminence, avgGrip, rho, confidence);
                return;
            }

            // Lock dominant motion axis after first qualified candidate
            if (!_dominantAxisLocked) {
                uint8_t bestAxis = 1;
                float maxEnergy = _gyroEnergy[1];
                if (_gyroEnergy[0] > maxEnergy) {
                    bestAxis = 0;
                    maxEnergy = _gyroEnergy[0];
                }
                if (_gyroEnergy[2] > maxEnergy) {
                    bestAxis = 2;
                }
                _dominantAxisIndex = bestAxis;
                _dominantSign = 1;
                _dominantAxisLocked = true;
            }

            // If rep is in uncertain range (40% - 60%), hold in uncertain pool
            if (confidence < 60.0f) {
                if (_uncertainCount < 2) {
                    _uncertainReps[_uncertainCount].durationMs = totalDuration;
                    _uncertainReps[_uncertainCount].peakRomDeg = _ascentProminence;
                    _uncertainReps[_uncertainCount].avgGrip = avgGrip;
                    _uncertainReps[_uncertainCount].couplingRho = rho;
                    _uncertainReps[_uncertainCount].confidencePct = confidence;
                    _uncertainReps[_uncertainCount].timestampMs = nowMs;
                    _uncertainReps[_uncertainCount].populated = true;
                    _uncertainCount++;
                }
                finalizeRep(false, REJECT_NONE, totalDuration, _ascentProminence, avgGrip, rho, confidence);
                return;
            }

            // Update Fatigue Tracking
            updateFatigueMetrics(_concentricPeakGyro, totalDuration, avgGrip);

            // 2-Candidate Confirmation Gate
            if (!_confirmedSetStarted) {
                if (!_pendingRep1.populated) {
                    _pendingRep1.durationMs = totalDuration;
                    _pendingRep1.peakRomDeg = _ascentProminence;
                    _pendingRep1.avgGrip = avgGrip;
                    _pendingRep1.couplingRho = rho;
                    _pendingRep1.confidencePct = confidence;
                    _pendingRep1.timestampMs = nowMs;
                    _pendingRep1.populated = true;
                    _stats.pendingCandidateCount = 1;

                    _lastRepEndTimeMs = nowMs;
                    resetInFlightState();
                    transitionTo(REP_STATE_IDLE);
                    return;
                } else {
                    float deltaRom = fabsf(_pendingRep1.peakRomDeg - _ascentProminence);
                    uint32_t deltaDur = (totalDuration > _pendingRep1.durationMs) ? 
                                        (totalDuration - _pendingRep1.durationMs) : 
                                        (_pendingRep1.durationMs - totalDuration);

                    bool isConsistent = (deltaRom <= 30.0f) && (deltaDur <= 2000);
                    if (isConsistent) {
                        _stats.totalValidReps += 2;
                        _confirmedSetStarted = true;
                        _stats.pendingCandidateCount = 0;

                        updateAdaptiveThresholds(_pendingRep1.peakRomDeg, _pendingRep1.durationMs);
                        updateAdaptiveThresholds(_ascentProminence, totalDuration);

                        _pendingRep1.populated = false;
                        finalizeRep(true, REJECT_NONE, totalDuration, _ascentProminence, avgGrip, rho, confidence);
                        return;
                    } else {
                        _pendingRep1.populated = false;
                        _stats.pendingCandidateCount = 0;
                        finalizeRep(false, REJECT_INSUFFICIENT_ROM, totalDuration, _ascentProminence, avgGrip, rho, confidence);
                        return;
                    }
                }
            } else {
                _stats.totalValidReps++;
                updateAdaptiveThresholds(_ascentProminence, totalDuration);
                finalizeRep(true, REJECT_NONE, totalDuration, _ascentProminence, avgGrip, rho, confidence);
            }
            break;
        }

        default:
            break;
    }
}
