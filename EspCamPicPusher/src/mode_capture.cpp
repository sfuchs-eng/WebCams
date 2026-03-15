#include <Arduino.h>
#include <ESPmDNS.h>
#include "globals.h"
#include "ConfigManager.h"
#include "SleepManager.h"
#include "WebConfigServer.h"

// ============================================================================
// Capture Mode — timer wake, capture one image then return to sleep
// ============================================================================

void runCaptureMode() {
    Serial.println("\n======================================");
    Serial.println("Executing scheduled capture");
    Serial.println("======================================");

    if (!cameraInitialized) {
        Serial.println("ERROR: Camera not initialized");
        sleepManager.incrementFailedCaptures();

        // Check if we should stay awake due to failures
        if (sleepManager.shouldStayAwake(3)) {
            Serial.println("Too many failures - staying awake in config mode");
            currentMode = MODE_CONFIG;

            // Start web server for troubleshooting
            webServer = new WebConfigServer(&configManager);
            webServer->setCameraReady(false);
            webServer->setCaptureCallback(captureAndPostImage);
            if (webServer->begin()) {
                MDNS.addService("http", "tcp", 80);
            }
            return;
        }

        enterSleepMode();
        return;
    }

    // Attempt capture and upload
    if (captureAndPostImage()) {
        Serial.println("✓ Capture successful!");
        sleepManager.resetFailedCaptures();
        blinkLED(2, 100);
    } else {
        Serial.println("✗ Capture failed");
        sleepManager.incrementFailedCaptures();
        blinkLED(5, 50);

        // Check if we should stay awake due to failures
        if (sleepManager.shouldStayAwake(3)) {
            Serial.println("Too many failures - staying awake in config mode");
            currentMode = MODE_CONFIG;

            // Start web server for troubleshooting
            webServer = new WebConfigServer(&configManager);
            webServer->setCameraReady(cameraInitialized);
            webServer->setCaptureCallback(captureAndPostImage);
            if (webServer->begin()) {
                MDNS.addService("http", "tcp", 80);
            }
            return;
        }
    }

    // Enter sleep mode for next capture
    enterSleepMode();
}
