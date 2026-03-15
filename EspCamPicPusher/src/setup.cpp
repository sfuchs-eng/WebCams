#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "config.h"
#include "globals.h"
#include "CameraMutex.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"
#include "WebConfigServer.h"
#include "OTAManager.h"
#include "RemoteLogger.h"

// ============================================================================
// Serial and Time Setup
// ============================================================================

void setupSerial() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== EspCamPicPusher ===");
    Serial.println("Starting...");
}

void setupTime() {
    Serial.println("\n--- Time Setup ---");
    Serial.printf("NTP Server: %s\n", NTP_SERVER);

    long gmtOffset = configManager.getGmtOffsetSec();
    int dstOffset = configManager.getDaylightOffsetSec();

    configTime(gmtOffset, dstOffset, NTP_SERVER, NTP_SERVER2);

    Serial.print("Waiting for NTP time sync");
    int attempts = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && attempts < 20) {
        Serial.print(".");
        delay(500);
        attempts++;
    }

    if (attempts < 20) {
        Serial.println("\nTime synchronized!");
        Serial.println(ScheduleManager::formatTime(&timeinfo));
        lastNtpUpdate = millis();
    } else {
        Serial.println("\nFailed to obtain time!");
    }
}

// ============================================================================
// Setup Helpers (file-private)
// ============================================================================

// Minimal boot: download and flash a pending OTA firmware image.
// No camera, web server, or AsyncTCP task — maximum free heap for OTA.
static void setupOtaMode() {
    Serial.println("\n=== PENDING OTA UPDATE DETECTED ===");
    Serial.println("=== Entering OTA MODE (minimal boot) ===");
    currentMode = MODE_OTA;

    // Only need WiFi for OTA - skip camera, web server, NTP, remote logger
    bool wifiConnected = setupWiFiSTA();
    if (!wifiConnected) {
        Serial.println("[OTA] WiFi failed - cannot perform OTA update");
        Serial.println("[OTA] Clearing pending update and rebooting normally");
        otaManager.clearPendingUpdate();
        delay(1000);
        ESP.restart();
        return;
    }

    // Initialize OTA manager (partition detection)
    otaManager.begin();

    Serial.println("=== OTA Mode Ready ===\n");
}

