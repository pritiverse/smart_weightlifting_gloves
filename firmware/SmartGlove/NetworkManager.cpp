#include "NetworkManager.h"

GripNetworkManager::GripNetworkManager()
    : _server(80),
      _calibRef(nullptr),
      _repRef(nullptr),
      _faultRef(nullptr),
      _isStaConnected(false) {
}

void GripNetworkManager::sendCORSHeaders() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void GripNetworkManager::handlePreflight() {
    sendCORSHeaders();
    _server.send(204);
}

void GripNetworkManager::handleNotFound() {
    sendCORSHeaders();
    _server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
}

void GripNetworkManager::init(PersonalCalibration* calib, RepetitionEngine* rep, FaultDetectionEngine* fault) {
    _calibRef = calib;
    _repRef = rep;
    _faultRef = fault;

    // 1. Dual Wi-Fi Configuration: AP + Station Mode
    WiFi.mode(WIFI_AP_STA);

    // Launch standalone Access Point ("GripGuard-AP" / "smartglove123")
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    _apIP = WiFi.softAPIP();

    // Attempt Station connection if valid SSID provided
    #ifdef WIFI_STA_SSID
    if (strcmp(WIFI_STA_SSID, "YOUR_WIFI_SSID") != 0 && strlen(WIFI_STA_SSID) > 0) {
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);
        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 3500) {
            delay(100); // Brief bounded attempt
        }
        if (WiFi.status() == WL_CONNECTED) {
            _isStaConnected = true;
            _staIP = WiFi.localIP();
        }
    }
    #endif

    // 2. Register REST API Routes
    registerRoutes();

    // 3. Start HTTP Server
    _server.begin();
}

void GripNetworkManager::registerRoutes() {
    // CORS Preflights
    _server.on("/api/telemetry", HTTP_OPTIONS, [this]() { handlePreflight(); });
    _server.on("/api/calibration", HTTP_OPTIONS, [this]() { handlePreflight(); });
    _server.on("/api/calibrate", HTTP_OPTIONS, [this]() { handlePreflight(); });
    _server.on("/api/diagnostics", HTTP_OPTIONS, [this]() { handlePreflight(); });
    _server.on("/api/exercise", HTTP_OPTIONS, [this]() { handlePreflight(); });
    _server.on("/api/reset_reps", HTTP_OPTIONS, [this]() { handlePreflight(); });

    // Functional Endpoints
    _server.on("/api/telemetry", HTTP_GET, [this]() { handleTelemetry(); });
    _server.on("/api/calibration", HTTP_GET, [this]() { handleGetCalibration(); });
    _server.on("/api/calibrate", HTTP_POST, [this]() { handlePostCalibrate(); });
    _server.on("/api/diagnostics", HTTP_GET, [this]() { handleGetDiagnostics(); });
    _server.on("/api/exercise", HTTP_POST, [this]() { handlePostExercise(); });
    _server.on("/api/reset_reps", HTTP_POST, [this]() { handlePostResetReps(); });

    _server.onNotFound([this]() { handleNotFound(); });
}

