# GRIPGUARD — Scientific Limitations & Boundary Conditions

To maintain scientific rigor and avoid deceptive or unsubstantiated claims, this document explicitly outlines the physical, mechanical, and algorithmic boundary conditions of the two-sensor wearable prototype.

---

## 1. What This System Measures (And What It Does NOT)

### 1.1 What Is Measured & Validated:
- **Wrist / Forearm Orientation & Kinematics:** 6-axis acceleration and angular rate tracked via MPU6050, fused into pitch and roll tilt angles relative to gravitational neutral.
- **Relative Palmar Grip Compression:** Proportional voltage change across a single Force Sensitive Resistor (FSR) with a 10 kΩ voltage divider.
- **Biomechanical Phase Timing & Excursion:** Bounded Range of Motion, turnaround inflection, duration limits, and return-to-start verification.
- **Sensor Reliability Intelligence:** Observable sensor dropout, resting baseline drift, contextual stuck-at silicon states, and assessment qualification.

### 1.2 What CANNOT Be Claimed (Scientific Non-Claims):
1. **NO Absolute Force Measurement (Newtons or kg):**
   - FSRs exhibit mechanical hysteresis, non-linear force-resistance transfer functions, and sensitivity variations based on contact surface curvature. The system measures **relative grip compression percentage** normalized against a calibrated personal squeeze, NOT calibrated absolute Newtons.
2. **NO Barbell / Dumbbell Weight Estimation:**
   - A wrist-mounted IMU and single FSR cannot measure the mass of an external barbell. Claiming to measure barbell weight from kinematic acceleration alone violates classical mechanics (due to unknown human muscular mechanical advantage and ground reaction forces).
3. **NO Injury Prevention or Medical Diagnosis Claims:**
   - Wrist deviation beyond neutral is labeled an "excursion deviation from personal reference", NOT an injury prediction. Wearable sensors cannot diagnose carpal tunnel syndrome, tendonitis, or musculoskeletal disorders without clinical evaluation.
4. **NO Muscle Fatigue Measurement (Without EMG):**
   - Measuring true muscular fatigue requires electromyography (sEMG) frequency power spectral shift analysis ($MF$ and $MDF$). Kinematic slowing down during late sets may suggest exertion, but cannot be claimed as direct muscle fatigue measurement.
5. **NO Full-Body Form Analysis:**
   - A single-glove prototype measures wrist and forearm trajectory. It cannot observe lumbar spine rounding during deadlifts, knee valgus during squats, or shoulder asymmetry.

---

## 2. Sensor-Specific Physical Limitations

### 2.1 MPU6050 6-Axis IMU
- **Yaw Drift (Absence of Magnetometer):**
  - The MPU6050 contains a 3-axis accelerometer and 3-axis gyroscope, but no 3-axis magnetometer (compass). Gravitational acceleration provides absolute reference for **Pitch** and **Roll**, but absolute **Yaw** (heading around the gravity vector) will slowly integrate gyro drift over extended minutes.
  - Mitigation: Exercise profiles are designed to operate primarily on Pitch and Roll, and the stationary bias tracking cancels zero-rate gyro drift during resting pauses.
- **Temperature Drift:**
  - MEMS silicon experiences zero-rate offset shifts with temperature changes. Our stationary-gated leaky integrator continuously corrects this during pauses.

### 2.2 Force Sensitive Resistor (FSR)
- **Single-Point Sensing:**
  - The prototype utilizes one physical FSR sensor on the palm. If the barbell knurling shifts pressure to other fingers (e.g. pinky or thumb) without compressing the sensor zone, the FSR may underestimate grip force.
- **Mechanical Settling & Creep:**
  - Polymer thick-film FSRs undergo logarithmic resistance drift when subjected to constant static force over several minutes. This is why GRIPGUARD uses personalized baseline re-zeroing and statistical separation checks.
