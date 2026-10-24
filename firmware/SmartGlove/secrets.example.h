#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

/**
 * ============================================================================
 * GRIPGUARD WI-FI CONFIGURATION TEMPLATE
 * ============================================================================
 * Copy this file to 'secrets.h' in the same folder and configure your local
 * network credentials if you wish to connect to an existing Wi-Fi router.
 * 
 * If 'secrets.h' is not present, GRIPGUARD will default to standalone SoftAP
 * mode ("GripGuard-AP" / "smartglove123"), requiring NO external router.
 */

// Station Mode (Connect to Home / Lab Wi-Fi)
#define WIFI_STA_SSID     "YOUR_WIFI_SSID"
#define WIFI_STA_PASSWORD "YOUR_WIFI_PASSWORD"

// Access Point Mode (Standalone Hotspot created by Glove ESP32)
#define WIFI_AP_SSID      "GripGuard-AP"
#define WIFI_AP_PASSWORD  "smartglove123"

#endif // SECRETS_EXAMPLE_H
