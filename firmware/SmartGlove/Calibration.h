#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "Config.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#endif
#include "SensorAcquisition.h"
#include "SignalProcessing.h"

/**
 * 5-Step Guided Calibration State Machine
 */
enum CalibrationStep {
    CALIB_IDLE = 0,
    CALIB_STEP1_PREFLIGHT,       // Verify sensor communication & signal stability
    CALIB_STEP2_NEUTRAL_WRIST,   // Hold resting neutral wrist posture (3 sec)
    CALIB_STEP3_RELAXED_GRIP,    // Hold relaxed, unloaded grip (2 sec)
    CALIB_STEP4_FIRM_GRIP,       // Apply firm, repeatable squeeze (2 sec)
    CALIB_STEP5_VALIDATION,      // Check separation ratio & statistical quality
    CALIB_COMPLETED,             // Successfully validated and saved to NVS
    CALIB_FAILED                 // Inconsistent, unstable, or insufficient separation
};

/**
 * Persistent Personal Calibration Profile
 * Stored in ESP32 Non-Volatile Storage (Preferences / NVS)
 */
struct CalibrationProfile {
    uint32_t magicHeader;        // 0x47524950 ("GRIP")
    uint16_t schemaVersion;      // 1 (v1.0)
    uint32_t calibratedTimeMs;   // System uptime when calibrated

    // Neutral Wrist Reference Posture
    float neutralPitchDeg;       // Mean resting pitch
    float neutralRollDeg;        // Mean resting roll
    float postureStabilityStd;   // Standard deviation during neutral hold

    // Grip Reference Baselines (12-bit ADC 0 - 4095)
    uint16_t fsrRelaxedMeanADC;
    float fsrRelaxedStdDev;
    uint16_t fsrFirmMeanADC;
    float fsrFirmStdDev;
    float gripSeparationRatio;   // (Firm - Relaxed) / (StdFirm + StdRelaxed)

    // Derived Personalized Repetition Thresholds
    uint16_t gripEligibilityADC; // Threshold to qualify as active grip during rep
    float postureToleranceDeg;   // Return-to-start angular window (default +/- 15 deg)

    // Integrity & Validity
    bool isValid;
    uint32_t crc32;
};

class PersonalCalibration {
public:
    PersonalCalibration();

    /**
     * Initialize NVS storage, load stored calibration profile, verify CRC32.
     * Falls back to conservative default profile if uncalibrated or corrupted.
     */
    bool init();

    /**
     * Calibration Wizard Control
     */
    void startWizard();
    void advanceStep();
    void abortWizard(const char* reason = "User aborted");

    /**
     * Feeds 50 Hz sensor frames during active calibration wizard step.
     */
    void processSample(const RawSensorData& raw, const ProcessedSensorData& proc);

    /**
     * Getters
     */
    CalibrationStep getActiveStep() const { return _currentStep; }
    const char* getStepName() const;
    uint8_t getStepProgressPct() const;
    const CalibrationProfile& getProfile() const { return _profile; }
    const char* getFailureReason() const { return _failureReason; }
    bool isCalibrated() const { return _profile.isValid; }

    /**
     * Profile Management & Persistence
     */
    bool saveToNVS();
    bool loadFromNVS();
    void resetToDefaults();

    /**
     * Calculate IEEE 802.3 32-bit CRC over profile data
     */
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);

private:
    CalibrationStep _currentStep;
    CalibrationProfile _profile;
    Preferences _prefs;

    char _failureReason[64];
    uint32_t _stepStartTimeMs;
    uint32_t _sampleCount;

    // Running accumulators for statistical moments (Welford's algorithm or sums)
    float _sumPitch;
    float _sumPitchSq;
    float _sumRoll;
    float _sumRollSq;
    uint32_t _sumADC;
    uint64_t _sumADCSq;

    void resetAccumulators();
    void failCalibration(const char* reason);
};

#endif // CALIBRATION_H
