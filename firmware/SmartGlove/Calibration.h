#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <math.h>
#include <cstring>
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
    CALIB_PREP_STEP1_REST,       // HCI Prep (5.0s): Hang arm relaxed
    CALIB_STEP1_PREFLIGHT,       // Verify sensor communication & signal stability (1.5s)
    CALIB_PREP_STEP2_POSTURE,    // HCI Transition (4.5s): Pick up dumbbell, hold starting curl posture
    CALIB_STEP2_NEUTRAL_WRIST,   // Active hold recording (3.0s)
    CALIB_PREP_STEP3_RELAXED,    // HCI Transition (3.5s): Relax fingers on handle ("hold an egg")
    CALIB_STEP3_RELAXED_GRIP,    // Active baseline recording (2.5s)
    CALIB_PREP_STEP4_FIRM,       // HCI Transition (3.5s): Prepare to squeeze firmly
    CALIB_STEP4_FIRM_GRIP,       // Active squeeze capture (2.5s)
    CALIB_STEP5_VALIDATION,      // Check separation ratio & statistical quality
    CALIB_COMPLETED,             // Successfully validated and saved to NVS
    CALIB_FAILED                 // Inconsistent, unstable, or insufficient separation
};

/**
 * Persistent Personal Calibration Profile (Schema v2)
 * Stored in ESP32 Non-Volatile Storage (Preferences / NVS)
 * Packed to guarantee deterministic byte alignment across compilers and CRC consistency.
 */
#pragma pack(push, 1)
struct CalibrationProfile {
    uint32_t magicHeader;        // 0x47524950 ("GRIP")
    uint16_t schemaVersion;      // 2 (Schema v2.0 - Gravity vector representation)
    uint32_t calibratedTimeMs;   // Milliseconds uptime when calibration was validated and committed to NVS (0 if fallback/uncalibrated)

    // Neutral Wrist Reference Gravity Unit Vector (|neutralG| = 1.0)
    float neutralG[3];           // (gx, gy, gz) in sensor frame
    float angularSpreadDeg;      // Angular spread / stability std during neutral hold (deg)

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
    uint32_t crc32;              // IEEE 802.3 CRC32 over preceding fields
};
#pragma pack(pop)

/**
 * Numerically stable Welford accumulator for computing running mean and variance
 */
struct WelfordAccumulator {
    uint32_t count;
    double mean;
    double M2;

    void reset() {
        count = 0;
        mean = 0.0;
        M2 = 0.0;
    }

    void update(double x) {
        count++;
        double delta = x - mean;
        mean += delta / (double)count;
        double delta2 = x - mean;
        M2 += delta * delta2;
    }

    double getMean() const {
        return mean;
    }

    double getVariance() const {
        return (count > 1) ? (M2 / (double)count) : 0.0;
    }

    double getStdDev() const {
        return sqrt(getVariance());
    }
};

class PersonalCalibration {
public:
    PersonalCalibration();

    /**
     * Initialize NVS storage, load stored calibration profile, verify CRC32 and schema version 2.
     * Falls back to conservative default profile if uncalibrated or corrupted.
     */
    bool init();

    /**
     * Calibration Wizard Control
     */
    void startWizard();
    void confirmReady();
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
    CalibrationProfile _stagingProfile;

#if defined(ARDUINO_ARCH_ESP32)
    Preferences _prefs;
#endif

    char _failureReason[128];
    uint32_t _stepStartTimeMs;
    uint8_t _consecutiveBadSamples;

    // Running accumulators for statistical moments using Welford's algorithm
    WelfordAccumulator _gxAccumulator;
    WelfordAccumulator _gyAccumulator;
    WelfordAccumulator _gzAccumulator;
    WelfordAccumulator _adcAccumulator;

    void advanceStep();
    void resetAccumulators();
    void failCalibration(const char* reason);
};

#endif // CALIBRATION_H
