# GRIPGUARD — Sensor Reliability & Fault Detection Architecture

This document details the transparent, explainable diagnostic engine designed to detect, classify, and handle wearable sensor faults in real time.

---

## 1. Core Philosophy: Honest & Explainable Sensing

Most wearable fitness devices pretend sensor data is always pristine, resulting in phantom rep counts or inaccurate health metrics when wires loosen or components degrade.

GRIPGUARD enforces **honest sensing**:
1. When sensor signals degrade or dropout, repetitions are paused or qualified rather than reporting fabricated counts.
2. The user is informed with transparent, human-readable diagnostic messages (e.g., "MPU6050 I2C communication lost", "FSR resting baseline shifted > 400 counts").
3. A $1500\text{ ms}$ continuous stability gate prevents contact bounce or intermittent chattering from corrupting session history.

---

## 2. The 6-State Sensor Health Model

```
               +-------------------------------------------+
               |           HEALTH_STATE_HEALTHY            |
               | (All sensors nominal, 100% confidence)    |
               +-----+-------------------------------+-----+
                     |                               ^
        Minor drift  |                               | 1500 ms continuous
        or noise     v                               | stability verified
               +-----+-----------------+             |
               | HEALTH_STATE_DEGRADED |             |
               | (Qualified, 70% conf) |             |
               +-----+-----------------+             |
                     |                               |
       Signal locked |                               |
       or anomalous  v                               |
         +-----------+-------------------+           |
         | HEALTH_STATE_SUSPECTED_FAULT  |           |
         | (Rep counting paused, 20% conf|           |
         +-----------+-------------------+           |
                     |                               |
       Bus timeout   |                               |
       or disconnect v                               |
         +-----------+-------------------+           |
         | HEALTH_STATE_SENSOR_UNAVAILABLE           |
         | (Total blackout, 0% confidence|           |
         +-----------+-------------------+           |
                     |                               |
                     | Signal returns to nominal     |
                     v                               |
         +-----------+-------------------+           |
         |     HEALTH_STATE_RECOVERING   |           |
         | (1500 ms stability hold timer)+-----------+
         +-------------------------------+
```

### Health State Permitted Operations Matrix

| Health State | Rep Counting Permitted | Telemetry Qualified | Confidence Score | Suggested Recovery Action |
|---|---|---|---|---|
| `HEALTHY` | **YES** | No | $100\%$ | Normal workout operation |
| `DEGRADED` | **YES** | Yes (Flagged) | $70\%$ | Minor drift; hold wrist still between sets |
| `SUSPECTED_FAULT` | **NO (GATED)** | Yes (Invalid) | $20\%$ | Check physical glove wires and sensor mounting |
| `SENSOR_UNAVAILABLE`| **NO (GATED)** | Yes (Blackout)| $0\%$ | Verify I2C connections (GPIO 21/22) and 3V3 rail |
| `RECOVERING` | **NO (HOLD)** | Yes (Pending) | $50\%$ | Keep glove steady during 1.5s stability verification |
| `CALIBRATION_REQUIRED`| **NO (GATED)**| Yes (Shifted) | $30\%$ | Run 5-step calibration wizard (`c` command) |

---

## 3. Four Sensor Fault Detection Classes

### 3.1 Dropout Detection
- **MPU6050:**
  - Evaluates consecutive I2C NACKs / transmission failures from `Wire.endTransmission()`.
  - Condition: $\ge 4$ consecutive errors OR timestamp gap $> 100\text{ ms}$ without a successful read.
  - Action: Transitions immediately to `SENSOR_UNAVAILABLE`.
- **FSR ADC:**
  - Detects rail saturation: $V_{ADC} \ge 3.28\text{V}$ ($ADC \ge 4085$) while resting indicates terminal short circuit.

### 3.2 Drift Detection
- **Gyroscope Zero-Rate Bias Drift:**
  - Continuous tracking of $\|\vec{b}_{gyro}\| = \sqrt{b_x^2 + b_y^2 + b_z^2}$.
  - Threshold: If $\|\vec{b}_{gyro}\| > 12.0^\circ/\text{s}$, flags `FAULT_DRIFT_GYRO_BIAS` and marks state as `DEGRADED`.
- **FSR Resting Baseline Drift:**
  - When the wrist is stationary ($> 2.0\text{ s}$) and unloaded, compares current filtered ADC against `calib.fsrRelaxedMeanADC`.
  - Threshold: If $|ADC - ADC_{calib}| > 450$ counts, flags `FAULT_DRIFT_FSR_BASELINE` and requires recalibration.

### 3.3 Context-Aware Stuck-At Detection
- Evaluates rolling variance over a 50-sample ($1.0\text{ s}$) window.
- **Why Context Matters:**
  A resting hand or steady grip naturally produces low variance. Traditional thresholding would generate hundreds of false alarms during rest intervals.
- **GRIPGUARD Cross-Sensor Contextual Logic:**
  1. *IMU Stuck Check:* Flagged *only* if IMU pitch variance is $\sigma^2 < 0.0001$ while FSR pressure variance is high ($\sigma^2 > 400\text{ counts}^2$, proving the user is actively squeezing).
  2. *FSR Stuck Check:* Flagged *only* if FSR variance is $\sigma^2 < 0.05$ while IMU pitch variance is high ($\sigma^2 > 35\text{ deg}^2$, proving dynamic lifting motion).

### 3.4 Gain Loss Detection
- Compares peak dynamic range during completed reps against calibrated firm grip reference.
- If repeated reps show $< 25\%$ of calibrated force under verified active workout, reports `FAULT_GAIN_LOSS_SUSPECTED` (suggesting loose glove strap or shifting sensor).

---

## 4. The 1500 ms Stability Recovery Gate

When communication is restored following an I2C blackout or sensor disconnect:
1. The engine transitions to `HEALTH_STATE_RECOVERING`.
2. A monotonic timer (`_recoveryStartTimeMs`) begins counting.
3. Repetition counting remains strictly **paused**.
4. The sensor must provide $1500\text{ ms}$ of continuous, zero-error, nominal samples.
5. If an error occurs during the window, the timer resets.
6. Once the $1500\text{ ms}$ gate is satisfied, the system smoothly transitions to `HEALTHY`.
