"""
GRIPGUARD — Repetition State Machine & Fault Gating Simulation Test
===================================================================
Offline verification script testing the Phase 3 RepEngine:
- Valley -> Peak -> Valley Prominence tracking
- Stationary bottom re-baselining
- Armed only when grip gate is HIGH; un-gripped motion silently ignored
- 2 consistent candidate reps required to confirm set and back-credit both
- Adaptive ROM (~0.65x median) and adaptive tempo window (median +- 2 sigma)
- Refractory period derived from profile minDurationMs
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

        # Valley prominence tracking
        self.bottom_valley = 0.0
        self.return_valley = 0.0
        self.peak_excursion = 0.0
        self.ascent_prominence = 0.0
        self.descent_prominence = 0.0

        # In-flight tracking
        self.rep_start_time_ms = 0
        self.transition_start_time_ms = 0
        self.last_rep_end_time_ms = -10000
        self.min_grip = 1.0
        self.grip_accum = 0.0
        self.grip_samples = 0
        self.sustained_low_grip_samples = 0
        self.grip_failed = False
        self.sensor_fault = False

        # Debounce counters (3 consecutive samples)
        self.onset_debounce = 0
        self.peak_debounce = 0
        self.return_onset_debounce = 0
        self.return_debounce = 0

        # Profile parameters (Bicep Curl)
        self.min_rom = 40.0
        self.min_duration_ms = 700
        self.max_duration_ms = 5000
        self.min_grip_ratio = 0.35
        self.return_tolerance = 22.0
        self.sustained_low_grip_samples_limit = 15

        self.onset_excursion_deg = 12.0
        self.onset_gyro_dps = 18.0
        self.peak_trans_excursion_deg = 22.0
        self.peak_trans_gyro_dps = 14.0
        self.return_onset_delta_deg = 4.0
        self.concentric_abort_excursion_deg = 8.0
        self.concentric_abort_peak_deg = 22.0
        self.transition_stall_gyro_dps = 14.0
        self.sustained_grip_min_excursion_deg = 20.0
        self.return_settled_excursion_deg = 30.0
        self.return_settled_gyro_dps = 12.0

        # Rule 6: 2-Candidate confirmation & back-crediting
        self.pending_rep1 = None
        self.confirmed_set_started = False

        # Rule 5: Adaptive thresholds buffer
        self.recent_reps = [] # (peakRom, durationMs)
        self.adaptive_min_rom = self.min_rom
        self.adaptive_min_dur = self.min_duration_ms
        self.adaptive_max_dur = self.max_duration_ms

        # Phase 4: Grip-Load Coupling & Signal History
        self.fsr_hp_history = []
        self.gyro_accel_history = []
        self.fsr_slow_ema = 0.0
        self.prev_filtered_gyro_mag = 0.0
        self.concentric_peak_gyro = 0.0
        self.concentric_peak_gyro_time_ms = 0
        self.concentric_start_time_ms = 0
        self.disarmed_samples_count = 0

        # Phase 4: Uncertain Reps Pool & Reconciliation
        self.uncertain_reps = []
        self.last_rep_was_uncertain = False

        # Phase 4: Fatigue Tracking
        self.initial_peak_gyro = 0.0
        self.initial_rep_duration = 0
        self.initial_mean_grip = 0.0
        self.fatigue_baseline_set = False
        self.is_fatigued = False
        self.fatigue_index_pct = 0.0

        # Phase 4 Telemetry
        self.last_rep_coupling_rho = 0.0
        self.last_rep_confidence_pct = 100.0

    def calculate_grip_load_coupling(self):
        sample_count = len(self.fsr_hp_history)
        if sample_count < 20:
            return 0.0, 0

        mean_fsr = sum(self.fsr_hp_history) / sample_count
        mean_accel = sum(self.gyro_accel_history) / sample_count

        var_fsr = sum((x - mean_fsr) ** 2 for x in self.fsr_hp_history)
        var_accel = sum((x - mean_accel) ** 2 for x in self.gyro_accel_history)
        denom = math.sqrt(var_fsr * var_accel)
        if denom < 1e-4:
            return 0.0, 0

        max_rho = -1.0
        best_lag = 0
        for tau in range(min(8, sample_count // 2)):
            count = sample_count - tau
            cross_sum = sum((self.fsr_hp_history[i + tau] - mean_fsr) * (self.gyro_accel_history[i] - mean_accel) for i in range(count))
            r = cross_sum / denom
            if r > max_rho:
                max_rho = r
                best_lag = tau
        return max(-1.0, min(1.0, max_rho)), best_lag

    def evaluate_rep_confidence(self, ascent_rom, duration_ms, rho, signal_noise, best_lag):
        rom_delta = ascent_rom - self.min_rom
        z_rom = max(0.0, min(1.0, 0.5 + (rom_delta / 30.0)))

        dur_delta = abs(duration_ms - (self.min_duration_ms + self.max_duration_ms) * 0.5)
        z_dur = max(0.0, min(1.0, 1.0 - (dur_delta / 3500.0)))

        z_rho = max(0.0, min(1.0, (rho + 0.15) / 0.85))
        z_noise = 1.0 - max(0.0, min(0.4, signal_noise / 1.5))
        z_phase = 1.0 if (1 <= best_lag <= 5) else 0.65

        confidence = (0.30 * z_rho + 0.25 * z_rom + 0.20 * z_dur + 0.15 * z_phase + 0.10 * z_noise) * 100.0
        return max(0.0, min(100.0, confidence))

    def update_fatigue_metrics(self, peak_gyro, duration_ms, avg_grip):
        if not self.fatigue_baseline_set:
            self.initial_peak_gyro = peak_gyro
            self.initial_rep_duration = duration_ms
            self.initial_mean_grip = avg_grip
            self.fatigue_baseline_set = True
            self.is_fatigued = False
            self.fatigue_index_pct = 0.0
            return

        vel_loss = 0.0
        if self.initial_peak_gyro > 10.0 and peak_gyro < self.initial_peak_gyro:
            vel_loss = ((self.initial_peak_gyro - peak_gyro) / self.initial_peak_gyro) * 100.0

        tempo_drift = 0.0
        if self.initial_rep_duration > 0 and duration_ms > self.initial_rep_duration:
            tempo_drift = ((duration_ms - self.initial_rep_duration) / self.initial_rep_duration) * 100.0

        grip_decay = 0.0
        if self.initial_mean_grip > 0.1 and avg_grip < self.initial_mean_grip:
            grip_decay = ((self.initial_mean_grip - avg_grip) / self.initial_mean_grip) * 100.0

        comp_fatigue = 0.45 * vel_loss + 0.35 * tempo_drift + 0.20 * grip_decay
        self.fatigue_index_pct = max(0.0, min(100.0, comp_fatigue))
        self.is_fatigued = (self.fatigue_index_pct >= 35.0 or vel_loss >= 30.0)

    def reconcile_uncertain_reps(self):
        if not self.uncertain_reps or self.valid_reps == 0:
            self.uncertain_reps = []
            return

        mean_tempo = float(self.last_rep_duration_ms)
        if mean_tempo < 500.0:
            mean_tempo = 1500.0

        for rep in self.uncertain_reps:
            delta_tempo = abs(rep["duration"] - mean_tempo)
            if delta_tempo <= 0.25 * mean_tempo:
                self.valid_reps += 1
                self.last_rep_was_uncertain = True
        self.uncertain_reps = []

    def update_adaptive_thresholds(self, rom, dur):
        self.recent_reps.append((rom, dur))
        if len(self.recent_reps) > 5:
            self.recent_reps.pop(0)

        if len(self.recent_reps) >= 3:
            roms = sorted([r[0] for r in self.recent_reps])
            durs = sorted([r[1] for r in self.recent_reps])
            median_rom = roms[len(roms) // 2]
            median_dur = durs[len(durs) // 2]

            self.adaptive_min_rom = max(self.min_rom * 0.5, min(self.min_rom * 1.15, 0.65 * median_rom))

            mean_dur = sum(durs) / len(durs)
            variance = sum((d - mean_dur) ** 2 for d in durs) / len(durs)
            sigma = math.sqrt(variance)

            self.adaptive_min_dur = max(self.min_duration_ms, min(self.max_duration_ms, median_dur - 2.0 * sigma))
            self.adaptive_max_dur = max(self.min_duration_ms, min(self.max_duration_ms, median_dur + 2.0 * sigma))

    def update(self, tilt_deg, gyro_mag_dps, norm_grip, now_ms, grip_gate=True, sensor_health_ok=True, is_stationary=False, fsr_raw=2000.0, signal_noise=0.15):
        excursion = abs(tilt_deg)
        refractory_ms = max(300, self.min_duration_ms // 2)
        dt = 0.020 # 50 Hz

        # In-flight sensor fault -> immediate REJECT_SENSOR_UNRELIABLE
        if not sensor_health_ok:
            self.sensor_fault = True
            if self.state in ["CONCENTRIC", "TRANSITION", "ECCENTRIC"]:
                self._finalize(False, "SENSOR_UNRELIABLE", now_ms, now_ms - self.rep_start_time_ms, self.grip_accum / max(1, self.grip_samples))
                return

        if self.state in ["VALID_REP", "REJECTED_REP"]:
            self.state = "IDLE"
            return

        if now_ms - self.last_rep_end_time_ms < refractory_ms:
            self.state = "IDLE"
            return

        # Check for set-end uncertain rep reconciliation when disarmed > 1.5s (75 samples)
        if not grip_gate:
            self.disarmed_samples_count += 1
            if self.disarmed_samples_count >= 75:
                self.reconcile_uncertain_reps()
        else:
            self.disarmed_samples_count = 0

        if self.state == "IDLE":
            self.onset_debounce = 0
            self.peak_debounce = 0
            self.return_onset_debounce = 0
            self.return_debounce = 0

            # Re-baseline valley at stationary rest
            if is_stationary or (gyro_mag_dps < self.return_settled_gyro_dps and excursion < 25.0):
                self.bottom_valley = excursion

            if excursion <= self.bottom_valley + self.return_tolerance:
                self.state = "READY"

        elif self.state == "READY":
            # Expire pending candidate #1 if second rep is not initiated within 4500 ms
            if self.pending_rep1 is not None and (now_ms - self.last_rep_end_time_ms > 4500):
                self.pending_rep1 = None

            # Rule 3: Armed ONLY while grip gate is HIGH. Motion without grip is silently ignored.
            if not grip_gate:
                self.onset_debounce = 0
                return

            # Continuous re-baselining at stationary bottom
            if is_stationary or (gyro_mag_dps < self.return_settled_gyro_dps and excursion < 25.0):
                self.bottom_valley = excursion

            prominence = excursion - self.bottom_valley
            if prominence > self.onset_excursion_deg and gyro_mag_dps > self.onset_gyro_dps:
                self.onset_debounce += 1
                if self.onset_debounce >= 3:
                    self.onset_debounce = 0
                    self.rep_start_time_ms = now_ms
                    self.concentric_start_time_ms = now_ms
                    self.peak_excursion = excursion
                    self.ascent_prominence = prominence
                    self.return_valley = excursion
                    self.min_grip = norm_grip
                    self.grip_accum = norm_grip
                    self.grip_samples = 1
                    self.sustained_low_grip_samples = 0
                    self.grip_failed = False
                    self.sensor_fault = not sensor_health_ok
                    self.peak_debounce = 0
                    self.fsr_hp_history = []
                    self.gyro_accel_history = []
                    self.fsr_slow_ema = fsr_raw
                    self.prev_filtered_gyro_mag = gyro_mag_dps
                    self.concentric_peak_gyro = gyro_mag_dps
                    self.concentric_peak_gyro_time_ms = 0
                    self.state = "CONCENTRIC"
            else:
                self.onset_debounce = 0
                if excursion > self.bottom_valley + self.return_tolerance + 10.0:
                    self.state = "IDLE"

        elif self.state == "CONCENTRIC":
            # Rule 3: Grip lost mid-rep -> immediate REJECT_INSUFFICIENT_GRIP
            if not grip_gate:
                self._finalize(False, "INSUFFICIENT_GRIP", now_ms)
                return

            # History accumulation for coupling
            self.fsr_slow_ema = 0.95 * self.fsr_slow_ema + 0.05 * fsr_raw
            fsr_hp = fsr_raw - self.fsr_slow_ema
            gyro_accel = abs(gyro_mag_dps - self.prev_filtered_gyro_mag) / dt
            self.prev_filtered_gyro_mag = gyro_mag_dps
            self.fsr_hp_history.append(fsr_hp)
            self.gyro_accel_history.append(gyro_accel)

            if gyro_mag_dps > self.concentric_peak_gyro:
                self.concentric_peak_gyro = gyro_mag_dps
                self.concentric_peak_gyro_time_ms = now_ms - self.concentric_start_time_ms

            if excursion > self.peak_excursion:
                self.peak_excursion = excursion
            self.ascent_prominence = self.peak_excursion - self.bottom_valley

            self.grip_accum += norm_grip
            self.grip_samples += 1
            if norm_grip < self.min_grip:
                self.min_grip = norm_grip

            if excursion >= self.sustained_grip_min_excursion_deg:
                if norm_grip < self.min_grip_ratio:
                    self.sustained_low_grip_samples += 1
                    if self.sustained_low_grip_samples >= self.sustained_low_grip_samples_limit:
                        self.grip_failed = True
                else:
                    self.sustained_low_grip_samples = 0

            # Rule 2: Concentric hysteresis abort path
            current_prominence = excursion - self.bottom_valley
            if current_prominence < self.concentric_abort_excursion_deg and self.ascent_prominence < self.concentric_abort_peak_deg:
                self.peak_debounce = 0
                self.onset_debounce = 0
                self.state = "READY"
                return

            if self.ascent_prominence >= self.peak_trans_excursion_deg and gyro_mag_dps < self.peak_trans_gyro_dps:
                self.peak_debounce += 1
                if self.peak_debounce >= 3:
                    self.peak_debounce = 0
                    self.transition_start_time_ms = now_ms
                    self.return_onset_debounce = 0
                    self.state = "TRANSITION"
            else:
                self.peak_debounce = 0

            if (now_ms - self.rep_start_time_ms) > self.max_duration_ms:
                self._finalize(False, "DURATION_TOO_LONG", now_ms)

        elif self.state == "TRANSITION":
            if not grip_gate:
                self._finalize(False, "INSUFFICIENT_GRIP", now_ms)
                return

            self.fsr_slow_ema = 0.95 * self.fsr_slow_ema + 0.05 * fsr_raw
            fsr_hp = fsr_raw - self.fsr_slow_ema
            gyro_accel = abs(gyro_mag_dps - self.prev_filtered_gyro_mag) / dt
            self.prev_filtered_gyro_mag = gyro_mag_dps
            self.fsr_hp_history.append(fsr_hp)
            self.gyro_accel_history.append(gyro_accel)

            peak_rose = False
            if excursion > self.peak_excursion:
                self.peak_excursion = excursion
                peak_rose = True
            self.ascent_prominence = self.peak_excursion - self.bottom_valley

            self.grip_accum += norm_grip
            self.grip_samples += 1
            if norm_grip < self.min_grip:
                self.min_grip = norm_grip

            if norm_grip < self.min_grip_ratio:
                self.sustained_low_grip_samples += 1
                if self.sustained_low_grip_samples >= self.sustained_low_grip_samples_limit:
                    self.grip_failed = True
            else:
                self.sustained_low_grip_samples = 0

            if gyro_mag_dps > self.transition_stall_gyro_dps or peak_rose:
                self.transition_start_time_ms = now_ms

            if excursion < (self.peak_excursion - self.return_onset_delta_deg):
                self.return_onset_debounce += 1
                if self.return_onset_debounce >= 3:
                    self.return_onset_debounce = 0
                    self.return_debounce = 0
                    self.return_valley = excursion
                    self.state = "ECCENTRIC"
            else:
                self.return_onset_debounce = 0

            if (now_ms - self.transition_start_time_ms > 3000) or ((now_ms - self.rep_start_time_ms) > self.max_duration_ms):
                self._finalize(False, "DURATION_TOO_LONG", now_ms)

        elif self.state == "ECCENTRIC":
            if not grip_gate:
                self._finalize(False, "INSUFFICIENT_GRIP", now_ms)
                return

            self.fsr_slow_ema = 0.95 * self.fsr_slow_ema + 0.05 * fsr_raw
            fsr_hp = fsr_raw - self.fsr_slow_ema
            gyro_accel = abs(gyro_mag_dps - self.prev_filtered_gyro_mag) / dt
            self.prev_filtered_gyro_mag = gyro_mag_dps
            self.fsr_hp_history.append(fsr_hp)
            self.gyro_accel_history.append(gyro_accel)

            if excursion < self.return_valley:
                self.return_valley = excursion
            self.descent_prominence = self.peak_excursion - self.return_valley

            self.grip_accum += norm_grip
            self.grip_samples += 1
            if norm_grip < self.min_grip:
                self.min_grip = norm_grip

            if norm_grip < self.min_grip_ratio:
                self.sustained_low_grip_samples += 1
                if self.sustained_low_grip_samples >= self.sustained_low_grip_samples_limit:
                    self.grip_failed = True
            else:
                self.sustained_low_grip_samples = 0

            return_near_bottom = (excursion <= self.bottom_valley + 10.0)
            below_threshold = (excursion <= self.bottom_valley + self.return_settled_excursion_deg)
            is_at_rest = (gyro_mag_dps < self.return_settled_gyro_dps) or is_stationary
            stationary_completion = (below_threshold and is_at_rest)

            if return_near_bottom or stationary_completion:
                self.return_debounce += 1
                if self.return_debounce >= 3:
                    self.return_debounce = 0
                    self.state = "EVALUATING"
                    self._evaluate_candidate(now_ms, signal_noise)
            else:
                self.return_debounce = 0

            if (now_ms - self.rep_start_time_ms) > self.max_duration_ms:
                if self.ascent_prominence >= self.min_rom:
                    self._finalize(False, "INCOMPLETE_RETURN", now_ms)
                else:
                    self._finalize(False, "DURATION_TOO_LONG", now_ms)

    def _evaluate_candidate(self, now_ms, signal_noise=0.15):
        duration = now_ms - self.rep_start_time_ms
        avg_grip = (self.grip_accum / self.grip_samples) if self.grip_samples > 0 else 0.0

        effective_min_rom = self.adaptive_min_rom if len(self.recent_reps) >= 3 else self.min_rom
        effective_min_dur = self.adaptive_min_dur if len(self.recent_reps) >= 3 else self.min_duration_ms
        effective_max_dur = self.adaptive_max_dur if len(self.recent_reps) >= 3 else self.max_duration_ms

        if self.sensor_fault:
            self._finalize(False, "SENSOR_UNRELIABLE", now_ms, duration, avg_grip)
            return

        if duration < effective_min_dur:
            self._finalize(False, "DURATION_TOO_SHORT", now_ms, duration, avg_grip)
            return

        if duration > effective_max_dur:
            self._finalize(False, "DURATION_TOO_LONG", now_ms, duration, avg_grip)
            return

        # Rule 2: Prominence required on both ascent and descent legs
        if self.ascent_prominence < effective_min_rom or self.descent_prominence < effective_min_rom:
            self._finalize(False, "INSUFFICIENT_ROM", now_ms, duration, avg_grip)
            return

        if self.grip_failed or self.min_grip < self.min_grip_ratio:
            self._finalize(False, "INSUFFICIENT_GRIP", now_ms, duration, avg_grip)
            return

        # Phase 4: Calculate Grip-Load Coupling Score (rho) & Best Lag
        rho, best_lag = self.calculate_grip_load_coupling()
        self.last_rep_coupling_rho = rho

        # Phase 4 Rule 2: Unloaded Movement / Dropped Weight Detection
        loadedness = 0.60 * rho + 0.40 * avg_grip
        if (rho < 0.12 and avg_grip < 0.42) or (loadedness < 0.18):
            self._finalize(False, "UNLOADED", now_ms, duration, avg_grip)
            return

        # Phase 4 Rule 4: Momentum Swing Cheat Detection
        concentric_dur = self.transition_start_time_ms - self.concentric_start_time_ms if self.transition_start_time_ms > self.concentric_start_time_ms else (duration // 2)
        if concentric_dur < 450 and self.concentric_peak_gyro_time_ms < 250 and self.concentric_peak_gyro > 110.0 and rho < 0.22:
            self._finalize(False, "MOMENTUM_SWING", now_ms, duration, avg_grip)
            return

        # Phase 4 Rule 3: Multi-Factor Confidence Scoring
        confidence = self.evaluate_rep_confidence(self.ascent_prominence, duration, rho, signal_noise, best_lag)
        self.last_rep_confidence_pct = confidence

        if confidence < 40.0:
            self._finalize(False, "INSUFFICIENT_ROM", now_ms, duration, avg_grip)
            return

        # Uncertain candidate pool (40% <= confidence < 60%)
        if confidence < 60.0:
            if len(self.uncertain_reps) < 2:
                self.uncertain_reps.append({"duration": duration, "rom": self.ascent_prominence, "grip": avg_grip})
            self._finalize(False, "NONE", now_ms, duration, avg_grip)
            return

        # Update Fatigue Metrics
        self.update_fatigue_metrics(self.concentric_peak_gyro, duration, avg_grip)

        # Rule 6: 2-Candidate confirmation & back-crediting
        if not self.confirmed_set_started:
            if self.pending_rep1 is None:
                # Store Rep 1, do NOT count yet
                self.pending_rep1 = (self.ascent_prominence, duration, avg_grip)
                self.last_rep_end_time_ms = now_ms
                self.state = "IDLE"
                return
            else:
                # Candidate Rep 2 arrived! Verify consistency with Rep 1
                rom1, dur1, _ = self.pending_rep1
                is_consistent = (abs(rom1 - self.ascent_prominence) <= 30.0) and (abs(dur1 - duration) <= 2000)
                if is_consistent:
                    # Back-credit both!
                    self.valid_reps += 2
                    self.confirmed_set_started = True
                    self.update_adaptive_thresholds(rom1, dur1)
                    self.update_adaptive_thresholds(self.ascent_prominence, duration)
                    self.pending_rep1 = None
                    self._finalize(True, "NONE", now_ms, duration, avg_grip, increment=False)
                    return
                else:
                    self.pending_rep1 = None
                    self._finalize(False, "INSUFFICIENT_ROM", now_ms, duration, avg_grip)
                    return
        else:
            self.valid_reps += 1
            self.update_adaptive_thresholds(self.ascent_prominence, duration)
            self._finalize(True, "NONE", now_ms, duration, avg_grip, increment=False)

    def _finalize(self, is_valid, reason, now_ms, duration=0, avg_grip=0.0, increment=True):
        self.last_rep_duration_ms = duration
        self.last_rep_peak_rom = self.ascent_prominence
        self.last_rep_avg_grip = avg_grip
        self.last_rejection_reason = reason
        self.last_rep_end_time_ms = now_ms

        if is_valid:
            if increment:
                self.valid_reps += 1
            self.state = "VALID_REP"
        else:
            if reason != "NONE":
                self.rejected_reps += 1
            self.state = "REJECTED_REP"


def run_tests():
    print("=" * 68)
    print("  GRIPGUARD REPETITION & FAULT GATING TEST SUITE (PHASE 3 ALIGNED)")
    print("=" * 68)

    passed = 0
    total = 0

    # --- Test 1: 2-Candidate Back-crediting Rule (Count nothing on Rep 1, back-credit both on Rep 2) ---
    total += 1
    engine = RepetitionEngineSim()
    # Rep 1 (0 to 2000 ms)
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        tilt = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0) if t < 1800 else 4.0
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)

    # After Rep 1: Count should still be 0, candidate held in pending slot!
    rep1_held = (engine.valid_reps == 0 and engine.pending_rep1 is not None)

    # Inter-rep rest (2000 to 2500 ms)
    for t in range(2000, 2500, 20):
        engine.update(0.0, 0.0, 0.70, t, grip_gate=True, is_stationary=True)

    # Rep 2 (2500 to 4500 ms)
    for t in range(2500, 4500, 20):
        phase = ((t - 2500) / 2000.0) * math.pi
        tilt = math.sin(phase) * 56.0
        gyro = abs(math.cos(phase) * 60.0) if t < 4300 else 4.0
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)

    if rep1_held and engine.valid_reps == 2 and engine.rejected_reps == 0:
        print("[PASS] Test 1: 2-Candidate rule correctly counted 0 on Rep 1 and back-credited both (Valid=2)")
        passed += 1
    else:
        print(f"[FAIL] Test 1: Rep1 held={rep1_held}, Valid={engine.valid_reps}, Rej={engine.rejected_reps}")

    # --- Test 2: Motion without Grip Ignored Silently (Rule 3) ---
    total += 1
    engine = RepetitionEngineSim()
    # User swings arm without gripping (grip_gate = False)
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        tilt = math.sin(phase) * 60.0
        gyro = abs(math.cos(phase) * 70.0)
        engine.update(tilt, gyro, 0.10, t, grip_gate=False)

    if engine.valid_reps == 0 and engine.rejected_reps == 0:
        print("[PASS] Test 2: Large motion without grip silently ignored (Zero phantom reps/rejects)")
        passed += 1
    else:
        print(f"[FAIL] Test 2: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, State={engine.state}")

    # --- Test 3: Grip Lost Mid-Rep Rejected Immediately (Rule 3) ---
    total += 1
    engine = RepetitionEngineSim()
    for t in range(0, 600, 20):
        phase = (t / 2000.0) * math.pi
        tilt = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0)
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)
    engine.update(35.0, 30.0, 0.15, 620, grip_gate=False)

    if engine.rejected_reps == 1 and engine.last_rejection_reason == "INSUFFICIENT_GRIP":
        print("[PASS] Test 3: Grip released mid-rep immediately rejected (Reason: INSUFFICIENT_GRIP)")
        passed += 1
    else:
        print(f"[FAIL] Test 3: Valid={engine.valid_reps}, Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 4: Valley Re-baselining (Torso tilt / seated position shift) ---
    total += 1
    engine = RepetitionEngineSim()
    # Baseline rests at 18 deg
    for t in range(0, 800, 20):
        engine.update(18.0, 0.5, 0.70, t, grip_gate=True, is_stationary=True)
    assert abs(engine.bottom_valley - 18.0) < 0.1, f"Bottom valley expected 18.0, got {engine.bottom_valley}"

    # Curl from 18 deg to 75 deg (prominence = 57 deg) and back to 18 deg
    for t in range(800, 2800, 20):
        phase = ((t - 800) / 2000.0) * math.pi
        tilt = 18.0 + math.sin(phase) * 57.0
        gyro = abs(math.cos(phase) * 60.0) if t < 2600 else 4.0
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)

    if engine.pending_rep1 is not None and abs(engine.pending_rep1[0] - 57.0) < 1.0:
        print("[PASS] Test 4: Valley dynamically re-baselined to 18° baseline shift with true 57° prominence")
        passed += 1
    else:
        print(f"[FAIL] Test 4: Valley={engine.bottom_valley}, Pending={engine.pending_rep1}")

    # --- Test 5: Incomplete Return / Low Descent Prominence Rejected (Rule 2) ---
    total += 1
    engine = RepetitionEngineSim()
    # Rep 1 to prime the engine
    for t in range(0, 2000, 20):
        phase = (t / 2000.0) * math.pi
        tilt = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0) if t < 1800 else 4.0
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)
    for t in range(2000, 2500, 20):
        engine.update(0.0, 0.0, 0.70, t, grip_gate=True, is_stationary=True)

    # Rep 2 curls to 55 deg, but only lowers to 30 deg and stalls there
    for t in range(2500, 3500, 20):
        phase = ((t - 2500) / 2000.0) * math.pi
        tilt = math.sin(phase) * 55.0
        gyro = abs(math.cos(phase) * 60.0)
        engine.update(tilt, gyro, 0.70, t, grip_gate=True)
    # Stalls at 30 deg until maxDurationMs
    for t in range(3500, 8000, 20):
        engine.update(30.0, 1.0, 0.70, t, grip_gate=True)

    if engine.rejected_reps >= 1 and engine.last_rejection_reason in ["INSUFFICIENT_ROM", "DURATION_TOO_LONG", "INCOMPLETE_RETURN"]:
        print(f"[PASS] Test 5: Incomplete return / insufficient descent prominence rejected ({engine.last_rejection_reason})")
        passed += 1
    else:
        print(f"[FAIL] Test 5: Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 6: Adaptive Thresholds Tuning after 3 Reps (Rule 5) ---
    total += 1
    engine = RepetitionEngineSim()
    # Perform 3 consistent clean reps:
    # Rep 1 & 2 confirm the set (Valid=2)
    # Rep 3 updates adaptive thresholds
    curr_time = 0
    for rep in range(3):
        # Lift
        for step in range(100):
            t = curr_time + step * 20
            phase = (step / 100.0) * math.pi
            tilt = math.sin(phase) * 60.0 # 60 deg ROM
            gyro = abs(math.cos(phase) * 60.0) if step < 90 else 4.0
            engine.update(tilt, gyro, 0.70, t, grip_gate=True)
        curr_time += 2000
        # Inter-rep rest
        for step in range(25):
            t = curr_time + step * 20
            engine.update(0.0, 0.0, 0.70, t, grip_gate=True, is_stationary=True)
        curr_time += 500

    # 3 reps performed with 60 deg peak ROM:
    # Adaptive minRom should be ~ 0.65 * 60 = 39 deg
    adaptive_passed = (engine.valid_reps == 3 and abs(engine.adaptive_min_rom - 39.0) < 3.0)
    if adaptive_passed:
        print(f"[PASS] Test 6: Adaptive thresholds tuned after 3 reps (Adaptive Min ROM = {engine.adaptive_min_rom:.1f}°)")
        passed += 1
    else:
        print(f"[FAIL] Test 6: Valid={engine.valid_reps}, Adaptive ROM={engine.adaptive_min_rom}")

    # --- Test 7: Phase 4 Grip-Load Coupling in True Loaded Lift (High rho -> High Confidence) ---
    total += 1
    engine = RepetitionEngineSim()
    # Confirm set with 2 reps that exhibit physiological FSR squeeze lagging gyro acceleration
    curr_time = 0
    for rep in range(2):
        for step in range(100):
            t_sec = step * 0.02
            t_ms = curr_time + step * 20
            phase = (step / 100.0) * math.pi
            tilt = math.sin(phase) * 60.0

            if t_sec < 1.0:
                gyro = 95.0 * math.sin(math.pi * t_sec / 1.0)
            else:
                gyro = 75.0 * math.sin(math.pi * (t_sec - 1.0) / 1.0) if step < 95 else 4.0

            t_lag = max(0.0, t_sec - 0.04)
            if t_lag < 1.0:
                a_lag = max(0.0, 95.0 * math.pi * math.cos(math.pi * t_lag / 1.0))
            else:
                a_lag = max(0.0, 75.0 * math.pi * math.cos(math.pi * (t_lag - 1.0) / 1.0))
            fsr_val = 2000.0 + a_lag * 3.0
            engine.update(tilt, gyro, 0.75, t_ms, grip_gate=True, fsr_raw=fsr_val)
        curr_time += 2000
        for step in range(25):
            t_ms = curr_time + step * 20
            engine.update(0.0, 0.0, 0.75, t_ms, grip_gate=True, is_stationary=True)
        curr_time += 500

    coupling_ok = (engine.valid_reps == 2 and engine.last_rep_coupling_rho > 0.35 and engine.last_rep_confidence_pct >= 70.0)
    if coupling_ok:
        print(f"[PASS] Test 7: Loaded lift verified (Coupling rho={engine.last_rep_coupling_rho:.2f}, Confidence={engine.last_rep_confidence_pct:.1f}%)")
        passed += 1
    else:
        print(f"[FAIL] Test 7: Valid={engine.valid_reps}, Rho={engine.last_rep_coupling_rho}, Conf={engine.last_rep_confidence_pct}")

    # --- Test 8: Phase 4 Unloaded / Mimed Movement Rejection (REJECT_UNLOADED) ---
    total += 1
    engine = RepetitionEngineSim()
    # Prime engine with valid Rep 1 (loaded)
    for step in range(100):
        t_sec = step * 0.02
        t_ms = step * 20
        phase = (step / 100.0) * math.pi
        tilt = math.sin(phase) * 60.0
        gyro = 95.0 * math.sin(math.pi * t_sec / 1.0) if t_sec < 1.0 else (75.0 * math.sin(math.pi * (t_sec - 1.0) / 1.0) if step < 95 else 4.0)
        t_lag = max(0.0, t_sec - 0.04)
        a_lag = max(0.0, 95.0 * math.pi * math.cos(math.pi * t_lag / 1.0)) if t_lag < 1.0 else 0.0
        fsr_val = 2000.0 + a_lag * 3.0
        engine.update(tilt, gyro, 0.75, t_ms, grip_gate=True, fsr_raw=fsr_val)
    for step in range(25):
        t_ms = 2000 + step * 20
        engine.update(0.0, 0.0, 0.75, t_ms, grip_gate=True, is_stationary=True)

    # Rep 2: Mimed movement without weight (low grip 0.35, decorrelated jitter in FSR, rho near 0)
    for step in range(100):
        t_ms = 2500 + step * 20
        phase = (step / 100.0) * math.pi
        tilt = math.sin(phase) * 60.0
        gyro = abs(math.cos(phase) * 60.0) if step < 90 else 4.0
        fsr_val = 1400.0 + (step % 3) * 10.0 # Uncorrelated jitter
        engine.update(tilt, gyro, 0.35, t_ms, grip_gate=True, fsr_raw=fsr_val)

    unloaded_passed = (engine.last_rejection_reason == "UNLOADED" and engine.rejected_reps == 1)
    if unloaded_passed:
        print(f"[PASS] Test 8: Mimed/unloaded curl correctly rejected (Reason: REJECT_UNLOADED, Rho={engine.last_rep_coupling_rho:.3f})")
        passed += 1
    else:
        print(f"[FAIL] Test 8: Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}, Rho={engine.last_rep_coupling_rho}")

    # --- Test 9: Phase 4 Momentum Swing Cheat Rejection (REJECT_MOMENTUM_SWING) ---
    total += 1
    engine = RepetitionEngineSim()
    # Prime engine with valid Rep 1
    for step in range(100):
        t_sec = step * 0.02
        t_ms = step * 20
        phase = (step / 100.0) * math.pi
        tilt = math.sin(phase) * 60.0
        gyro = 95.0 * math.sin(math.pi * t_sec / 1.0) if t_sec < 1.0 else (75.0 * math.sin(math.pi * (t_sec - 1.0) / 1.0) if step < 95 else 4.0)
        t_lag = max(0.0, t_sec - 0.04)
        a_lag = max(0.0, 95.0 * math.pi * math.cos(math.pi * t_lag / 1.0)) if t_lag < 1.0 else 0.0
        fsr_val = 2000.0 + a_lag * 3.0
        engine.update(tilt, gyro, 0.75, t_ms, grip_gate=True, fsr_raw=fsr_val)
    for step in range(25):
        t_ms = 2000 + step * 20
        engine.update(0.0, 0.0, 0.75, t_ms, grip_gate=True, is_stationary=True)

    # Rep 2: Ballistic momentum swing (explosive 135 dps gyro burst, concentric dur 380ms < 450ms)
    for step in range(3):
        t_ms = 2500 + step * 20
        engine.update(5.0 + step * 5.0, 35.0, 0.65, t_ms, grip_gate=True, fsr_raw=2100.0)
    for step in range(5):
        t_ms = 2560 + step * 20
        engine.update(25.0 + step * 7.0, 135.0, 0.65, t_ms, grip_gate=True, fsr_raw=2100.0)
    for step in range(5):
        t_ms = 2660 + step * 20
        engine.update(58.0, 8.0, 0.65, t_ms, grip_gate=True, fsr_raw=2100.0)
    for step in range(40):
        t_ms = 2760 + step * 20
        phase = min(math.pi / 2.0, (step / 30.0) * (math.pi / 2.0))
        tilt = math.cos(phase) * 58.0
        gyro = 25.0 if step < 26 else 4.0
        engine.update(tilt, gyro, 0.65, t_ms, grip_gate=True, fsr_raw=2100.0)

    swing_passed = (engine.last_rejection_reason == "MOMENTUM_SWING" and engine.rejected_reps == 1)
    if swing_passed:
        print(f"[PASS] Test 9: Momentum swing cheat rejected (Reason: REJECT_MOMENTUM_SWING)")
        passed += 1
    else:
        print(f"[FAIL] Test 9: Rej={engine.rejected_reps}, Reason={engine.last_rejection_reason}")

    # --- Test 10: Phase 4 Set-End Uncertain Rep Cadence Reconciliation ---
    total += 1
    engine = RepetitionEngineSim()
    # Confirm set with 2 clean reps (valid_reps = 2)
    curr_time = 0
    for rep in range(2):
        for step in range(100):
            t_sec = step * 0.02
            t_ms = curr_time + step * 20
            phase = (step / 100.0) * math.pi
            tilt = math.sin(phase) * 60.0
            gyro = 95.0 * math.sin(math.pi * t_sec / 1.0) if t_sec < 1.0 else (75.0 * math.sin(math.pi * (t_sec - 1.0) / 1.0) if step < 95 else 4.0)
            t_lag = max(0.0, t_sec - 0.04)
            a_lag = max(0.0, 95.0 * math.pi * math.cos(math.pi * t_lag / 1.0)) if t_lag < 1.0 else 0.0
            fsr_val = 2000.0 + a_lag * 3.0
            engine.update(tilt, gyro, 0.75, t_ms, grip_gate=True, fsr_raw=fsr_val)
        curr_time += 2000
        for step in range(25):
            t_ms = curr_time + step * 20
            engine.update(0.0, 0.0, 0.75, t_ms, grip_gate=True, is_stationary=True)
        curr_time += 500

    # Rep 3: Uncertain rep (moderate noise, confidence ~ 55%) -> Held in pool
    for step in range(110):
        t_ms = curr_time + step * 20
        phase = min(math.pi, (step / 100.0) * math.pi)
        tilt = math.sin(phase) * 48.0 # 48 deg ROM
        gyro = abs(math.cos(phase) * 50.0) if step < 95 else 4.0
        engine.update(tilt, gyro, 0.48, t_ms, grip_gate=True, fsr_raw=2000.0, signal_noise=0.35)
    curr_time += 2200

    rep3_pooled = (engine.valid_reps == 2 and len(engine.uncertain_reps) == 1)

    # Disarm / set end: User releases grip for > 1.5 seconds (80 samples)
    for step in range(80):
        t_ms = curr_time + step * 20
        engine.update(0.0, 0.0, 0.05, t_ms, grip_gate=False, is_stationary=True)

    reconciled_passed = rep3_pooled and (engine.valid_reps == 3 and engine.last_rep_was_uncertain)
    if reconciled_passed:
        print(f"[PASS] Test 10: Uncertain rep held and reconciled at set end (Final Valid Reps: 3)")
        passed += 1
    else:
        print(f"[FAIL] Test 10: Rep3 pooled={rep3_pooled}, Valid={engine.valid_reps}, LastUncertain={engine.last_rep_was_uncertain}")

    # --- Test 11: Phase 4 Fatigue & Near-Failure Tracking ---
    total += 1
    engine = RepetitionEngineSim()
    # Rep 1 establishes baseline (velocity ~60 dps, duration 1600ms, grip 0.80)
    for step in range(80):
        t_ms = step * 20
        phase = (step / 80.0) * math.pi
        tilt = math.sin(phase) * 60.0
        gyro = abs(math.cos(phase) * 60.0) if step < 72 else 4.0
        engine.update(tilt, gyro, 0.80, t_ms, grip_gate=True, fsr_raw=2100.0)
    for step in range(25):
        t_ms = 1600 + step * 20
        engine.update(0.0, 0.0, 0.80, t_ms, grip_gate=True, is_stationary=True)

    # Rep 2: Fatigue progression (peak velocity drops to 36 dps -> 40% velocity loss, duration slows to 2600ms)
    for step in range(130):
        t_ms = 2100 + step * 20
        phase = (step / 130.0) * math.pi
        tilt = math.sin(phase) * 58.0
        gyro = abs(math.cos(phase) * 36.0) if step < 118 else 4.0
        engine.update(tilt, gyro, 0.60, t_ms, grip_gate=True, fsr_raw=2100.0)

    fatigue_passed = engine.is_fatigued and (engine.fatigue_index_pct >= 30.0)
    if fatigue_passed:
        print(f"[PASS] Test 11: Fatigue & near-failure detected (Fatigue Index: {engine.fatigue_index_pct:.1f}%, IsFatigued={engine.is_fatigued})")
        passed += 1
    else:
        print(f"[FAIL] Test 11: IsFatigued={engine.is_fatigued}, Index={engine.fatigue_index_pct}")

    print("=" * 68)
    print(f"Results: {passed} / {total} Passed ({passed/total*100:.1f}%)")
    print("=" * 68)

    return passed == total

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
