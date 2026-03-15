#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "globals.h"
#include "ConfigManager.h"

// ============================================================================
// WiFi Setup Functions
// ============================================================================

String generateApSsid() {
    // Get last 4 characters of MAC address for unique SSID
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String suffix = mac.substring(mac.length() - 4);
    suffix.toUpperCase();
    return "ESP32-CAM-" + suffix;
}

// Returns the configured hostname, or auto-generates one from the last 4 MAC
// hex digits (e.g. "espcam-a1b2"). Must be called after WiFi is initialised so
// the MAC address is available.
String resolveHostname() {
    String configured = String(configManager.getHostname());
    if (configured.length() > 0) {
        return configured;
    }
    // Auto-generate: espcam-<last4MAC-lowercase>
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String suffix = mac.substring(mac.length() - 4);
    suffix.toLowerCase();
    return "espcam-" + suffix;
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void setupWiFiAPSTA() {
    Serial.println("\n--- WiFi AP+STA Setup ---");

    // Get configured credentials
    const char* ssid = configManager.getWifiSsid();
    const char* password = configManager.getWifiPassword();

    // Generate AP SSID
    String apSsid = generateApSsid();

    // Set hostname for STA interface (must be before WiFi.softAP / WiFi.begin)
    String hostname = resolveHostname();

    // Start AP+STA mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(hostname.c_str());

    // Configure and start Access Point (no password)
    bool apStarted = WiFi.softAP(apSsid.c_str());
    if (apStarted) {
        Serial.println("Access Point started");
        Serial.printf("AP SSID: %s\n", apSsid.c_str());
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

        // Start mDNS on the AP interface so clients can reach the config UI
        // even without internet/router DNS.
        if (!MDNS.begin(hostname.c_str())) {
            Serial.println("WARNING: mDNS start failed");
        } else {
            Serial.printf("mDNS: http://%s.local/\n", hostname.c_str());
        }
    } else {
        Serial.println("ERROR: Failed to start Access Point");
    }

    // Attempt to connect to configured WiFi
    Serial.printf("Attempting STA connection to: %s\n", ssid);
    WiFi.begin(ssid, password);

    Serial.println("\n=== AP+STA Mode Active ===");
    Serial.printf("Connect to: %s\n", apSsid.c_str());
    Serial.printf("Configuration URL: http://192.168.4.1\n");
    Serial.printf("mDNS URL: http://%s.local/\n", hostname.c_str());
    Serial.println("===========================\n");
}

bool setupWiFiSTA() {
    Serial.println("\n--- WiFi STA Setup ---");

    const char* ssid = configManager.getWifiSsid();
    const char* password = configManager.getWifiPassword();

    Serial.printf("Connecting to: %s\n", ssid);

    // setHostname() must be called BEFORE WiFi.begin() so that the DHCP
    // DISCOVER/REQUEST packets carry the desired hostname option.
    String hostname = resolveHostname();
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
        Serial.printf("Hostname: %s\n", hostname.c_str());
        if (!MDNS.begin(hostname.c_str())) {
            Serial.println("WARNING: mDNS start failed");
        } else {
            Serial.printf("mDNS: http://%s.local/\n", hostname.c_str());
        }
        return true;
    } else {
        Serial.println("\nWiFi connection failed!");
        return false;
    }
}