void GripNetworkManager::handleTelemetry() {
    sendCORSHeaders();

    const DiagnosticReport& diag = _faultRef->getReport();
    const RepetitionStats& repStats = _repRef->getStats();
    const ExerciseProfile& exProf = _repRef->getActiveProfile();

    String json = "{";
    json += "\"schema_version\":\"1.0\",";
    json += "\"device_id\":\"GRIPGUARD_ESP32\",";
    json += "\"sequence_id\":" + String(_cachedRaw.sequenceId) + ",";
    json += "\"uptime_ms\":" + String(_cachedRaw.timestampMs) + ",";
    json += "\"sample_dt_us\":" + String(_cachedRaw.dtUs) + ",";

    // Orientation
    json += "\"orientation\":{";
    json += "\"pitch_deg\":" + String(_cachedProc.pitchDeg, 1) + ",";
    json += "\"roll_deg\":" + String(_cachedProc.rollDeg, 1) + ",";
    json += "\"is_stationary\":" + String(_cachedProc.isStationary ? "true" : "false") + ",";
    json += "\"stationary_ms\":" + String(_cachedProc.stationaryDurationMs);
    json += "},";

    // IMU Physical Readings
    json += "\"imu_physical\":{";
    json += "\"accel_x\":" + String(_cachedProc.accelX_mps2, 2) + ",";
    json += "\"accel_y\":" + String(_cachedProc.accelY_mps2, 2) + ",";
    json += "\"accel_z\":" + String(_cachedProc.accelZ_mps2, 2) + ",";
    json += "\"gyro_x\":" + String(_cachedProc.gyroX_dps, 1) + ",";
    json += "\"gyro_y\":" + String(_cachedProc.gyroY_dps, 1) + ",";
    json += "\"gyro_z\":" + String(_cachedProc.gyroZ_dps, 1) + ",";
    json += "\"temp_c\":" + String(_cachedRaw.temperatureC, 1);
    json += "},";

    // Grip Metrics
    json += "\"grip\":{";
    json += "\"raw_adc\":" + String(_cachedRaw.fsrRawADC) + ",";
    json += "\"filtered_adc\":" + String((int)_cachedProc.filteredFsrADC) + ",";
    json += "\"voltage\":" + String(_cachedProc.filteredFsrVoltage, 2) + ",";
    json += "\"normalized_pct\":" + String((int)(_cachedProc.normalizedGrip * 100.0f));
    json += "},";

    // Repetition Engine
    json += "\"repetition\":{";
    json += "\"state\":\"" + String(_repRef->getStateName()) + "\",";
    json += "\"exercise\":\"" + String(exProf.name) + "\",";
    json += "\"valid_count\":" + String(repStats.totalValidReps) + ",";
    json += "\"rejected_count\":" + String(repStats.totalRejectedReps) + ",";
    json += "\"acceptance_ratio_pct\":" + String(repStats.acceptanceRatioPct, 1) + ",";
    json += "\"current_excursion_deg\":" + String(repStats.currentExcursionDeg, 1) + ",";
    json += "\"last_rep_duration_ms\":" + String(repStats.lastRepDurationMs) + ",";
    json += "\"last_rep_rom_deg\":" + String(repStats.lastRepPeakRomDeg, 1) + ",";
    json += "\"last_rep_grip_pct\":" + String((int)(repStats.lastRepAvgGripRatio * 100.0f)) + ",";
    json += "\"last_rejection_reason\":\"" + String(_repRef->getRejectionReasonName(repStats.lastRejectionReason)) + "\"";
    json += "},";

    // Health Engine
    json += "\"health\":{";
    json += "\"overall\":\"" + String(_faultRef->getHealthStateName(diag.overallHealth)) + "\",";
    json += "\"mpu\":\"" + String(_faultRef->getHealthStateName(diag.mpuHealth)) + "\",";
    json += "\"fsr\":\"" + String(_faultRef->getHealthStateName(diag.fsrHealth)) + "\",";
    json += "\"confidence_pct\":" + String((int)diag.assessmentConfidencePct) + ",";
    json += "\"active_fault\":\"" + String(_faultRef->getFaultTypeName(diag.primaryFault)) + "\",";
    json += "\"message\":\"" + String(diag.faultMessage) + "\",";
    json += "\"rep_counting_gated\":" + String(diag.repCountingPermitted ? "false" : "true");
    json += "},";

    // Calibration Summary
    json += "\"calibration\":{";
    json += "\"is_calibrated\":" + String(_calibRef->isCalibrated() ? "true" : "false") + ",";
    json += "\"step\":\"" + String(_calibRef->getStepName()) + "\",";
    json += "\"progress_pct\":" + String(_calibRef->getStepProgressPct());
    json += "}";

    json += "}";
    _server.send(200, "application/json", json);
}

void GripNetworkManager::handleGetCalibration() {
    sendCORSHeaders();
    const CalibrationProfile& prof = _calibRef->getProfile();

    String json = "{";
    json += "\"is_valid\":" + String(prof.isValid ? "true" : "false") + ",";
    json += "\"schema_version\":" + String(prof.schemaVersion) + ",";
    json += "\"neutral_pitch_deg\":" + String(prof.neutralPitchDeg, 1) + ",";
    json += "\"neutral_roll_deg\":" + String(prof.neutralRollDeg, 1) + ",";
    json += "\"posture_stability_std\":" + String(prof.postureStabilityStd, 2) + ",";
    json += "\"fsr_relaxed_mean_adc\":" + String(prof.fsrRelaxedMeanADC) + ",";
    json += "\"fsr_relaxed_std\":" + String(prof.fsrRelaxedStdDev, 1) + ",";
    json += "\"fsr_firm_mean_adc\":" + String(prof.fsrFirmMeanADC) + ",";
    json += "\"fsr_firm_std\":" + String(prof.fsrFirmStdDev, 1) + ",";
    json += "\"grip_separation_ratio\":" + String(prof.gripSeparationRatio, 2) + ",";
    json += "\"grip_eligibility_adc\":" + String(prof.gripEligibilityADC) + ",";
    json += "\"posture_tolerance_deg\":" + String(prof.postureToleranceDeg, 1) + ",";
    json += "\"crc32\":" + String(prof.crc32);
    json += "}";

    _server.send(200, "application/json", json);
}

