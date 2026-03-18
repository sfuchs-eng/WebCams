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
        // Wake time is already behind us — this happens when the device woke up
        // sleepMarginSec seconds before the scheduled capture and completed the
        // capture while still inside that wake window (current clock is still
        // in the minute before the scheduled time).  Without this correction
        // the same schedule entry would be treated as "next", the computed
        // wake time would be in the past, and the device would restart —
        // triggering a duplicate capture via CONFIG mode.
        // Fix: advance past the wake window and recalculate.
        Serial.println("Wake time is within current capture window, advancing past it...");
        timeinfo.tm_sec += sleepMargin + 30;  // push past the entire wake window
        mktime(&timeinfo);                    // normalize (propagates overflow into tm_min etc.)
        sleepSeconds = scheduleManager.getSecondsUntilWake(&timeinfo, schedule, numTimes, sleepMargin);
        Serial.printf("Recalculated sleep duration: %ld seconds\n", sleepSeconds);
    }

    if (sleepSeconds <= 0) {
        Serial.println("ERROR: Invalid sleep duration after adjustment, restarting...");
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
