#ifndef SMARTGLOVE_CONFIG_H
#define SMARTGLOVE_CONFIG_H

#include <Arduino.h>

/**
 * ============================================================================
 * GRIPGUARD SMART WEIGHTLIFTING GLOVE — SYSTEM CONFIGURATION
 * ============================================================================
 * Centralized configuration parameters with units, default values, valid ranges,
 * calibration justification, and trade-off analysis (false positives vs false negatives).
 */

namespace Config {

    // ========================================================================
    // 1. HARDWARE PIN DEFINITIONS (STRICT HARDWARE CONSTRAINTS)
    // ========================================================================
    namespace Hardware {
        // MPU6050 I2C Connections
        constexpr uint8_t PIN_I2C_SDA           = 21;    // ESP32 default SDA
        constexpr uint8_t PIN_I2C_SCL           = 22;    // ESP32 default SCL
        constexpr uint32_t I2C_CLOCK_SPEED_HZ   = 400000;// Fast-mode I2C (400 kHz)
        constexpr uint8_t MPU6050_PRIMARY_ADDR  = 0x68;  // AD0 low
        constexpr uint8_t MPU6050_ALT_ADDR      = 0x69;  // AD0 high

        // FSR Analog Input Connection
        // GPIO 34 is an INPUT-ONLY pin on ADC1 (ADC1_CH6).
        // NEVER configure as OUTPUT or attempt internal pull-up/pull-down!
        constexpr uint8_t PIN_FSR_ADC           = 34;
        
        // Voltage Divider Reference Resistor (Ohms)
        // Fixed resistor from GPIO 34 to GND is 10 kOhm (10000 Ohms).
        constexpr float R_DIVIDER_OHMS          = 10000.0f;
        constexpr float VCC_VOLTS               = 3.30f;
        
        // ESP32 ADC Resolution: 12-bit (0 - 4095)
        constexpr uint16_t ADC_MAX_COUNT        = 4095;
    }

    // ========================================================================
    // 2. TIMING & SAMPLING PARAMETERS
    // ========================================================================
    namespace Timing {
        /**
         * Purpose: Sampling interval between consecutive sensor readings.
         * Units: Microseconds (us) / Milliseconds (ms)
         * Default: 20000 us (50 Hz)
         * Valid Range: 10000 us (100 Hz) to 50000 us (20 Hz)
         * Justification: Human exercise dynamics operate below 10 Hz. 50 Hz provides
         * 5x Nyquist oversampling for clean numerical integration without saturating
         * the ESP32 processing budget or I2C bus bandwidth.
         * Trade-off: Lower interval increases CPU load and battery drain; higher interval
         * risks aliasing rapid turnover transitions and increases integration error.
         */
        constexpr uint32_t SAMPLE_INTERVAL_US   = 20000; // 50 Hz
        constexpr float DT_SECONDS              = 0.020f;// 20 ms
        
        /**
         * Purpose: Serial telemetry streaming interval for Milestone 1 debug.
         * Units: Milliseconds (ms)
         * Default: 100 ms (10 Hz)
         */
        constexpr uint32_t SERIAL_DEBUG_INTERVAL_MS = 100;

        /**
         * Purpose: Number of consecutive ADC readings in a single burst sample.
         * Units: Samples (integer)
         * Default: 8
         * Valid Range: 4 to 16
         * Justification: ESP32 SAR ADC has thermal noise (~20-40 counts). 8-sample
         * burst with trimmed mean provides noise reduction of ~sqrt(N) with minimal latency (~400 us).
         */
        constexpr uint8_t FSR_OVERSAMPLE_COUNT  = 8;
        constexpr uint32_t FSR_OVERSAMPLE_DELAY_US = 50;
    }

    // ========================================================================
    // 3. SIGNAL PROCESSING & FILTERING PARAMETERS
    // ========================================================================
    namespace Filtering {
        /**
         * Purpose: Complementary filter weight (alpha) for gyro vs accelerometer fusion.
         * Units: Unitless ratio [0.0 - 1.0]
         * Default: 0.96f
         * Valid Range: 0.90f to 0.99f
         * Justification: High gyro weight preserves dynamic angle tracking during rapid reps,
         * while the 4% accelerometer component slowly corrects low-frequency drift.
         */
        constexpr float COMPLEMENTARY_ALPHA     = 0.96f;

        /**
         * Purpose: Exponential moving average (EWMA) smoothing factor for FSR grip.
         * Units: Unitless ratio [0.0 - 1.0]
         * Default: 0.35f
         * Valid Range: 0.10f to 0.70f
         * Justification: Rapid onset response with moderate smoothing to avoid false dips.
         */
        constexpr float FSR_EWMA_ALPHA          = 0.35f;

        /**
         * Purpose: Rolling buffer size for stationary variance detection.
         * Units: Number of samples (at 50 Hz, 50 samples = 1.0 second)
         * Default: 50
         */
        constexpr uint8_t STATIONARY_WINDOW_SIZE = 50;

