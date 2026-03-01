#include "ScheduleManager.h"

ScheduleManager::ScheduleManager() {
}

time_t ScheduleManager::getNextWakeTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes, int sleepMarginSec) {
    if (!currentTime || !schedule || numTimes <= 0) {
        Serial.println("Error: Invalid parameters for getNextWakeTime");
        return 0;
    }
    
    // Get next capture time
    time_t nextCaptureTime = getNextCaptureTime(currentTime, schedule, numTimes);
    if (nextCaptureTime == 0) {
        return 0;
    }
    
    // Subtract sleep margin to get wake time
    time_t wakeTime = nextCaptureTime - sleepMarginSec;
    
    return wakeTime;
}

long ScheduleManager::getSecondsUntilWake(struct tm* currentTime, ScheduleTime* schedule, int numTimes, int sleepMarginSec) {
    time_t wakeTime = getNextWakeTime(currentTime, schedule, numTimes, sleepMarginSec);
    if (wakeTime == 0) {
        return -1;
    }
    
    // Get current time as epoch
    time_t now = mktime(currentTime);
    
    // Calculate difference
    long secondsUntil = (long)(wakeTime - now);
    
    // If negative or very small, something is wrong
    if (secondsUntil < 0) {
        Serial.println("Warning: Wake time is in the past");
        return 0; // Wake immediately
    }
    
    return secondsUntil;
}

time_t ScheduleManager::getNextCaptureTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes) {
    if (!currentTime || !schedule || numTimes <= 0) {
        Serial.println("Error: Invalid parameters for getNextCaptureTime");
        return 0;
    }
    
    int nextIndex = findNextScheduledTime(currentTime, schedule, numTimes);
    
    struct tm nextTime = *currentTime;
    
    if (nextIndex >= 0) {
        // Found a time later today
        nextTime.tm_hour = schedule[nextIndex].hour;
        nextTime.tm_min = schedule[nextIndex].minute;
        nextTime.tm_sec = 0;
    } else {
        // All times have passed today, use first time tomorrow
        nextTime.tm_mday += 1; // Add one day
        nextTime.tm_hour = schedule[0].hour;
        nextTime.tm_min = schedule[0].minute;
        nextTime.tm_sec = 0;
    }
    
    // Normalize the time structure (handles month/year rollovers)
    time_t result = mktime(&nextTime);
    
    return result;
}

bool ScheduleManager::isTimeToCapture(struct tm* currentTime, ScheduleTime* schedule, int numTimes) {
    if (!currentTime || !schedule || numTimes <= 0) {
        return false;
    }
    
    int currentHour = currentTime->tm_hour;
    int currentMin = currentTime->tm_min;
    
    // Check if current time matches any scheduled time
    for (int i = 0; i < numTimes; i++) {
        if (currentHour == schedule[i].hour && currentMin == schedule[i].minute) {
            return true;
        }
    }
    
    return false;
}

int ScheduleManager::findNextScheduledTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes) {
    int currentHour = currentTime->tm_hour;
    int currentMin = currentTime->tm_min;
    
    // Find the first scheduled time that is after current time
    for (int i = 0; i < numTimes; i++) {
        int cmp = compareTimes(schedule[i].hour, schedule[i].minute, currentHour, currentMin);
        if (cmp > 0) {
            // This scheduled time is after current time
            return i;
        }
    }
    
    // No scheduled time found after current time today
    return -1;
}

time_t ScheduleManager::makeTime(int year, int month, int day, int hour, int minute, int second) {
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;  // Years since 1900
    timeinfo.tm_mon = month - 1;     // Months since January (0-11)
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;          // Let mktime determine DST
    
    return mktime(&timeinfo);
}

int ScheduleManager::compareTimes(int hour1, int min1, int hour2, int min2) {
    if (hour1 < hour2) return -1;
    if (hour1 > hour2) return 1;
    
    // Hours are equal, compare minutes
    if (min1 < min2) return -1;
    if (min1 > min2) return 1;
    
    // Times are equal
    return 0;
}

String ScheduleManager::formatTime(struct tm* timeinfo) {
    if (!timeinfo) {
        return "Invalid time";
    }
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

bool ScheduleManager::getCurrentTime(struct tm* timeinfo) {
    if (!timeinfo) {
        return false;
    }
    
    return getLocalTime(timeinfo);
}
