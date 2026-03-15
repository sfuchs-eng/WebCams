#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"

// ============================================================================
// Sleep Helpers (shared by all run modes)
// ============================================================================

void enterSleepMode() {
    struct tm timeinfo;
    if (!ScheduleManager::getCurrentTime(&timeinfo)) {
        Serial.println("ERROR: Cannot get time for sleep calculation");
        Serial.println("Restarting...");
        delay(5000);
        ESP.restart();
        return;
    }

    // Build schedule array from config
    int numTimes = configManager.getNumCaptureTimes();
    if (numTimes == 0) {
        Serial.println("ERROR: No capture times configured");
        Serial.println("Restarting...");
        delay(5000);
        ESP.restart();
        return;
    }

    ScheduleTime schedule[MAX_CAPTURE_TIMES];
    for (int i = 0; i < numTimes; i++) {
        schedule[i].hour = configManager.getCaptureHour(i);
        schedule[i].minute = configManager.getCaptureMinute(i);
    }

    int sleepMargin = configManager.getSleepMarginSec();

    // Calculate sleep duration
    long sleepSeconds = scheduleManager.getSecondsUntilWake(&timeinfo, schedule, numTimes, sleepMargin);

    if (sleepSeconds <= 0) {
        Serial.println("ERROR: Invalid sleep duration, restarting...");
        delay(5000);
        ESP.restart();
        return;
    }

    Serial.printf("Sleeping for %ld seconds\n", sleepSeconds);

    // Enter deep sleep
    sleepManager.enterDeepSleep(sleepSeconds);

    // Code never reaches here
}

bool shouldEnterSleepMode() {
    struct tm timeinfo;
    if (!ScheduleManager::getCurrentTime(&timeinfo)) {
        return true; // If we can't get time, try to sleep
    }

    // Build schedule array from config
    int numTimes = configManager.getNumCaptureTimes();
    ScheduleTime schedule[MAX_CAPTURE_TIMES];
    for (int i = 0; i < numTimes; i++) {
        schedule[i].hour = configManager.getCaptureHour(i);
        schedule[i].minute = configManager.getCaptureMinute(i);
    }

    int sleepMargin = configManager.getSleepMarginSec();

    // Calculate seconds until next wake
    long secondsUntil = scheduleManager.getSecondsUntilWake(&timeinfo, schedule, numTimes, sleepMargin);

    // If more than MIN_SLEEP_THRESHOLD_SEC away, sleep
    return (secondsUntil > MIN_SLEEP_THRESHOLD_SEC);
}
