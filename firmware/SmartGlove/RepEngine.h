#ifndef REP_ENGINE_H
#define REP_ENGINE_H

#include <Arduino.h>
#include "Config.h"
#include "SensorAcquisition.h"
#include "SignalProcessing.h"
#include "Calibration.h"

/**
 * 8-State Deterministic Movement Finite State Machine
 */
enum RepState {
    REP_STATE_IDLE = 0,
    REP_STATE_READY,
    REP_STATE_CONCENTRIC_OR_OUTBOUND,
    REP_STATE_TRANSITION,
    REP_STATE_ECCENTRIC_OR_RETURN,
    REP_STATE_REP_CANDIDATE,
    REP_STATE_VALID_REP,
    REP_STATE_REJECTED_REP
};

/**
 * Structured, explainable rejection reasons
 */
enum RejectionReason {
    REJECT_NONE = 0,
    REJECT_INSUFFICIENT_ROM,
    REJECT_DURATION_TOO_SHORT,
    REJECT_DURATION_TOO_LONG,
    REJECT_INSUFFICIENT_GRIP,
    REJECT_INCOMPLETE_RETURN,
    REJECT_SENSOR_UNRELIABLE
};

/**
 * Supported Exercise Types
 */
enum ExerciseType {
    EXERCISE_BICEP_CURL = 0,
    EXERCISE_WRIST_EXTENSION,
    EXERCISE_DUMBBELL_PRESS
};

/**
 * Exercise Profile Configuration
 */
struct ExerciseProfile {
    ExerciseType type;
    char name[24];
    float minRomDeg;           // Minimum required angular excursion (deg)
    uint32_t minDurationMs;    // Ballistic filter threshold (ms)
    uint32_t maxDurationMs;    // Stalled movement threshold (ms)
    float minGripRatio;        // Minimum normalized grip during concentric phase [0.0 - 1.0]
    float returnToleranceDeg;  // Return-to-start posture tolerance window (deg)
};

/**
 * Real-time repetition tracking telemetry
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
};

class RepetitionEngine {
public:
    RepetitionEngine();

    /**
     * Initialize repetition state machine and load default exercise profile
     */
    void init();

    /**
     * Process 50 Hz frame through the repetition finite state machine
     */
    void update(const ProcessedSensorData& proc, const CalibrationProfile& calib, bool sensorHealthOk = true);

    /**
     * Configure active exercise profile
     */
    void setExerciseProfile(ExerciseType type);

    /**
     * Reset repetition session counters
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
    RepState _currentState;
    ExerciseProfile _activeProfile;
    RepetitionStats _stats;

    // In-flight rep tracking variables
    uint32_t _repStartTimeMs;
    uint32_t _concentricStartTimeMs;
    uint32_t _transitionStartTimeMs;
    uint32_t _lastRepEndTimeMs;

    float _startPitchDeg;
    float _startRollDeg;
    float _peakExcursionDeg;

    // Grip integration accumulators
    float _gripAccumulator;
    uint32_t _gripSampleCount;
    bool _gripFailedDuringConcentric;
    bool _sensorFaultDuringRep;

    float calculateExcursion(const ProcessedSensorData& proc, const CalibrationProfile& calib) const;
    void finalizeRep(bool isValid, RejectionReason reason, uint32_t durationMs, float peakRom, float avgGrip);
    void transitionTo(RepState nextState);
};

#endif // REP_ENGINE_H