// Config mode boot: used for WAKE_POWER_ON and any unknown wake reason.
// Starts the web server and keeps the ESP awake for user interaction.
static void setupConfigMode() {
    bool wifiConnected = setupWiFiSTA();
    if (!wifiConnected) {
        Serial.println("WiFi connection failed, starting AP+STA mode");
        setupWiFiAPSTA();
        isApMode = true;
    } else {
        isApMode = false;
    }

    setupCamera();

    // Only setup time if WiFi is connected (NTP requires internet)
    if (wifiConnected || isWiFiConnected()) {
        setupTime();

        // Initialize remote logger (requires WiFi)
        RemoteLogger::begin(
            configManager.getServerUrl(),
            configManager.getAuthToken(),
            WiFi.macAddress()
        );
    } else {
        Serial.println("Skipping NTP setup (no WiFi connection)");
        // Disable remote logging if no WiFi
        RemoteLogger::setEnabled(false);
    }

    // Initialize OTA manager
    if (wifiConnected || isWiFiConnected()) {
        otaManager.begin();

        // Check if this is first boot after OTA
        if (otaManager.isFirstBootAfterOta()) {
            Serial.println("\n[OTA] First boot after update detected");
            otaValidationPending = true;
            pendingOtaFirmwareFile = otaManager.loadConfirmFirmwareFile();
            Serial.printf("[OTA] Firmware to confirm: %s\n", pendingOtaFirmwareFile.c_str());
            // Validation happens after first successful capture
        }
    }

    // Start web server
    webServer = new WebConfigServer(&configManager);
    webServer->setCameraReady(cameraInitialized);
    webServer->setCaptureCallback(captureAndPostImage);
    webServer->setApMode(isApMode);
    if (!webServer->begin()) {
        Serial.println("ERROR: Failed to start web server");
    } else {
        MDNS.addService("http", "tcp", 80);
    }

    Serial.println("\n=== EspCamPicPusher Ready - Config Mode ===");
    if (isApMode) {
        Serial.printf("AP Mode: Connect to %s\n", generateApSsid().c_str());
        Serial.println("Configuration URL: http://192.168.4.1/");
        if (isWiFiConnected()) {
            Serial.printf("Also available at: http://%s/\n", WiFi.localIP().toString().c_str());
        }
    } else {
        Serial.printf("Configuration URL: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    Serial.printf("Web timeout: %d minutes\n", configManager.getWebTimeoutMin());
    Serial.println("===========================================\n");
}

// Capture mode boot: timer wake — capture one image then return to sleep.
static void setupCaptureMode() {
    // Get WiFi retry count
    uint32_t retryCount = sleepManager.getWifiRetryCount();
    Serial.printf("WiFi retry attempt: %u/5\n", retryCount);

    bool wifiConnected = setupWiFiSTA();
    if (!wifiConnected) {
        sleepManager.incrementFailedCaptures();

        if (retryCount < 5) {
            // Retry: increment counter and sleep for 5 minutes
            sleepManager.incrementWifiRetryCount();
            Serial.printf("\nWiFi retry %u/5 failed, sleeping for 5 minutes...\n", retryCount + 1);
            sleepManager.enterDeepSleep(300);  // 5 minutes = 300 seconds
            // Code never reaches here
        } else {
            // Max retries reached, skip this capture and sleep until next scheduled time
            Serial.println("\nWiFi unavailable after 5 retries, sleeping until next scheduled capture");
            sleepManager.resetWifiRetryCount();
            enterSleepMode();
            // Code never reaches here
        }
    }

    // WiFi connected successfully - reset retry counter
    sleepManager.resetWifiRetryCount();

    // Initialize remote logger
    RemoteLogger::begin(
        configManager.getServerUrl(),
        configManager.getAuthToken(),
        WiFi.macAddress()
    );

    setupCamera();

    // Check if NTP sync needed (>24 hours since last)
    time_t lastSync = sleepManager.getLastNtpSync();
    time_t now = time(nullptr);
    if (lastSync == 0 || (now - lastSync) > 86400) {
        Serial.println("NTP sync required...");
        setupTime();
        sleepManager.setLastNtpSync(time(nullptr));
    } else {
        Serial.println("Using RTC time (NTP sync not required)");
        // Deep sleep wipes the POSIX TZ env var from RAM even though the RTC
        // hardware counter survives. Calling configTime() with an empty NTP
        // server string restores the TZ offset without issuing any NTP request,
        // so getLocalTime() returns the correct local time for X-Timestamp.
        configTime(configManager.getGmtOffsetSec(), configManager.getDaylightOffsetSec(), "");
    }

    // Initialize OTA manager
    otaManager.begin();

    // Check if this is first boot after OTA - force validation capture
    if (otaManager.isFirstBootAfterOta()) {
        Serial.println("\n[OTA] First boot after update detected");
        Serial.println("[OTA] Forcing validation capture even if no timeslot due");
        pendingOtaFirmwareFile = otaManager.loadConfirmFirmwareFile();
        Serial.printf("[OTA] Firmware to confirm: %s\n", pendingOtaFirmwareFile.c_str());
        RemoteLogger::info("OTA", "First boot after OTA - validating");
        otaValidationPending = true;
        // Validation will occur in runCaptureMode() after successful upload
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    setupSerial();
    blinkLED(3, 200); // Visual indication of startup

    // Initialize sleep manager
    sleepManager.begin();

    // Initialize camera mutex for thread-safe access
    CameraMutex::init();

    // Initialize configuration manager
    if (!configManager.begin()) {
        Serial.println("ERROR: Failed to initialize configuration");
        blinkLED(10, 100);
        delay(5000);
        ESP.restart();
    }

    // Check for pending OTA update BEFORE initializing anything else.
    // This ensures a clean, minimal boot: no camera, no web server,
    // no AsyncTCP task — maximum free heap for OTA download + flash.
    if (otaManager.hasPendingUpdate()) {
        setupOtaMode();
        return;
    }

    WakeReason wakeReason = sleepManager.getWakeReason();
    Serial.printf("\n=== Wake Reason: %s ===\n", sleepManager.getWakeReasonString().c_str());

    switch (wakeReason) {
        case WAKE_POWER_ON:
            Serial.println("=== Entering CONFIGURATION MODE ===");
            currentMode = MODE_CONFIG;
            setupConfigMode();
            break;

        case WAKE_TIMER:
            Serial.println("=== Entering CAPTURE MODE ===");
            currentMode = MODE_CAPTURE;
            setupCaptureMode();
            break;

        default:
            Serial.println("=== Unknown wake reason - entering CONFIG MODE ===");
            currentMode = MODE_CONFIG;
            setupConfigMode();
            break;
    }
}
