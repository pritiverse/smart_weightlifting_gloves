#!/usr/bin/env python3
"""
GRIPGUARD — Mock ESP32 Simulation Server & Scenario Generator
============================================================
Simulates the GRIPGUARD Smart Glove hardware, Wi-Fi REST API, and
sensor physics without needing a physical ESP32 or sensors connected.

Endpoints implemented (matches ESP32 NetworkManager.cpp):
  - GET  /api/telemetry     -> Dynamic real-time packets (50 Hz synthesized telemetry)
  - GET  /api/calibration   -> Personal calibration profile & statistical metrics
  - GET  /api/diagnostics   -> Reliability engine status, fault codes & drift rates
  - POST /api/calibrate     -> Calibration control (?action=start|cancel|reset)
  - POST /api/exercise      -> Exercise profile (?profile=bicep_curl|...)
  - POST /api/reset_reps    -> Reset valid/rejected repetition counters
  - GET  /scenario          -> Switch active scenario (?name=healthy_rep|flt_dropout|...)
  - GET  /                  -> Interactive Scenario Control Dashboard
"""

import http.server
import json
import math
import sys
import threading
import time
import urllib.parse

PORT = 8000

# ── Available Simulation Scenarios ───────────────────────────────────────────
SCENARIOS = {
    "healthy_rep": {
        "title": "Normal Valid Repetition (REP-01)",
        "description": "55° pitch excursion, 2.0s duration, 72% firm grip, repetition increments cleanly.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "All sensors operational",
        "rep_gated": False,
        "target_rom": 55.0,
        "period_s": 2.2,
        "grip_pct": 72.0,
        "expected_result": "VALID_REP",
        "rejection_reason": "NONE"
    },
    "insufficient_rom": {
        "title": "Cheated Repetition / Low ROM (REP-02)",
        "description": "Shallow 26° curl (<40° min ROM), 2.0s, firm grip. Flagged as REJECT_INSUFFICIENT_ROM.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "All sensors operational",
        "rep_gated": False,
        "target_rom": 26.0,
        "period_s": 2.0,
        "grip_pct": 68.0,
        "expected_result": "REJECTED_REP",
        "rejection_reason": "REJECT_INSUFFICIENT_ROM"
    },
    "insufficient_grip": {
        "title": "Free-Arm Swing / Insufficient Grip (REP-04)",
        "description": "55° full arm excursion, 2.0s, but loose grip (14% < 35% min). Flagged as REJECT_INSUFFICIENT_GRIP.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "All sensors operational",
        "rep_gated": False,
        "target_rom": 55.0,
        "period_s": 2.2,
        "grip_pct": 14.0,
        "expected_result": "REJECTED_REP",
        "rejection_reason": "REJECT_INSUFFICIENT_GRIP"
    },
    "ballistic_spike": {
        "title": "Ballistic Spike / Jerking Movement (REP-03)",
        "description": "Rapid 48° snap executed in only 420ms (<700ms min). Flagged as REJECT_DURATION_TOO_SHORT.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "All sensors operational",
        "rep_gated": False,
        "target_rom": 48.0,
        "period_s": 0.5,
        "grip_pct": 65.0,
        "expected_result": "REJECTED_REP",
        "rejection_reason": "REJECT_DURATION_TOO_SHORT"
    },
    "flt_dropout": {
        "title": "Sensor Dropout / I2C Bus Blackout (FLT-01)",
        "description": "I2C SDA line disconnected. Status: SENSOR_UNAVAILABLE, confidence 0%, rep counting gated.",
        "health": "SENSOR_UNAVAILABLE",
        "mpu": "DISCONNECTED / BUS_ERROR",
        "fsr": "OK",
        "confidence": 0,
        "active_fault": "MPU_I2C_DROPOUT",
        "fault_message": "MPU6050 bus blackout detected. Rep counting paused.",
        "rep_gated": True,
        "target_rom": 0.0,
        "period_s": 3.0,
        "grip_pct": 0.0,
        "expected_result": "IDLE",
        "rejection_reason": "REJECT_SENSOR_UNRELIABLE"
    },
    "flt_recovering": {
        "title": "Dropout Recovery Hold Gate (FLT-02)",
        "description": "Sensor reconnected. Status: RECOVERING, requiring 1500ms continuous stability gate before HEALTHY.",
        "health": "RECOVERING",
        "mpu": "STABILIZING",
        "fsr": "OK",
        "confidence": 45,
        "active_fault": "RECOVERY_HOLD_ACTIVE",
        "fault_message": "Awaiting 1500ms sensor stability gate (elapsed: 850ms).",
        "rep_gated": True,
        "target_rom": 4.0,
        "period_s": 3.0,
        "grip_pct": 20.0,
        "expected_result": "IDLE",
        "rejection_reason": "REJECT_SENSOR_UNRELIABLE"
    },
    "flt_gyro_drift": {
        "title": "Gyroscope Zero-Rate Bias Drift (FLT-03)",
        "description": "Stationary gyro bias exceeds 12.0°/s threshold. Status: DEGRADED, confidence 70%, warnings flagged.",
        "health": "DEGRADED",
        "mpu": "HIGH_DRIFT",
        "fsr": "OK",
        "confidence": 70,
        "active_fault": "GYRO_BIAS_EXCESSIVE",
        "fault_message": "Gyro bias rate 14.8°/s exceeds threshold. Reps counted with caution.",
        "rep_gated": False,
        "target_rom": 52.0,
        "period_s": 2.4,
        "grip_pct": 70.0,
        "expected_result": "VALID_REP",
        "rejection_reason": "NONE"
    },
    "flt_fsr_drift": {
        "title": "FSR Resting Baseline Drift (FLT-04)",
        "description": "Resting unweighted FSR ADC shifted by +480 counts. Status: CALIBRATION_REQUIRED.",
        "health": "CALIBRATION_REQUIRED",
        "mpu": "OK",
        "fsr": "BASELINE_SHIFT",
        "confidence": 55,
        "active_fault": "FSR_REST_DRIFT",
        "fault_message": "Resting grip baseline shifted. Recalibration strongly recommended.",
        "rep_gated": False,
        "target_rom": 50.0,
        "period_s": 2.2,
        "grip_pct": 40.0,
        "expected_result": "VALID_REP",
        "rejection_reason": "NONE"
    },
    "calib_profile_saved": {
        "title": "5-Step Calibration Profile Saved in NVS",
        "description": "5-step calibration complete, separation ratio 28.50 verified, saved to NVS memory.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "All sensors operational",
        "rep_gated": False,
        "target_rom": 0.0,
        "period_s": 4.0,
        "grip_pct": 0.0,
        "expected_result": "IDLE",
        "rejection_reason": "NONE",
        "calib_step": "IDLE",
        "calib_pct": 100
    },
    "calibrating_step1": {
        "title": "Guided Calibration: Step 1 (Rest Arm)",
        "description": "Step 1 of 4: Arm resting on bench for zero baseline calculation.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "Calibrating baseline",
        "rep_gated": True,
        "target_rom": 0.0,
        "period_s": 4.0,
        "grip_pct": 1.0,
        "expected_result": "IDLE",
        "rejection_reason": "NONE",
        "calib_step": "PREFLIGHT_CHECK",
        "calib_pct": 25
    },
    "calibrating_step4": {
        "title": "Guided Calibration: Step 4 (Firm Grip)",
        "description": "Step 4 of 4: Recording maximal voluntary isometric contraction grip.",
        "health": "HEALTHY",
        "mpu": "OK",
        "fsr": "OK",
        "confidence": 100,
        "active_fault": "NONE",
        "fault_message": "Sampling firm grip baseline",
        "rep_gated": True,
        "target_rom": 0.0,
        "period_s": 4.0,
        "grip_pct": 85.0,
        "expected_result": "IDLE",
        "rejection_reason": "NONE",
        "calib_step": "FIRM_GRIP",
        "calib_pct": 90
    }
}

