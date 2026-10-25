#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Config.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include "SensorAcquisition.h"
#include "SignalProcessing.h"
#include "Calibration.h"
#include "RepEngine.h"
#include "FaultDetection.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

class GripNetworkManager {
public:
    GripNetworkManager();

    /**
     * Initialize SoftAP and optional Station Wi-Fi; register HTTP REST endpoints.
     */
    void init(PersonalCalibration* calib, RepetitionEngine* rep, FaultDetectionEngine* fault);

    /**
     * Non-blocking HTTP request processing loop.
     */
    void update(const RawSensorData& raw, const ProcessedSensorData& proc);

    /**
     * Network Status
     */
    bool isConnectedSTA() const { return _isStaConnected; }
    IPAddress getStationIP() const { return _staIP; }
    IPAddress getAccessPointIP() const { return _apIP; }

private:
    WebServer _server;
    PersonalCalibration* _calibRef;
    RepetitionEngine* _repRef;
    FaultDetectionEngine* _faultRef;

    bool _isStaConnected;
    IPAddress _staIP;
    IPAddress _apIP;

    // Cache latest sensor frames for instant API response without lock contention
    RawSensorData _cachedRaw;
    ProcessedSensorData _cachedProc;

    void registerRoutes();
    void sendCORSHeaders();
    void handleTelemetry();
    void handleGetCalibration();
    void handlePostCalibrate();
    void handleGetDiagnostics();
    void handlePostExercise();
    void handlePostResetReps();
    void handlePreflight();
    void handleNotFound();
};

#endif // NETWORK_MANAGER_H
