# GRIPGUARD — Personalized Calibration & Baseline Intelligence

This document details the theory, protocol, statistical criteria, and non-volatile storage architecture of the personal calibration engine.

---

## 1. Why Personal Calibration is Mandatory

In wearable fitness and rehabilitation sensing, static universal thresholds fail:
1. **Anatomical Differences:** Forearm resting angle, grip strength, and hand geometry differ significantly across individuals.
2. **Glove Sensor Coupling:** Depending on hand size and glove tension, the FSR sensor resting baseline varies between 50 and 800 ADC counts.
3. **Sensor Wear & Aging:** FSR polymers undergo microscopic mechanical settling over repeated cycles, shifting the baseline resistance.

GRIPGUARD solves this by learning individual neutral wrist posture and relaxed/firm grip baselines through a 5-step guided wizard.

---

## 2. The 5-Step Guided Calibration Protocol

```
+-----------------------------------------------------------------------------------+
|                           5-STEP CALIBRATION WIZARD                               |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [STEP 1: PREFLIGHT]        Verify sensor health, stable gravity (8-12 m/s^2),    |
|                             and open-circuit FSR (< 1000 ADC counts).             |
|                                     |                                             |
|                                     v                                             |
|  [STEP 2: NEUTRAL WRIST]    User holds neutral lifting wrist posture stationary   |
|                             for 3.0 seconds (150 samples). Estimates mean pitch,  |
|                             mean roll, and posture stability (StdDev <= 6 deg).   |
|                                     |                                             |
|                                     v                                             |
|  [STEP 3: RELAXED GRIP]     User holds bar/handle with relaxed fingers for 2.0 s  |
|                             (100 samples). Calculates mean relaxed ADC baseline   |
|                             and electronic noise floor.                           |
|                                     |                                             |
|                                     v                                             |
|  [STEP 4: FIRM GRIP]        User applies comfortable firm exercise squeeze for    |
|                             2.0 seconds (100 samples). Calculates mean firm ADC   |
|                             and squeeze variability.                              |
|                                     |                                             |
|                                     v                                             |
|  [STEP 5: VALIDATION]       Computes Statistical Separation Ratio:                |
|                             Sep = (MeanFirm - MeanRelaxed) / (StdFirm + StdRelax) |
|                             Pass condition: Sep >= 3.0 and Range >= 350 counts.   |
|                                     |                                             |
|                   +-----------------+-----------------+                           |
|                   | Pass                              | Fail                      |
|                   v                                   v                           |
|        [CALIB_COMPLETED]                       [CALIB_FAILED]                     |
|  Saved to NVS with CRC32                Display transparent failure reason;       |
|  Derived rep thresholds active          revert to safe conservative defaults      |
+-----------------------------------------------------------------------------------+
```

---

## 3. Mathematical Foundations

### 3.1 Mean & Variance Accumulation
During steps 2, 3, and 4, numerical moments are accumulated over $N$ samples ($50\text{ Hz}$):
$$\bar{x} = \frac{1}{N} \sum_{i=1}^N x_i$$
$$\sigma^2 = \frac{1}{N} \sum_{i=1}^N x_i^2 - \bar{x}^2, \quad \sigma = \sqrt{\max(0, \sigma^2)}$$

### 3.2 Grip Statistical Separation Ratio
To verify that relaxed resting contact and firm gripping can be reliably distinguished without false positives, GRIPGUARD enforces:
$$\text{Separation Ratio} = \frac{\mu_{\text{firm}} - \mu_{\text{relaxed}}}{\sigma_{\text{firm}} + \sigma_{\text{relaxed}}} \ge 3.0$$

#### Scientific Meaning:
- Under Gaussian assumptions, a separation ratio of $\ge 3.0$ guarantees that the overlap between the relaxed and firm grip probability distributions is $< 0.13\%$ ($3\sigma$ confidence interval).
- If the user barely squeezes, or if the sensor is misaligned, the separation ratio will fall below 3.0. The wizard will **reject** the calibration rather than saving an ambiguous threshold.

### 3.3 Derived Repetition Thresholds
Upon successful validation, personalized thresholds are derived dynamically:
$$\text{Grip Eligibility Threshold} = \mu_{\text{relaxed}} + 0.40 \cdot (\mu_{\text{firm}} - \mu_{\text{relaxed}})$$
$$\text{Neutral Posture Tolerance} = \pm 15.0^\circ \text{ relative to } (\bar{\theta}_{\text{pitch}}, \bar{\theta}_{\text{roll}})$$

---

## 4. Non-Volatile Persistence Architecture

Saved profiles persist in ESP32 flash memory across power cycles using the ESP32 `Preferences` library (under namespace `glove_cal`, key `profile`).

### Memory Layout & Integrity Checks
```
Offset   Field                 Type        Description
------------------------------------------------------------------------
0x00     magicHeader           uint32_t    0x47524950 ("GRIP")
0x04     schemaVersion         uint16_t    1 (v1.0)
0x06     calibratedTimeMs      uint32_t    System uptime when saved
0x0A     neutralPitchDeg       float       Mean resting pitch angle (deg)
0x0E     neutralRollDeg        float       Mean resting roll angle (deg)
0x12     postureStabilityStd   float       Wrist stability during hold (deg)
0x16     fsrRelaxedMeanADC     uint16_t    Relaxed baseline ADC (0-4095)
0x18     fsrRelaxedStdDev      float       Noise standard deviation
0x1C     fsrFirmMeanADC        uint16_t    Firm grip target ADC (0-4095)
0x1E     fsrFirmStdDev         float       Firm grip variability
0x22     gripSeparationRatio   float       Statistical separation metric
0x26     gripEligibilityADC    uint16_t    Grip qualification threshold
0x28     postureToleranceDeg   float       Angular window (+/- deg)
0x2C     isValid               bool        Calibration state flag
0x2D     crc32                 uint32_t    IEEE 802.3 32-bit CRC checksum
```

### Safety & Fallback Rules
1. **CRC Verification on Boot:** During initialization, the firmware calculates the CRC32 of the stored blob. If a single bit is corrupted or uninitialized, loading fails immediately.
2. **Version Matching:** If `schemaVersion != 1`, the stored blob is rejected.
3. **Conservative Default Fallback:** If uncalibrated or corrupted, the system falls back to safe constants (`Relaxed = 350`, `Firm = 2200`, `Neutral = 0.0 deg`), and sets `isValid = false`. All telemetry explicitly warns the user that factory fallback thresholds are in use.
