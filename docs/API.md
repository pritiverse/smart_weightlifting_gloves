# GRIPGUARD — REST API & Network Protocol Specification

This document details the network architecture, asynchronous HTTP server, versioned JSON schemas, and REST endpoints exposed by the ESP32 firmware for dashboard integration.

---

## 1. Network Topology & Dual-Mode Wi-Fi

The ESP32 firmware operates in intelligent **Access Point + Station (AP+STA)** mode with pure AP fallback:
1. **Standalone Access Point (SoftAP):**
   - **SSID:** `GripGuard-AP`
   - **Password:** `smartglove123`
   - **Default Gateway IP:** `http://192.168.4.1`
   - **Advantage:** Functions anywhere (gym floor, outdoor track) with zero dependence on a local router.
2. **Station Mode (Optional):**
   - Configured via `secrets.h` (from `secrets.example.h`).
   - Connects to local lab/home Wi-Fi network and obtains a local DHCP IP (e.g., `http://192.168.1.150`). If unconfigured or association fails, the firmware automatically locks into pure `WIFI_AP` mode to eliminate background channel hopping.
3. **Decoupled Asynchronous Execution & Low-Latency RF:**
   - `netManager.update()` is called non-blockingly. Sensor sampling remains strictly timed at 50 Hz via `micros()`, guaranteeing that slow network requests or browser disconnections **never** starve sensor acquisition.
   - Wi-Fi modem sleep is disabled (`WiFi.setSleep(false)`), eliminating 100–300 ms packet latency jitter and socket exhaustion.
   - JSON string outputs pre-allocate memory with `reserve()` to completely prevent FreeRTOS heap fragmentation.

---

## 2. Cross-Origin Resource Sharing (CORS)

All API endpoints emit standard CORS headers to permit development against Vite dev servers running on `localhost:5173` or any local origin:
```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
```
All preflight `OPTIONS` requests receive an immediate `204 No Content` response.

---

## 3. API Endpoints

### 3.1 `GET /api/telemetry`
Returns the complete, synchronized real-time state of the glove.

#### Response Payload (`application/json`):
```json
{
  "schema_version": "1.0",
  "device_id": "GRIPGUARD_ESP32",
  "sequence_id": 1420,
  "uptime_ms": 28400,
  "sample_dt_us": 20012,
  "orientation": {
    "pitch_deg": 35.4,
    "roll_deg": -1.2,
    "is_stationary": false,
    "stationary_ms": 0
  },
  "imu_physical": {
    "accel_x": 0.12,
    "accel_y": 9.81,
    "accel_z": 0.45,
    "gyro_x": -0.2,
    "gyro_y": 14.5,
    "gyro_z": 0.1,
    "temp_c": 28.4
  },
  "grip": {
    "raw_adc": 1850,
    "filtered_adc": 1845,
    "voltage": 1.49,
    "normalized_pct": 68
  },
  "repetition": {
    "state": "CONCENTRIC",
    "exercise": "Bicep Curl",
    "valid_count": 5,
    "rejected_count": 1,
    "acceptance_ratio_pct": 83.3,
    "current_excursion_deg": 35.4,
    "last_rep_duration_ms": 1950,
    "last_rep_rom_deg": 54.2,
    "last_rep_grip_pct": 74,
    "last_rejection_reason": "NONE (VALID)"
  },
  "health": {
    "overall": "HEALTHY",
    "mpu": "HEALTHY",
    "fsr": "HEALTHY",
    "confidence_pct": 100,
    "active_fault": "NONE",
    "message": "All sensors nominal",
    "rep_counting_gated": false
  },
  "calibration": {
    "is_calibrated": true,
    "step": "IDLE",
    "progress_pct": 0
  }
}
```

---

### 3.2 `GET /api/calibration`
Retrieves the persistent personal calibration profile stored in NVS.

#### Response Payload (`application/json`):
```json
{
  "is_valid": true,
  "schema_version": 1,
  "neutral_pitch_deg": 0.4,
  "neutral_roll_deg": -0.8,
  "posture_stability_std": 1.25,
  "fsr_relaxed_mean_adc": 340,
  "fsr_relaxed_std": 24.5,
  "fsr_firm_mean_adc": 2350,
  "fsr_firm_std": 52.1,
  "grip_separation_ratio": 26.24,
  "grip_eligibility_adc": 1144,
  "posture_tolerance_deg": 15.0,
  "crc32": 284195102
}
```

---

### 3.3 `POST /api/calibrate`
Triggers actions within the 5-step personal calibration wizard.

#### Parameters (Query String or Form URL-Encoded):
- `action`:
  - `start`: Initiates the 5-step guided calibration wizard.
  - `cancel`: Aborts active wizard and returns to idle.
  - `reset`: Reverts calibration to factory fallback defaults.

#### Example Request:
```bash
curl -X POST "http://192.168.4.1/api/calibrate?action=start"
```

#### Response:
```json
{
  "status": "started",
  "step": "PREFLIGHT_CHECK"
}
```

---

### 3.4 `GET /api/diagnostics`
Retrieves deep sensor reliability metrics and fault diagnostics.

#### Response:
```json
{
  "overall_health": "HEALTHY",
  "mpu_health": "HEALTHY",
  "fsr_health": "HEALTHY",
  "active_fault": "NONE",
  "fault_message": "All sensors nominal",
  "confidence_pct": 100,
  "rep_counting_permitted": true,
  "gyro_bias_mag_dps": 0.45,
  "fsr_baseline_delta_adc": 12.4,
  "consecutive_i2c_errors": 0,
  "recovery_elapsed_ms": 0
}
```

---

### 3.5 `POST /api/exercise`
Switches the active repetition exercise profile.

#### Parameters:
- `profile`:
  - `bicep_curl` (or `0`)
  - `wrist_extension` (or `1`)
  - `dumbbell_press` (or `2`)

#### Example Request:
```bash
curl -X POST "http://192.168.4.1/api/exercise?profile=bicep_curl"
```

#### Response:
```json
{
  "status": "ok",
  "exercise": "Bicep Curl"
}
```

---

### 3.6 `POST /api/reset_reps`
Resets valid reps, rejected attempts, and session duration to zero.

#### Response:
```json
{
  "status": "repetition_counters_reset"
}
```
