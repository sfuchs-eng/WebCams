#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "globals.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"
#include "WebConfigServer.h"

// ============================================================================
// Config Mode — web server active, handles manual and scheduled captures
// ============================================================================

void runConfigMode() {
    // Check for a capture queued by the async web handler.
    // Placed before the 1-second throttle so it triggers within ~100 ms.
    // The actual blocking work (camera warm-up + outbound HTTPS POST) must run
    // here on the main loop — never inside an AsyncWebServer callback — so the
    // ESP32 TCP stack is never blocked while keeping the browser connection open.
    if (webServer && webServer->isCaptureRequested()) {
        webServer->ackCaptureRequest();
        Serial.println("\n=== Manual capture requested via web UI ===");
        bool success = captureAndPostImage();
        webServer->setCaptureResult(success);
        if (success) {
            Serial.println("✓ Manual capture successful!");
            sleepManager.resetFailedCaptures();
            blinkLED(2, 100);
        } else {
            Serial.println("✗ Manual capture failed");
            sleepManager.incrementFailedCaptures();
            blinkLED(5, 50);
        }
        webServer->resetActivityTimer();
    }

    // Drive the non-blocking WiFi test state machine.
    // POST /config/test queues the test (PENDING); here we initiate WiFi.begin()
    // and poll WiFi.status() on each loop pass — no delay(), no busy wait.
    {
        static unsigned long wifiTestStartMs = 0;

        if (webServer && webServer->isWifiTestPending()) {
            Serial.printf("[WiFiTest] Starting test for SSID: %s\n",
                webServer->getWifiTestSsid().c_str());
            WiFi.begin(webServer->getWifiTestSsid().c_str(),
                       webServer->getWifiTestPassword().c_str());
            webServer->ackWifiTest();  // PENDING → IN_PROGRESS
            wifiTestStartMs = millis();
        }

        if (webServer && webServer->isWifiTestInProgress()) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFiTest] Connected!");
                Serial.printf("[WiFiTest] IP: %s, RSSI: %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
                webServer->setWifiTestResult(true, WiFi.localIP().toString(), WiFi.RSSI());
            } else if (millis() - wifiTestStartMs > 15000) {
                Serial.println("[WiFiTest] Timeout. Reconnecting to configured WiFi.");
                const char* origSsid = configManager.getWifiSsid();
                if (origSsid && strlen(origSsid) > 0) {
                    Serial.printf("[WiFiTest] Reconnecting to: %s\n", origSsid);
                    WiFi.begin(origSsid, configManager.getWifiPassword());
                }
                webServer->setWifiTestResult(false);
            }
        }
    }

    static unsigned long lastCheck = 0;
    static unsigned long lastCaptureCheck = 0;
    static int lastCaptureMinute = -1; // Track last capture to prevent duplicates
    static unsigned long lastApCheck = 0;
    static bool staWasConnected = false;

    // Check every second
    if (millis() - lastCheck < 1000) {
        return;
    }
    lastCheck = millis();

    // Check AP+STA status every 10 seconds
    if (isApMode && (millis() - lastApCheck >= 10000)) {
        lastApCheck = millis();

        bool staConnected = isWiFiConnected();
        if (staConnected && !staWasConnected) {
            // STA just connected
            Serial.println("\\n=== STA Connection Established ===");
            Serial.printf("IP address: %s\\n", WiFi.localIP().toString().c_str());
            Serial.printf("Signal strength: %d dBm\\n", WiFi.RSSI());
            Serial.printf("Also accessible at: http://%s/\\n", WiFi.localIP().toString().c_str());
            Serial.println("==============================\\n");
            staWasConnected = true;
        } else if (!staConnected && staWasConnected) {
            // STA disconnected
            Serial.println("\\n=== STA Connection Lost ===");
            staWasConnected = false;
        }
    }

    // Check if it's time to capture (even while in config mode)
    // Check every 10 seconds to be responsive to schedule changes
    if (millis() - lastCaptureCheck >= 10000) {
        lastCaptureCheck = millis();

        struct tm timeinfo;
        if (ScheduleManager::getCurrentTime(&timeinfo)) {
            int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min; // Minutes since midnight
            int numTimes = configManager.getNumCaptureTimes();
            if (numTimes > 0) {
                ScheduleTime schedule[MAX_CAPTURE_TIMES];
                for (int i = 0; i < numTimes; i++) {
                    schedule[i].hour = configManager.getCaptureHour(i);
                    schedule[i].minute = configManager.getCaptureMinute(i);
                }

                // Only capture if it's time AND we haven't captured in this minute yet
                if (scheduleManager.isTimeToCapture(&timeinfo, schedule, numTimes) &&
                    currentMinute != lastCaptureMinute) {
                    Serial.println("\n=== Scheduled capture while in CONFIG mode ===");

                    if (captureAndPostImage()) {
                        Serial.println("✓ Capture successful!");
                        sleepManager.resetFailedCaptures();
                        blinkLED(2, 100);
                    } else {
                        Serial.println("✗ Capture failed");
                        sleepManager.incrementFailedCaptures();
                        blinkLED(5, 50);
                    }

                    // Remember we captured in this minute
                    lastCaptureMinute = currentMinute;

                    // Reset web server activity timer to give user more time after capture
                    if (webServer) {
                        webServer->resetActivityTimer();
                    }
                }
            }
        }
    }

    // Check if timeout expired
    if (webServer && webServer->isTimeoutExpired()) {
        Serial.println("\n=== Web server timeout expired ===");

        // If in AP mode, restart to retry
        if (isApMode) {
            Serial.println("AP mode timeout - WiFi not configured, restarting...");
            delay(2000);
            ESP.restart();
            return;
        }

        // Check if we should enter sleep mode or wait mode
        if (shouldEnterSleepMode()) {
            enterSleepMode();
        } else {
            // Next capture is soon, enter wait mode
            Serial.println("Next capture is imminent, entering WAIT mode");
            currentMode = MODE_WAIT;

            // Clean up web server
            if (webServer) {
                webServer->stop();
                delete webServer;
                webServer = nullptr;
            }
        }
    }
}
