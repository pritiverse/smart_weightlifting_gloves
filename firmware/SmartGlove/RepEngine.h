#ifndef REP_ENGINE_H
#define REP_ENGINE_H

#include <cstring>
#include <math.h>
#include "Config.h"
#include "SensorAcquisition.h"
#include "SignalProcessing.h"
#include "Calibration.h"

/**
 * 8-State Deterministic Movement Finite State Machine
 */
enum RepState {
    REP_STATE_IDLE = 0,
    REP_STATE_READY,                  // Valley tracking & stationary re-baselining
    REP_STATE_CONCENTRIC_OR_OUTBOUND, // Ascent leg (prominence tracking)
    REP_STATE_TRANSITION,             // Peak inflection
    REP_STATE_ECCENTRIC_OR_RETURN,    // Descent leg (prominence tracking)
    REP_STATE_REP_CANDIDATE,          // Candidate evaluation
    REP_STATE_VALID_REP,
    REP_STATE_REJECTED_REP
};

/**
 * Structured, explainable rejection reasons (Phase 4 novel rejections included)
 */
enum RejectionReason {
    REJECT_NONE = 0,
    REJECT_INSUFFICIENT_ROM,
    REJECT_DURATION_TOO_SHORT,
    REJECT_DURATION_TOO_LONG,
    REJECT_INSUFFICIENT_GRIP,
    REJECT_INCOMPLETE_RETURN,
    REJECT_SENSOR_UNRELIABLE,
    REJECT_UNLOADED,         // Phase 4: Mimed or dropped weight (insufficient grip-load coupling)
    REJECT_MOMENTUM_SWING    // Phase 4: Body swing cheat (early gyro spike, short concentric, low coupling)
};

/**
 * Supported Exercise Types
 * Note: EXERCISE_DUMBBELL_PRESS operates via a stationary-bounded lockout/rack window
 * and has documented lower angular accuracy compared to rotational curl/extension.
 */
enum ExerciseType {
    EXERCISE_BICEP_CURL = 0,
    EXERCISE_WRIST_EXTENSION,
    EXERCISE_DUMBBELL_PRESS
};

/**
 * Exercise Profile Configuration with per-exercise thresholds
 */
struct ExerciseProfile {
    ExerciseType type;
    char name[24];
    float minRomDeg;                   // Minimum required angular excursion (deg)
    uint32_t minDurationMs;            // Ballistic filter threshold (ms)
    uint32_t maxDurationMs;            // Stalled movement threshold (ms)
    float minGripRatio;                // Fallback minimum normalized grip during rep [0.0 - 1.0]
    float returnToleranceDeg;          // Return-to-start posture tolerance window (deg)

    // Dynamic movement thresholds
    float onsetExcursionDeg;           // Excursion threshold above valley for movement onset (deg)
    float onsetGyroDps;                // Gyro angular velocity for movement onset (deg/s)
    float peakTransExcursionDeg;       // Excursion threshold to qualify for peak transition (deg)
    float peakTransGyroDps;            // Gyro velocity drop threshold at peak (deg/s)
    float returnOnsetDeltaDeg;         // Descent from peak to enter eccentric (deg)
    float concentricAbortExcursionDeg; // Excursion threshold for concentric abort (deg)
    float concentricAbortPeakDeg;      // Peak excursion threshold for concentric abort (deg)
    float transitionStallGyroDps;      // Gyro velocity threshold resetting transition stall timer (deg/s)
    float sustainedGripMinExcursionDeg;// Excursion threshold where sustained low grip is evaluated (deg)
    float returnSettledExcursionDeg;   // Stationary return completion excursion threshold (deg)
    float returnSettledGyroDps;        // Stationary return completion gyro threshold (deg/s)
};

/**
 * Real-time repetition tracking telemetry (Phase 4 Coupling & Confidence Metrics)
 */
struct RepetitionStats {
    uint32_t totalValidReps;
    uint32_t totalRejectedReps;
    float acceptanceRatioPct;  // Valid / (Valid + Rejected) * 100

    // Metrics for the most recently completed rep attempt
    uint32_t lastRepDurationMs;
    float lastRepPeakRomDeg;
    float lastRepAvgGripRatio;
    RejectionReason lastRejectionReason;
    uint32_t lastRepTimestampMs;

    // Active in-progress rep tracking
    float currentExcursionDeg;
    uint32_t currentRepElapsedMs;
    float currentRepMeanGrip;

    // Phase 3 Adaptive Threshold Telemetry
    float adaptiveMinRomDeg;
    uint32_t adaptiveMinDurationMs;
    uint32_t adaptiveMaxDurationMs;
    uint8_t pendingCandidateCount;
    bool isSetArmed;

    // Phase 4 Novel Feature Telemetry: Grip-Load Coupling, Confidence & Fatigue
    float lastRepCouplingRho;     // Grip-load cross-correlation score [-1.0, 1.0]
    float lastRepConfidencePct;   // Multi-factor confidence score [0 - 100%]
    bool lastRepWasUncertain;     // True if held in uncertain pool and reconciled at set end
    bool isFatigued;              // True if fatigue criteria (velocity loss, tempo drift, grip decay) active
    float fatigueIndexPct;        // Composite fatigue index [0 - 100%]
};

