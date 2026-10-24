"""
GRIPGUARD — Repetition State Machine & Fault Gating Simulation Test
===================================================================
Offline verification script testing the 8-state movement FSM,
dynamic grip eligibility, and sensor reliability gating against
synthesized biomechanical trajectories.
"""

import math
import sys

class RepetitionEngineSim:
    def __init__(self):
        self.state = "IDLE"
        self.valid_reps = 0
        self.rejected_reps = 0
        self.last_rejection_reason = "NONE"
        self.last_rep_duration_ms = 0
        self.last_rep_peak_rom = 0.0
        self.last_rep_avg_grip = 0.0

        # In-flight tracking
        self.rep_start_time_ms = 0
        self.peak_excursion = 0.0
        self.grip_accum = 0.0
        self.grip_samples = 0
        self.grip_failed = False
        self.sensor_fault = False
        self.last_rep_end_time_ms = -10000

        # Profile parameters (Bicep Curl)
        self.min_rom = 40.0
        self.min_duration_ms = 700
        self.max_duration_ms = 5000
        self.min_grip_ratio = 0.35
        self.return_tolerance = 14.0
        self.refractory_period_ms = 450

    def update(self, pitch_deg, gyro_mag_dps, norm_grip, now_ms, sensor_health_ok=True):
        excursion = abs(pitch_deg)

        if not sensor_health_ok:
            self.sensor_fault = True

        if self.state in ["VALID_REP", "REJECTED_REP"]:
            self.state = "IDLE"
            return

        if now_ms - self.last_rep_end_time_ms < self.refractory_period_ms:
            self.state = "IDLE"
            return

        if self.state == "IDLE":
            if excursion <= self.return_tolerance:
                self.state = "READY"

        elif self.state == "READY":
            if excursion > 12.0 and gyro_mag_dps > 18.0:
                self.rep_start_time_ms = now_ms
                self.peak_excursion = excursion
                self.grip_accum = norm_grip
                self.grip_samples = 1
                self.grip_failed = (norm_grip < self.min_grip_ratio)
                self.sensor_fault = not sensor_health_ok
                self.state = "CONCENTRIC"
            elif excursion > self.return_tolerance + 8.0:
                self.state = "IDLE"

        elif self.state == "CONCENTRIC":
            if excursion > self.peak_excursion:
                self.peak_excursion = excursion
            self.grip_accum += norm_grip
            self.grip_samples += 1
            if norm_grip < self.min_grip_ratio:
                self.grip_failed = True

            if excursion >= 22.0 and gyro_mag_dps < 12.0:
                self.state = "TRANSITION"

            if (now_ms - self.rep_start_time_ms) > self.max_duration_ms:
                self._finalize(False, "DURATION_TOO_LONG", now_ms)

        elif self.state == "TRANSITION":
            if excursion > self.peak_excursion:
                self.peak_excursion = excursion
            if excursion < (self.peak_excursion - 4.0):
                self.state = "ECCENTRIC"
            if (now_ms - self.rep_start_time_ms) > self.max_duration_ms:
                self._finalize(False, "DURATION_TOO_LONG", now_ms)

        elif self.state == "ECCENTRIC":
            self.grip_accum += norm_grip
            self.grip_samples += 1
            if excursion <= self.return_tolerance:
                self.state = "EVALUATING"
                self._evaluate_candidate(now_ms)
            if (now_ms - self.rep_start_time_ms) > self.max_duration_ms:
                self._finalize(False, "DURATION_TOO_LONG", now_ms)

    def _evaluate_candidate(self, now_ms):
        duration = now_ms - self.rep_start_time_ms
        avg_grip = (self.grip_accum / self.grip_samples) if self.grip_samples > 0 else 0.0

        if self.sensor_fault:
            self._finalize(False, "SENSOR_UNRELIABLE", now_ms, duration, avg_grip)
        elif duration < self.min_duration_ms:
            self._finalize(False, "DURATION_TOO_SHORT", now_ms, duration, avg_grip)
        elif duration > self.max_duration_ms:
            self._finalize(False, "DURATION_TOO_LONG", now_ms, duration, avg_grip)
        elif self.peak_excursion < self.min_rom:
            self._finalize(False, "INSUFFICIENT_ROM", now_ms, duration, avg_grip)
        elif self.grip_failed or avg_grip < self.min_grip_ratio:
            self._finalize(False, "INSUFFICIENT_GRIP", now_ms, duration, avg_grip)
        else:
            self._finalize(True, "NONE", now_ms, duration, avg_grip)

    def _finalize(self, is_valid, reason, now_ms, duration=0, avg_grip=0.0):
        self.last_rep_duration_ms = duration
        self.last_rep_peak_rom = self.peak_excursion
        self.last_rep_avg_grip = avg_grip
        self.last_rejection_reason = reason
        self.last_rep_end_time_ms = now_ms

        if is_valid:
            self.valid_reps += 1
            self.state = "VALID_REP"
        else:
            self.rejected_reps += 1
            self.state = "REJECTED_REP"

