# GRIPGUARD — Repetition State Machine & Movement Validation Engine

This document provides complete technical specifications for the deterministic 8-state finite state machine (FSM) that validates repetitions, enforces personalized grip criteria, and classifies rejected movement attempts.

---

## 1. Core Philosophy: Why Motion Alone is Insufficient

Ordinary consumer rep counters rely solely on IMU acceleration peaks or zero-crossings. This causes severe failure modes:
1. **Accidental Jolts:** Quick hand gestures or scratching the nose register as completed reps.
2. **Partial Reps ("Cheating"):** Shallow $15^\circ$ half-curls are counted identically to full $60^\circ$ reps.
3. **Ghost Reps Without Weight:** Swinging the arm freely without gripping a barbell or dumbbell is counted.
4. **Double Counting:** High-velocity turnaround deceleration causes multiple peak crossings for a single physical rep.

GRIPGUARD solves this by integrating:
- **Calibrated Neutral Posture Verification:** Movement must begin and terminate within the user's calibrated neutral posture.
- **Biomechanical Direction & ROM Checks:** Excursion must cross the minimum Range of Motion ($ROM_{min}$) along the primary exercise axis.
- **Dynamic Grip-Eligibility Gating:** The user must maintain active grip ($\ge 35\%\text{--}45\%$ of firm squeeze) throughout the concentric lift phase.
- **Refractory Suppression:** A $450\text{ ms}$ dead-time prevents turnaround rebound double-counting.
- **Sensor Health Reliability Gating:** Repetitions executed during sensor faults or communication dropouts are automatically rejected.

---

## 2. The 8-State Finite State Machine Diagram

```
                 +---------------------------------------------+
                 |               STATE: IDLE                   |
                 |      (Waiting for neutral alignment)        |
                 +----------------------+----------------------+
                                        | Excursion <= ReturnTolerance
                                        v
                 +---------------------------------------------+
                 |               STATE: READY                  |
                 |        (Poised at starting posture)         |
                 +----------------------+----------------------+
                                        | Excursion > 12 deg AND
                                        | Gyro Mag > 18 deg/s (Onset)
                                        v
                 +---------------------------------------------+
                 |       STATE: CONCENTRIC_OR_OUTBOUND         |
                 |   (Ascending phase; track ROM & grip)       |
                 +----------------------+----------------------+
                                        | Excursion >= 22 deg AND
                                        | Gyro Mag < 12 deg/s (Inflection)
                                        v
                 +---------------------------------------------+
                 |             STATE: TRANSITION               |
                 |         (Top of movement / turnaround)      |
                 +----------------------+----------------------+
                                        | Excursion drops 4 deg below peak
                                        v
                 +---------------------------------------------+
                 |        STATE: ECCENTRIC_OR_RETURN           |
                 |       (Descending phase back to start)      |
                 +----------------------+----------------------+
                                        | Excursion <= ReturnTolerance
                                        v
                 +---------------------------------------------+
                 |           STATE: REP_CANDIDATE              |
                 |       (Evaluate all validation rules)       |
                 +----------------------+----------------------+
                                        |
                   +--------------------+--------------------+
                   | All Rules Satisfied                     | Any Rule Violated
                   v                                         v
    +------------------------------+          +------------------------------+
    |      STATE: VALID_REP        |          |     STATE: REJECTED_REP      |
    | - Increment totalValidReps   |          | - Increment totalRejected    |
    | - Update acceptance ratio    |          | - Record explainable reason  |
    +--------------+---------------+          +--------------+---------------+
                   |                                         |
                   +--------------------+--------------------+
                                        | Latch for 1 frame
                                        | Start 450 ms Refractory Timer
                                        v
                                  (Back to IDLE)
```

---

## 3. Exercise Profiles & Biomechanical Parameters

GRIPGUARD supports multiple exercises via configurable profiles:

| Profile | Primary Axis | Min ROM ($\theta_{min}$) | Min Duration ($T_{min}$) | Max Duration ($T_{max}$) | Min Grip Ratio | Return Tolerance |
|---|---|---|---|---|---|---|
| **Bicep Curl** | Forearm Pitch | $40.0^\circ$ | $700\text{ ms}$ | $5000\text{ ms}$ | $0.35$ ($35\%$) | $\pm 14.0^\circ$ |
| **Wrist Extension** | Wrist Pitch | $25.0^\circ$ | $500\text{ ms}$ | $4000\text{ ms}$ | $0.40$ ($40\%$) | $\pm 10.0^\circ$ |
| **Dumbbell Press** | Compound 2D | $45.0^\circ$ | $800\text{ ms}$ | $6000\text{ ms}$ | $0.45$ ($45\%$) | $\pm 15.0^\circ$ |

---

## 4. Rejection Taxonomy & Diagnostic Explanations

When a repetition attempt fails, the engine assigns an explainable reason code:

1. `REJECT_INSUFFICIENT_ROM`
   - **Trigger:** Peak excursion reached $< \theta_{min}$ (e.g. only $28^\circ$ of a required $40^\circ$ bicep curl).
   - **Feedback:** "Partial movement detected. Extend range of motion to complete the rep."
2. `REJECT_DURATION_TOO_SHORT`
   - **Trigger:** Total duration $< T_{min}$ (e.g. completed in $320\text{ ms}$).
   - **Feedback:** "Ballistic jolt or accidental hand movement detected. Perform deliberate, controlled reps."
3. `REJECT_DURATION_TOO_LONG`
   - **Trigger:** Movement stalled or held in flight $> T_{max}$ (e.g. stalled for $> 5.0\text{ s}$).
   - **Feedback:** "Repetition duration exceeded safe limits. Rest between attempts."
4. `REJECT_INSUFFICIENT_GRIP`
   - **Trigger:** Normalized grip fell below minimum threshold during concentric phase, or average grip $< \text{MinGripRatio}$.
   - **Feedback:** "Grip loosened or weight released during concentric lift phase."
5. `REJECT_INCOMPLETE_RETURN`
   - **Trigger:** User stopped before returning within the return tolerance window of their calibrated starting posture.
   - **Feedback:** "Movement did not return fully to the neutral starting position."
6. `REJECT_SENSOR_UNRELIABLE`
   - **Trigger:** MPU6050 I2C bus error, packet dropout, or FSR saturation occurred during rep execution.
   - **Feedback:** "Assessment qualified: Sensor communication was unreliable during this repetition."
