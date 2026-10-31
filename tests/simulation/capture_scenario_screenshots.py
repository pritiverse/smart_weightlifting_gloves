#!/usr/bin/env python3
"""
GRIPGUARD — Automated Scenario Screenshot Capture Utility
=========================================================
Runs through each test scenario and window modal, automatically rendering
and capturing pixel-perfect 1920x1080 full-HD screenshots of the web
dashboard into `report_figures/simulated_scenarios/` without needing an ESP32.

Prerequisites:
  - Mock server running at http://localhost:8000 (starts automatically if not running)
  - Vite dev server running at http://localhost:5173
  - Microsoft Edge (or Google Chrome) installed
"""

import os
import subprocess
import sys
import time
import urllib.request
import json

EDGE_PATHS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"
]

OUTPUT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "report_figures", "simulated_scenarios"))
os.makedirs(OUTPUT_DIR, exist_ok=True)

SCENARIOS_TO_CAPTURE = [
    {
        "id": "01_normal_valid_bicep_curl",
        "scenario": "healthy_rep",
        "url": "http://localhost:5173/?sim=1",
        "title": "REP-01: Valid Repetition (55° ROM, 72% Grip, Green Healthy Status)",
        "delay_s": 2.5
    },
    {
        "id": "02_cheated_rep_low_rom",
        "scenario": "insufficient_rom",
        "url": "http://localhost:5173/?sim=1",
        "title": "REP-02: Cheated Repetition (26° Shallow ROM, Form Mismatch Alert)",
        "delay_s": 2.5
    },
    {
        "id": "03_free_arm_insufficient_grip",
        "scenario": "insufficient_grip",
        "url": "http://localhost:5173/?sim=1",
        "title": "REP-04: Free-Arm Swing (14% Loose Grip, Insufficient Grip Alert)",
        "delay_s": 2.5
    },
    {
        "id": "04_ballistic_spike_too_fast",
        "scenario": "ballistic_spike",
        "url": "http://localhost:5173/?sim=1",
        "title": "REP-03: Ballistic Spike (<700ms Jerking, Duration Rejection)",
        "delay_s": 2.0
    },
    {
        "id": "05_sensor_dropout_i2c_blackout",
        "scenario": "flt_dropout",
        "url": "http://localhost:5173/?sim=1",
        "title": "FLT-01: MPU6050 Bus Dropout (SENSOR_UNAVAILABLE, Confidence 0%, Gated Reps)",
        "delay_s": 2.0
    },
    {
        "id": "06_recovery_stability_gate",
        "scenario": "flt_recovering",
        "url": "http://localhost:5173/?sim=1",
        "title": "FLT-02: Sensor Recovery Gate (RECOVERING, 1500ms Hold Active)",
        "delay_s": 2.0
    },
    {
        "id": "07_gyro_bias_drift_degraded",
        "scenario": "flt_gyro_drift",
        "url": "http://localhost:5173/?sim=1",
        "title": "FLT-03: Gyroscope Drift (DEGRADED, 14.8°/s Bias, Confidence 70%)",
        "delay_s": 2.0
    },
    {
        "id": "08_fsr_baseline_drift",
        "scenario": "flt_fsr_drift",
        "url": "http://localhost:5173/?sim=1",
        "title": "FLT-04: FSR Baseline Shift (CALIBRATION_REQUIRED Alert)",
        "delay_s": 2.0
    },
    {
        "id": "09_window_guided_calibration_modal",
        "scenario": "calibrating_step1",
        "url": "http://localhost:5173/?sim=1&calib=1",
        "title": "Modal Window: 4-Step Personal Biomechanical Calibration Flow",
        "delay_s": 2.5
    },
    {
        "id": "10_simulation_control_console",
        "scenario": "healthy_rep",
        "url": "http://localhost:8000/",
        "title": "Hardware Simulation Console & Interactive Scenario Controller",
        "delay_s": 1.5
    }
]

def find_browser():
    for p in EDGE_PATHS:
        if os.path.exists(p):
            return p
    return None

def check_service(url, name):
    try:
        res = urllib.request.urlopen(url, timeout=2)
        return True
    except Exception:
        return False

def switch_scenario(name):
    try:
        url = f"http://localhost:8000/scenario?name={urllib.parse.quote(name)}"
        res = urllib.request.urlopen(url, timeout=2)
        return True
    except Exception as e:
        print(f"  [!] Failed to set scenario '{name}': {e}")
        return False

def capture_screenshot(browser_bin, url, output_png_path, delay_s=2.0):
    virtual_time_ms = int(delay_s * 1000)
    cmd = [
        browser_bin,
        "--headless",
        "--disable-gpu",
        f"--virtual-time-budget={virtual_time_ms}",
        "--window-size=1920,1080",
        f"--screenshot={output_png_path}",
        url
    ]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
    return os.path.exists(output_png_path) and os.path.getsize(output_png_path) > 0

def main():
    print("=" * 70)
    print(" GRIPGUARD Automated Scenario & Window Screenshot Utility")
    print("=" * 70)

    browser = find_browser()
    if not browser:
        print("[ERROR] Neither Microsoft Edge nor Google Chrome found on system.")
        sys.exit(1)
    print(f"[+] Using browser: {browser}")

    # Check services
    if not check_service("http://localhost:8000/api/telemetry", "Mock ESP32 Server"):
        print("[!] Mock ESP32 server not detected on port 8000. Launching in background...")
        script_path = os.path.join(os.path.dirname(__file__), "mock_esp32_server.py")
        subprocess.Popen([sys.executable, script_path])
        time.sleep(2)

    if not check_service("http://localhost:5173", "Vite Dashboard"):
        print("[ERROR] Vite Dashboard is not running on http://localhost:5173.")
        print("        Please run 'npm run dev' inside the 'dashboard/' directory first.")
        sys.exit(1)

    print(f"[+] Output folder: {OUTPUT_DIR}\n")

    successful = 0
    for idx, item in enumerate(SCENARIOS_TO_CAPTURE, 1):
        sc_id = item["id"]
        sc_name = item["scenario"]
        target_url = item["url"]
        title = item["title"]
        delay_s = item["delay_s"]
        out_png = os.path.join(OUTPUT_DIR, f"{sc_id}.png")

        print(f"[{idx}/{len(SCENARIOS_TO_CAPTURE)}] {title}")
        print(f"      -> Switching scenario to '{sc_name}'...")
        switch_scenario(sc_name)

        print(f"      -> Capturing 1920x1080 screenshot...")
        ok = capture_screenshot(browser, target_url, out_png, delay_s=delay_s)

        if ok:
            sz_kb = os.path.getsize(out_png) // 1024
            print(f"      [OK] Saved to: {os.path.basename(out_png)} ({sz_kb} KB)\n")
            successful += 1
        else:
            print(f"      [FAIL] Failed to save screenshot: {out_png}\n")

    print("=" * 70)
    print(f" Completed: {successful}/{len(SCENARIOS_TO_CAPTURE)} scenario screenshots captured successfully!")
    print(f" All images saved to: {OUTPUT_DIR}")
    print("=" * 70)

if __name__ == "__main__":
    main()