# Test Fixture Runner
def run_tests():
    print("=" * 65)
    print("  GRIPGUARD REPETITION & FAULT GATING TEST SUITE")
    print("=" * 65)

    passed = 0
    total = 0

    # --- Test 1: Complete Valid Repetition ---
    total += 1
    engine = RepetitionEngineSim()
    # 50 Hz simulation (20 ms steps) over 2.0 seconds
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        pitch = math.sin(phase) * 55.0  # 0 -> 55 -> 0 deg
        gyro = abs(math.cos(phase) * 60.0)
        grip = 0.70  # Firm grip (70%)
        engine.update(pitch, gyro, grip, t)
    
    if engine.valid_reps == 1 and engine.rejected_reps == 0:
        print("[PASS] Test 1: Complete Valid Repetition correctly accepted (Valid=1)")
        passed += 1
    else:
        print(f"[FAIL] Test 1: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 2: Incomplete / Partial Repetition (Insufficient ROM) ---
    total += 1
    engine = RepetitionEngineSim()
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        pitch = math.sin(phase) * 28.0  # Peak only 28 deg (< 40 deg min ROM)
        gyro = abs(math.cos(phase) * 35.0)
        grip = 0.65
        engine.update(pitch, gyro, grip, t)

    if engine.valid_reps == 0 and engine.rejected_reps == 1 and engine.last_rejection_reason == "INSUFFICIENT_ROM":
        print("[PASS] Test 2: Partial Repetition correctly rejected (Reason: INSUFFICIENT_ROM)")
        passed += 1
    else:
        print(f"[FAIL] Test 2: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 3: Ballistic Jolt / Accidental Hand Gesture (< 700 ms) ---
    total += 1
    engine = RepetitionEngineSim()
    for t in range(0, 450, 20):
        phase = (t / 450.0) * math.pi
        pitch = math.sin(phase) * 45.0
        gyro = abs(math.cos(phase) * 120.0)
        grip = 0.50
        engine.update(pitch, gyro, grip, t)

    if engine.valid_reps == 0 and engine.rejected_reps == 1 and engine.last_rejection_reason == "DURATION_TOO_SHORT":
        print("[PASS] Test 3: Ballistic Jolt correctly rejected (Reason: DURATION_TOO_SHORT)")
        passed += 1
    else:
        print(f"[FAIL] Test 3: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 4: Free-Arm Movement Without Grip (Insufficient Grip) ---
    total += 1
    engine = RepetitionEngineSim()
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        pitch = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0)
        grip = 0.12  # Relaxed fingers, only 12% grip (< 35% threshold)
        engine.update(pitch, gyro, grip, t)

    if engine.valid_reps == 0 and engine.rejected_reps == 1 and engine.last_rejection_reason == "INSUFFICIENT_GRIP":
        print("[PASS] Test 4: Movement without weight correctly rejected (Reason: INSUFFICIENT_GRIP)")
        passed += 1
    else:
        print(f"[FAIL] Test 4: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 5: Repetition During Sensor Dropout / Reliability Fault ---
    total += 1
    engine = RepetitionEngineSim()
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        pitch = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0)
        grip = 0.70
        # Simulated I2C communication fault halfway through rep
        health_ok = False if (600 <= t <= 1200) else True
        engine.update(pitch, gyro, grip, t, sensor_health_ok=health_ok)

    if engine.valid_reps == 0 and engine.rejected_reps == 1 and engine.last_rejection_reason == "SENSOR_UNRELIABLE":
        print("[PASS] Test 5: Repetition during sensor fault correctly rejected (Reason: SENSOR_UNRELIABLE)")
        passed += 1
    else:
        print(f"[FAIL] Test 5: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 6: Refractory Dead-Time Lockout (Turnaround Double-Counting) ---
    total += 1
    engine = RepetitionEngineSim()
    # First rep (0 - 2000 ms)
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        pitch = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0)
        engine.update(pitch, gyro, 0.70, t)

    # Immediately trigger secondary oscillation at 2100 ms (< 450 ms refractory)
    for t in range(2000, 2350, 20):
        phase = ((t - 2000) / 350.0) * math.pi
        pitch = math.sin(phase) * 20.0
        gyro = abs(math.cos(phase) * 40.0)
        engine.update(pitch, gyro, 0.70, t)

    if engine.valid_reps == 1 and engine.rejected_reps == 0:
        print("[PASS] Test 6: Refractory lockout strictly prevented duplicate rebound count")
        passed += 1
    else:
        print(f"[FAIL] Test 6: Valid={engine.valid_reps}, Rej={engine.rejected_reps}")

    print("=" * 65)
    print(f"Results: {passed} / {total} Passed ({passed/total*100:.1f}%)")
    print("=" * 65)

    return passed == total

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