void GripNetworkManager::handlePostCalibrate() {
    sendCORSHeaders();
    String action = _server.hasArg("action") ? _server.arg("action") : "";

    if (action == "start") {
        _calibRef->startWizard();
        _server.send(200, "application/json", "{\"status\":\"started\",\"step\":\"PREFLIGHT_CHECK\"}");
    } else if (action == "cancel") {
        _calibRef->abortWizard("Canceled via API");
        _server.send(200, "application/json", "{\"status\":\"canceled\"}");
    } else if (action == "reset") {
        _calibRef->resetToDefaults();
        _server.send(200, "application/json", "{\"status\":\"reset_to_defaults\"}");
    } else {
        _server.send(400, "application/json", "{\"error\":\"Invalid action. Expected 'start', 'cancel', or 'reset'\"}");
    }
}

void GripNetworkManager::handleGetDiagnostics() {
    sendCORSHeaders();
    const DiagnosticReport& diag = _faultRef->getReport();

    String json = "{";
    json += "\"overall_health\":\"" + String(_faultRef->getHealthStateName(diag.overallHealth)) + "\",";
    json += "\"mpu_health\":\"" + String(_faultRef->getHealthStateName(diag.mpuHealth)) + "\",";
    json += "\"fsr_health\":\"" + String(_faultRef->getHealthStateName(diag.fsrHealth)) + "\",";
    json += "\"active_fault\":\"" + String(_faultRef->getFaultTypeName(diag.primaryFault)) + "\",";
    json += "\"fault_message\":\"" + String(diag.faultMessage) + "\",";
    json += "\"confidence_pct\":" + String((int)diag.assessmentConfidencePct) + ",";
    json += "\"rep_counting_permitted\":" + String(diag.repCountingPermitted ? "true" : "false") + ",";
    json += "\"gyro_bias_mag_dps\":" + String(diag.gyroBiasMagnitude_dps, 2) + ",";
    json += "\"fsr_baseline_delta_adc\":" + String(diag.fsrBaselineDeltaADC, 1) + ",";
    json += "\"consecutive_i2c_errors\":" + String(diag.consecutiveI2cErrors) + ",";
    json += "\"recovery_elapsed_ms\":" + String(diag.recoveryStabilityElapsedMs);
    json += "}";

    _server.send(200, "application/json", json);
}

void GripNetworkManager::handlePostExercise() {
    sendCORSHeaders();
    String profile = _server.hasArg("profile") ? _server.arg("profile") : "";

    if (profile == "bicep_curl" || profile == "0") {
        _repRef->setExerciseProfile(EXERCISE_BICEP_CURL);
        _server.send(200, "application/json", "{\"status\":\"ok\",\"exercise\":\"Bicep Curl\"}");
    } else if (profile == "wrist_extension" || profile == "1") {
        _repRef->setExerciseProfile(EXERCISE_WRIST_EXTENSION);
        _server.send(200, "application/json", "{\"status\":\"ok\",\"exercise\":\"Wrist Extension\"}");
    } else if (profile == "dumbbell_press" || profile == "2") {
        _repRef->setExerciseProfile(EXERCISE_DUMBBELL_PRESS);
        _server.send(200, "application/json", "{\"status\":\"ok\",\"exercise\":\"Dumbbell Press\"}");
    } else {
        _server.send(400, "application/json", "{\"error\":\"Unknown profile. Expected 'bicep_curl', 'wrist_extension', or 'dumbbell_press'\"}");
    }
}

void GripNetworkManager::handlePostResetReps() {
    sendCORSHeaders();
    _repRef->resetCounters();
    _server.send(200, "application/json", "{\"status\":\"repetition_counters_reset\"}");
}

void GripNetworkManager::update(const RawSensorData& raw, const ProcessedSensorData& proc) {
    _cachedRaw = raw;
    _cachedProc = proc;

    // Process pending HTTP requests without blocking
    _server.handleClient();
}
