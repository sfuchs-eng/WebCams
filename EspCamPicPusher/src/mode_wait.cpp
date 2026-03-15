#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"

// ============================================================================
// Wait Mode — next capture is imminent, stay awake and poll the schedule
// ============================================================================

void runWaitMode() {
    static unsigned long lastCheck = 0;
    static int lastCaptureMinute = -1;  // Prevent duplicate captures within the same minute

    // Check every 10 seconds
    if (millis() - lastCheck < 10000) {
        return;
    }
    lastCheck = millis();

    struct tm timeinfo;
    if (!ScheduleManager::getCurrentTime(&timeinfo)) {
        Serial.println("Failed to get current time in wait mode");
        return;
    }

    int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    // Build schedule array from config
    int numTimes = configManager.getNumCaptureTimes();
    ScheduleTime schedule[MAX_CAPTURE_TIMES];
    for (int i = 0; i < numTimes; i++) {
        schedule[i].hour = configManager.getCaptureHour(i);
        schedule[i].minute = configManager.getCaptureMinute(i);
    }

    // Check if it's time to capture and we haven't already captured this minute
    if (scheduleManager.isTimeToCapture(&timeinfo, schedule, numTimes) &&
        currentMinute != lastCaptureMinute) {
        lastCaptureMinute = currentMinute;  // Set before upload to guard against re-entry
        Serial.println("\n=== Time to capture! ===");

        if (captureAndPostImage()) {
            Serial.println("✓ Capture successful!");
            sleepManager.resetFailedCaptures();
            blinkLED(2, 100);
        } else {
            Serial.println("✗ Capture failed");
            sleepManager.incrementFailedCaptures();
            blinkLED(5, 50);
        }

        // After capture in wait mode, enter sleep or stay in wait
        if (shouldEnterSleepMode()) {
            enterSleepMode();
        } else {
            Serial.println("Next capture is soon, staying in wait mode");
        }
    } else {
        Serial.printf("Waiting... Current time: %s\n", ScheduleManager::formatTime(&timeinfo).c_str());
    }
}