/**
 * Fixed-size candidate record for back-crediting and set-end reconciliation
 */
struct CandidateRepRecord {
    uint32_t durationMs;
    float peakRomDeg;
    float avgGrip;
    float couplingRho;
    float confidencePct;
    uint32_t timestampMs;
    bool populated;
};

class RepetitionEngine {
public:
    RepetitionEngine();

    /**
     * Initialize repetition state machine; retains current profile type without forcing bicep curl
     */
    void init();

    /**
     * Process 50 Hz frame through the repetition finite state machine
     */
    void update(const ProcessedSensorData& proc, const CalibrationProfile& calib, bool sensorHealthOk = true);

    /**
     * Configure active exercise profile and reset in-flight state
     */
    void setExerciseProfile(ExerciseType type);

    /**
     * Reset repetition session counters, fatigue metrics, and adaptive baselines
     */
    void resetCounters();

    /**
     * Telemetry and State Getters
     */
    RepState getState() const { return _currentState; }
    const char* getStateName() const;
    const RepetitionStats& getStats() const { return _stats; }
    const ExerciseProfile& getActiveProfile() const { return _activeProfile; }
    const char* getRejectionReasonName(RejectionReason reason) const;

private:
    static constexpr uint16_t MAX_REP_SAMPLES = 256;

    RepState _currentState;
    ExerciseProfile _activeProfile;
    RepetitionStats _stats;

    // Valley -> Peak -> Valley Prominence Tracking
    float _bottomValleyDeg;
    float _returnValleyDeg;
    float _peakExcursionDeg;
    float _ascentProminence;
    float _descentProminence;

    // Dominant Motion Axis Estimation
    float _gyroEnergy[3];
    uint8_t _dominantAxisIndex;
    int8_t _dominantSign;
    bool _dominantAxisLocked;

    // Grip Gate & Latch
    float _minGrip;
    float _gripAccumulator;
    uint32_t _gripSampleCount;
    uint16_t _sustainedLowGripSamples;
    bool _gripFailedDuringRep;
    bool _sensorFaultDuringRep;

    // Timers
    uint32_t _repStartTimeMs;
    uint32_t _concentricStartTimeMs;
    uint32_t _transitionStartTimeMs;
    uint32_t _lastRepEndTimeMs;
    uint16_t _disarmedSamplesCount; // Tracks grip release duration for set-end reconciliation

    // 2-Candidate Rep Confirmation & Back-Crediting Buffer
    CandidateRepRecord _pendingRep1;
    bool _confirmedSetStarted;

    // Phase 4: Uncertain Rep Pool for Set-End Cadence Reconciliation
    CandidateRepRecord _uncertainReps[2];
    uint8_t _uncertainCount;

    // Adaptive Thresholds Memory (Ring buffer of last 5 valid reps)
    float _recentPeakRom[5];
    uint32_t _recentDuration[5];
    uint8_t _recentCount;
    uint8_t _recentHead;

    // Phase 4 Novel Feature Ring Buffers (Grip-Load Coupling)
    float _fsrHighPassHistory[MAX_REP_SAMPLES];
    float _gyroAccelHistory[MAX_REP_SAMPLES];
    uint16_t _repHistorySampleCount;
    float _fsrSlowEMA;
    float _prevFilteredGyroMag;

    // Momentum Swing & Velocity Tracking
    float _concentricPeakGyro;
    uint32_t _concentricPeakGyroTimeMs;

    // Fatigue Baseline Tracking (from Rep 1)
    float _initialPeakGyro;
    uint32_t _initialRepDuration;
    float _initialMeanGrip;
    bool _fatigueBaselineSet;

    // 3-consecutive-sample debouncing counters
    uint8_t _onsetDebounceCount;
    uint8_t _peakDebounceCount;
    uint8_t _returnOnsetDebounceCount;
    uint8_t _returnDebounceCount;

    float calculateExcursion(const ProcessedSensorData& proc, const CalibrationProfile& calib);
    void finalizeRep(bool isValid, RejectionReason reason, uint32_t durationMs, float peakRom, float avgGrip, float rho = 0.0f, float confidence = 100.0f);
    void transitionTo(RepState nextState);
    void resetInFlightState();
    void updateAdaptiveThresholds(float peakRom, uint32_t durationMs);

    // Phase 4 Helpers
    float calculateGripLoadCoupling(uint16_t sampleCount, uint8_t& outBestLag);
    float evaluateRepConfidence(float ascentRom, uint32_t durationMs, float rho, float signalNoise, uint8_t bestLag);
    void reconcileUncertainReps();
    void updateFatigueMetrics(float peakGyro, uint32_t durationMs, float avgGrip);
};

#endif // REP_ENGINE_H