        /**
         * Purpose: Gyroscope angular velocity threshold for declaring stationary state.
         * Units: Degrees per second (deg/s)
         * Default: 2.5 deg/s
         * Valid Range: 1.0 to 6.0 deg/s
         * Trade-off: Too low -> hand tremors prevent stationary detection; too high -> drift adapts during active slow movement.
         */
        constexpr float STATIONARY_GYRO_THRESH_DPS = 2.5f;

        /**
         * Purpose: Accelerometer variance threshold for stationary state.
         * Units: (m/s^2)^2
         * Default: 0.08f
         * Valid Range: 0.02f to 0.25f
         */
        constexpr float STATIONARY_ACCEL_VAR_THRESH = 0.08f;
    }

    // ========================================================================
    // 4. SENSOR HEALTH & FAULT DETECTION PARAMETERS
    // ========================================================================
    namespace Health {
        /**
         * Purpose: Timeout for MPU6050 I2C bus response before declaring communication failure.
         * Units: Milliseconds (ms)
         * Default: 100 ms (5 missed samples at 50 Hz)
         * Valid Range: 40 ms to 500 ms
         */
        constexpr uint32_t I2C_DROPOUT_TIMEOUT_MS = 100;
        constexpr uint8_t MAX_CONSECUTIVE_I2C_ERRORS = 4;

        /**
         * Purpose: Stuck-at fault observation window.
         * Units: Milliseconds (ms)
         * Default: 3000 ms (3.0 seconds)
         * Valid Range: 1500 ms to 10000 ms
         * Justification: If sensor variance is zero while the user is actively exercising,
         * or if readings are pinned to rail without noise, stuck-at is flagged.
         */
        constexpr uint32_t STUCK_AT_WINDOW_MS = 3000;
        constexpr float MIN_EXPECTED_ADC_VARIANCE = 0.5f;

        /**
         * Purpose: Gyroscope zero-rate bias drift warning threshold.
         * Units: Degrees per second (deg/s)
         * Default: 12.0 deg/s deviation from calibrated bias.
         */
        constexpr float MAX_PERMISSIBLE_BIAS_DRIFT_DPS = 12.0f;

        /**
         * Purpose: Required duration of continuous stable, fault-free data before recovery.
         * Units: Milliseconds (ms)
         * Default: 1500 ms (1.5 seconds)
         * Justification: Prevents chattering back into HEALTHY state during intermittent contact issues.
         */
        constexpr uint32_t RECOVERY_STABILITY_TIME_MS = 1500;
    }

    // ========================================================================
    // 5. CALIBRATION DEFAULT BOUNDS
    // ========================================================================
    namespace CalibrationDefaults {
        constexpr uint32_t NEUTRAL_RECORD_DURATION_MS = 3000; // 3 seconds
        constexpr uint32_t GRIP_RECORD_DURATION_MS    = 2000; // 2 seconds
        
        // Minimum statistical separation ratio between relaxed and firm grip
        // Separation Ratio = (Mean_Firm - Mean_Relaxed) / (Std_Firm + Std_Relaxed)
        constexpr float MIN_ACCEPTABLE_SEPARATION_RATIO = 3.0f;

        // Default fallback thresholds if no calibration is stored
        constexpr uint16_t FALLBACK_FSR_RELAXED_ADC = 350;
        constexpr uint16_t FALLBACK_FSR_FIRM_ADC    = 2200;
        constexpr float FALLBACK_PITCH_NEUTRAL_DEG  = 0.0f;
        constexpr float FALLBACK_ROLL_NEUTRAL_DEG   = 0.0f;
    }

    // ========================================================================
    // 6. REPETITION STATE MACHINE PARAMETERS
    // ========================================================================
    namespace RepEngineDefaults {
        /**
         * Purpose: Minimum and maximum repetition durations.
         * Units: Milliseconds (ms)
         * Defaults: Min = 600 ms, Max = 5000 ms
         * Justification: Typical deliberate weightlifting repetitions take 1.0 to 4.0 seconds.
         * Reps under 600 ms are accidental jolts; reps over 5000 ms indicate a paused/stalled bar.
         */
        constexpr uint32_t MIN_REP_DURATION_MS = 600;
        constexpr uint32_t MAX_REP_DURATION_MS = 5000;

        /**
         * Purpose: Minimum Range of Motion (ROM) in degrees for valid repetition.
         * Units: Degrees (deg)
         * Default: 35.0 deg
         * Valid Range: 20.0 to 90.0 deg
         */
        constexpr float MIN_ROM_DEGREES = 35.0f;

        /**
         * Purpose: Minimum grip percentage required during repetition concentric phase.
         * Units: Percentage of calibrated firm grip [0.0 - 1.0]
         * Default: 0.40f (40% firm grip required)
         */
        constexpr float MIN_GRIP_ELIGIBILITY_RATIO = 0.40f;

        /**
         * Purpose: Refractory dead-time after a rep completion before another rep can begin.
         * Units: Milliseconds (ms)
         * Default: 450 ms
         * Justification: Prevents duplicate double-counting from mechanical oscillation at turnaround.
         */
        constexpr uint32_t REFRACTORY_PERIOD_MS = 450;
    }

} // namespace Config

#endif // SMARTGLOVE_CONFIG_H