class SimulationState:
    def __init__(self):
        self.lock = threading.Lock()
        self.scenario_key = "healthy_rep"
        self.sequence_id = 0
        self.start_time = time.time()
        self.valid_count = 8
        self.rejected_count = 2
        self.active_exercise = "Bicep Curl"
        self.last_rep_cycle = 0
        self.rep_state = "IDLE"
        self.last_rep_duration_ms = 2100
        self.last_rep_rom_deg = 55.4
        self.last_rep_grip_pct = 71.8
        self.last_rejection_reason = "NONE"

    def get_scenario(self):
        with self.lock:
            return SCENARIOS.get(self.scenario_key, SCENARIOS["healthy_rep"])

    def set_scenario(self, key):
        with self.lock:
            if key in SCENARIOS:
                self.scenario_key = key
                return True
            return False

    def reset_reps(self):
        with self.lock:
            self.valid_count = 0
            self.rejected_count = 0

SIM = SimulationState()

class MockESP32Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Quiet standard HTTP logs to avoid terminal spam
        pass

    def send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_cors_headers()
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        # 1. API: Telemetry
        if path == "/api/telemetry":
            self.handle_telemetry()
            return

        # 2. API: Calibration Profile
        if path == "/api/calibration":
            self.handle_calibration()
            return

        # 3. API: Diagnostics Report
        if path == "/api/diagnostics":
            self.handle_diagnostics()
            return

        # 4. Scenario Switcher Endpoint
        if path == "/scenario":
            sc_name = query.get("name", [""])[0]
            if sc_name and SIM.set_scenario(sc_name):
                self.send_response(200)
                self.send_cors_headers()
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                resp = {"status": "ok", "active_scenario": sc_name, "info": SCENARIOS[sc_name]}
                self.wfile.write(json.dumps(resp, indent=2).encode("utf-8"))
            else:
                self.send_response(400)
                self.send_cors_headers()
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                resp = {"status": "error", "message": f"Unknown scenario '{sc_name}'", "available": list(SCENARIOS.keys())}
                self.wfile.write(json.dumps(resp).encode("utf-8"))
            return

        # 5. Interactive Control Web Dashboard (Root)
        if path == "/" or path == "/index.html":
            self.handle_dashboard_ui()
            return

        # Not Found
        self.send_response(404)
        self.send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"error": "Endpoint not found"}')

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()

        if path == "/api/reset_reps":
            SIM.reset_reps()
            self.wfile.write(b'{"status": "ok", "action": "reset_reps"}')
            return

        if path == "/api/calibrate":
            action = query.get("action", ["none"])[0]
            self.wfile.write(json.dumps({"status": "ok", "action": action}).encode("utf-8"))
            return

        if path == "/api/exercise":
            profile = query.get("profile", ["bicep_curl"])[0]
            SIM.active_exercise = profile.replace("_", " ").title()
            self.wfile.write(json.dumps({"status": "ok", "profile": profile}).encode("utf-8"))
            return

        self.wfile.write(b'{"status": "received"}')

    def handle_telemetry(self):
        now = time.time()
        elapsed = now - SIM.start_time
        sc = SIM.get_scenario()

        with SIM.lock:
            SIM.sequence_id += 1
            seq = SIM.sequence_id

        # Synthesize biomechanical movement trajectory
        target_rom = sc["target_rom"]
        period = sc["period_s"]

        # Cycle phase between 0.0 and 1.0
        phase = (elapsed % period) / period
        current_cycle = int(elapsed // period)

        # Sinusoidal motion: excursion rises and returns smoothly
        # Half cycle concentric (0 to 0.5), half cycle eccentric (0.5 to 1.0)
        if target_rom > 0 and sc["health"] != "SENSOR_UNAVAILABLE":
            pitch_excursion = target_rom * (0.5 - 0.5 * math.cos(2.0 * math.pi * phase))
            gyro_mag = abs(target_rom * (math.pi / period) * math.sin(2.0 * math.pi * phase))
            is_stat = phase < 0.1 or phase > 0.95
            roll_deg = 2.5 * math.sin(phase * 4.0)

            # Determine rep state from movement phase
            if phase < 0.1:
                rep_state = "READY"
            elif phase < 0.45:
                rep_state = "CONCENTRIC"
            elif phase < 0.55:
                rep_state = "TRANSITION"
            elif phase < 0.90:
                rep_state = "ECCENTRIC"
            else:
                rep_state = sc["expected_result"]

            # If cycle finished, record rep outcome
            if current_cycle != SIM.last_rep_cycle:
                SIM.last_rep_cycle = current_cycle
                if sc["expected_result"] == "VALID_REP":
                    SIM.valid_count += 1
                    SIM.last_rejection_reason = "NONE"
                elif sc["expected_result"] == "REJECTED_REP":
                    SIM.rejected_count += 1
                    SIM.last_rejection_reason = sc["rejection_reason"]
                SIM.last_rep_duration_ms = int(period * 1000)
                SIM.last_rep_rom_deg = round(target_rom, 1)
                SIM.last_rep_grip_pct = round(sc["grip_pct"], 1)
        else:
            pitch_excursion = 0.0
            gyro_mag = 0.4
            is_stat = True
            roll_deg = 0.0
            rep_state = "IDLE"

        # Grip values
        norm_grip = sc["grip_pct"]
        raw_adc = int(150 + (norm_grip / 100.0) * (3400 - 150))
        voltage = round((raw_adc / 4095.0) * 3.3, 2)

        total_reps = SIM.valid_count + SIM.rejected_count
        ratio = round((SIM.valid_count / total_reps * 100.0), 1) if total_reps > 0 else 0.0

        # Build TelemetryPacket
        telemetry = {
            "schema_version": "1.0",
            "device_id": "GRIPGUARD_SIMULATOR",
            "sequence_id": seq,
            "uptime_ms": int(elapsed * 1000),
            "sample_dt_us": 20000,
            "orientation": {
                "pitch_deg": round(pitch_excursion, 1),
                "roll_deg": round(roll_deg, 1),
                "is_stationary": is_stat,
                "stationary_ms": 1200 if is_stat else 0
            },
            "imu_physical": {
                "accel_x": 0.02,
                "accel_y": round(math.sin(pitch_excursion * math.pi / 180.0), 3),
                "accel_z": round(math.cos(pitch_excursion * math.pi / 180.0), 3),
                "gyro_x": 0.2,
                "gyro_y": round(gyro_mag, 1),
                "gyro_z": -0.1,
                "temp_c": 28.4
            },
            "grip": {
                "raw_adc": raw_adc,
                "filtered_adc": raw_adc,
                "voltage": voltage,
                "normalized_pct": round(norm_grip, 1),
                "schmitt_armed": norm_grip >= 35.0
            },
            "repetition": {
                "state": rep_state,
                "exercise": SIM.active_exercise,
                "valid_count": SIM.valid_count,
                "rejected_count": SIM.rejected_count,
                "acceptance_ratio_pct": ratio,
                "current_excursion_deg": round(pitch_excursion, 1),
                "last_rep_duration_ms": SIM.last_rep_duration_ms,
                "last_rep_rom_deg": SIM.last_rep_rom_deg,
                "last_rep_grip_pct": SIM.last_rep_grip_pct,
                "last_rejection_reason": SIM.last_rejection_reason,
                "coupling_rho": round(sc.get("coupling_rho", 0.412 if sc["expected_result"] == "VALID_REP" else 0.082), 3),
                "confidence_pct": sc.get("rep_confidence", 88 if sc["expected_result"] == "VALID_REP" else 42),
                "is_fatigued": sc.get("is_fatigued", False),
                "fatigue_index_pct": sc.get("fatigue_pct", 18),
                "adaptive_min_rom_deg": round(sc.get("adaptive_rom", 38.4), 1),
                "pending_candidates": 0,
                "is_set_armed": norm_grip >= 35.0
            },
            "health": {
                "overall": sc["health"],
                "mpu": sc["mpu"],
                "fsr": sc["fsr"],
                "confidence_pct": sc["confidence"],
                "active_fault": sc["active_fault"],
                "message": sc["fault_message"],
                "rep_counting_gated": sc["rep_gated"]
            },
            "calibration": {
                "is_calibrated": sc.get("calib_pct", 100) == 100,
                "step": sc.get("calib_step", "COMPLETE"),
                "progress_pct": sc.get("calib_pct", 100)
            }
        }

        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(telemetry).encode("utf-8"))

    def handle_calibration(self):
        calib_profile = {
            "is_valid": True,
            "schema_version": 1,
            "neutral_pitch_deg": 1.8,
            "neutral_roll_deg": -0.9,
            "posture_stability_std": 0.038,
            "fsr_relaxed_mean_adc": 115,
            "fsr_relaxed_std": 6.8,
            "fsr_firm_mean_adc": 3280,
            "fsr_firm_std": 42.1,
            "grip_separation_ratio": 28.5,
            "grip_eligibility_adc": 1222,
            "posture_tolerance_deg": 14.0,
            "crc32": 314159265
        }
        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(calib_profile).encode("utf-8"))

    def handle_diagnostics(self):
        sc = SIM.get_scenario()
        diag = {
            "overall_health": sc["health"],
            "mpu_health": sc["mpu"],
            "fsr_health": sc["fsr"],
            "active_fault": sc["active_fault"],
            "fault_message": sc["fault_message"],
            "confidence_pct": sc["confidence"],
            "rep_counting_permitted": not sc["rep_gated"],
            "gyro_bias_mag_dps": 14.8 if sc["active_fault"] == "GYRO_BIAS_EXCESSIVE" else 0.72,
            "fsr_baseline_delta_adc": 480 if sc["active_fault"] == "FSR_REST_DRIFT" else 12,
            "consecutive_i2c_errors": 42 if sc["health"] == "SENSOR_UNAVAILABLE" else 0,
            "recovery_elapsed_ms": 850 if sc["health"] == "RECOVERING" else 0
        }
        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(diag).encode("utf-8"))

    def handle_dashboard_ui(self):
        # Interactive HTML Control Console
        buttons_html = ""
        current_sc = SIM.scenario_key
        for key, sc in SCENARIOS.items():
            is_active = (key == current_sc)
            border = "border: 2px solid #00f2fe; background: #132238;" if is_active else "border: 1px solid #1f2e46; background: #0d1626;"
            badge_color = "#00f2fe" if is_active else "#64748b"
            status_text = "ACTIVE NOW" if is_active else "CLICK TO ACTIVATE"
            buttons_html += f"""
            <div style="padding: 16px; border-radius: 12px; margin-bottom: 12px; {border} transition: all 0.2s ease;">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
                    <span style="font-size: 16px; font-weight: 700; color: #f8fafc;">{sc['title']}</span>
                    <span style="font-size: 11px; font-weight: 800; padding: 4px 10px; border-radius: 999px; background: rgba(0, 242, 254, 0.15); color: {badge_color};">{status_text}</span>
                </div>
                <p style="font-size: 13px; color: #94a3b8; margin: 0 0 12px 0; line-height: 1.4;">{sc['description']}</p>
                <div style="display: flex; gap: 8px;">
                    <button onclick="setScenario('{key}')" style="background: linear-gradient(135deg, #00f2fe, #4facfe); color: #040814; font-weight: 700; border: none; padding: 8px 18px; border-radius: 8px; cursor: pointer;">
                        Simulate Scenario
                    </button>
                </div>
            </div>
            """

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>GRIPGUARD — Hardware Simulation Console</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: #060b14;
            color: #f8fafc;
            margin: 0;
            padding: 30px;
        }}
        .container {{
            max-width: 840px;
            margin: 0 auto;
        }}
        .header {{
            border-bottom: 1px solid #1e293b;
            padding-bottom: 20px;
            margin-bottom: 24px;
        }}
        .title {{
            font-size: 26px;
            font-weight: 800;
            letter-spacing: -0.5px;
            margin: 0 0 6px 0;
            background: linear-gradient(135deg, #00f2fe, #4facfe);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }}
        .instructions {{
            background: #0f172a;
            border-left: 4px solid #00f2fe;
            padding: 14px 18px;
            border-radius: 6px;
            margin-bottom: 24px;
            font-size: 14px;
            line-height: 1.5;
            color: #cbd5e1;
        }}
        .btn-reset {{
            background: #e11d48;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 8px;
            font-weight: 600;
            cursor: pointer;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 class="title">GRIPGUARD Hardware Simulator</h1>
            <p style="color: #64748b; margin: 0; font-size: 14px;">Simulates ESP32 Wi-Fi Telemetry, FSM Repetition Mechanics &amp; Fault Injection</p>
        </div>

        <div class="instructions">
            <strong>How to connect the Vite Dashboard:</strong><br>
            1. Keep this server running on <code>http://localhost:8000</code>.<br>
            2. Open your GRIPGUARD Dashboard at <code>http://localhost:5173</code>.<br>
            3. In the top right header, click <strong>Custom IP</strong> and enter <code>http://localhost:8000</code>.<br>
            4. Click any scenario below to immediately watch the dashboard animate and respond in real time!
        </div>

        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px;">
            <h2 style="font-size: 18px; font-weight: 700; margin: 0;">Test Scenarios (Verification Matrix)</h2>
            <button class="btn-reset" onclick="resetReps()">Reset Rep Counters</button>
        </div>

        <div>
            {buttons_html}
        </div>
    </div>

    <script>
        async function setScenario(name) {{
            await fetch('/scenario?name=' + encodeURIComponent(name));
            window.location.reload();
        }}
        async function resetReps() {{
            await fetch('/api/reset_reps', {{ method: 'POST' }});
            alert('Repetition counters reset to 0!');
        }}
    </script>
</body>
</html>"""
        self.send_response(200)
        self.send_cors_headers()
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode("utf-8"))

def run():
    server = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), MockESP32Handler)
    print(f"================================================================")
    print(f" GRIPGUARD Simulation Server running at http://localhost:{PORT}")
    print(f" Control Dashboard: http://localhost:{PORT}/")
    print(f" Telemetry API:     http://localhost:{PORT}/api/telemetry")
    print(f" Press Ctrl+C to terminate.")
    print(f"================================================================")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nSimulation Server stopped.")
        server.server_close()

if __name__ == "__main__":
    run()
