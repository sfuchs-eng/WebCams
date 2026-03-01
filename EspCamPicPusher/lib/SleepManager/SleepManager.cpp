#include "SleepManager.h"
#include <WiFi.h>

// Declare RTC data in slow RTC memory (survives deep sleep)
RTC_DATA_ATTR static rtc_data_t rtc_data;

SleepManager::SleepManager() {
    wakeReason = WAKE_UNKNOWN;
}

void SleepManager::begin() {
    loadRtcData();
    
    // Determine wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            wakeReason = WAKE_TIMER;
            Serial.println("Wake reason: Timer");
            break;
            
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
            wakeReason = WAKE_EXT;
            Serial.println("Wake reason: External");
            break;
            
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            wakeReason = WAKE_POWER_ON;
            Serial.println("Wake reason: Power-on or reset");
            break;
    }
    
    // Increment boot count
    incrementBootCount();
    
    Serial.printf("Boot count: %u\n", rtcData.bootCount);
    Serial.printf("Failed captures: %u\n", rtcData.failedCaptures);
    Serial.printf("WiFi retries: %u\n", rtcData.wifiRetryCount);
    
    if (rtcData.lastNtpSync > 0) {
        Serial.printf("Last NTP sync: %lu\n", rtcData.lastNtpSync);
    } else {
        Serial.println("Last NTP sync: Never");
    }
}

void SleepManager::loadRtcData() {
    // Copy from RTC memory
    memcpy(&rtcData, &rtc_data, sizeof(rtc_data_t));
    
    // Validate data
    if (!validateRtcData()) {
        Serial.println("RTC data invalid, initializing...");
        initRtcData();
        saveRtcData();
    }
}

void SleepManager::saveRtcData() {
    rtcData.magic = RTC_DATA_MAGIC;
    memcpy(&rtc_data, &rtcData, sizeof(rtc_data_t));
}

bool SleepManager::validateRtcData() {
    return rtcData.magic == RTC_DATA_MAGIC;
}

void SleepManager::initRtcData() {
    rtcData.magic = RTC_DATA_MAGIC;
    rtcData.bootCount = 0;
    rtcData.lastNtpSync = 0;
    rtcData.failedCaptures = 0;
    rtcData.wifiRetryCount = 0;
}

WakeReason SleepManager::getWakeReason() {
    return wakeReason;
}

String SleepManager::getWakeReasonString() {
    switch (wakeReason) {
        case WAKE_POWER_ON:
            return "Power-On/Reset";
        case WAKE_TIMER:
            return "Timer";
        case WAKE_EXT:
            return "External";
        case WAKE_UNKNOWN:
        default:
            return "Unknown";
    }
}

time_t SleepManager::getLastNtpSync() {
    return rtcData.lastNtpSync;
}

void SleepManager::setLastNtpSync(time_t syncTime) {
    rtcData.lastNtpSync = syncTime;
    saveRtcData();
}

uint32_t SleepManager::getBootCount() {
    return rtcData.bootCount;
}

void SleepManager::incrementBootCount() {
    rtcData.bootCount++;
    saveRtcData();
}

uint32_t SleepManager::getFailedCaptureCount() {
    return rtcData.failedCaptures;
}

void SleepManager::incrementFailedCaptures() {
    rtcData.failedCaptures++;
    saveRtcData();
    Serial.printf("Failed captures: %u\n", rtcData.failedCaptures);
}

void SleepManager::resetFailedCaptures() {
    rtcData.failedCaptures = 0;
    saveRtcData();
}

bool SleepManager::shouldStayAwake(uint32_t threshold) {
    return rtcData.failedCaptures >= threshold;
}

void SleepManager::prepare() {
    Serial.println("Preparing for deep sleep...");
    
    // Disconnect WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    // Small delay to ensure cleanup
    delay(200);
}

uint32_t SleepManager::getWifiRetryCount() {
    return rtcData.wifiRetryCount;
}

void SleepManager::setWifiRetryCount(uint32_t count) {
    rtcData.wifiRetryCount = count;
    saveRtcData();
}

void SleepManager::incrementWifiRetryCount() {
    rtcData.wifiRetryCount++;
    saveRtcData();
    Serial.printf("WiFi retry count: %u\n", rtcData.wifiRetryCount);
}

void SleepManager::resetWifiRetryCount() {
    rtcData.wifiRetryCount = 0;
    saveRtcData();
}

void SleepManager::enterDeepSleep(uint64_t seconds) {
    Serial.printf("\n=== Entering Deep Sleep for %llu seconds ===\n", seconds);
    Serial.println("Next wake time will be approximately:");
    
    time_t now = time(nullptr);
    time_t wakeTime = now + seconds;
    struct tm* wakeTm = localtime(&wakeTime);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", wakeTm);
    Serial.println(buffer);
    
    Serial.flush(); // Ensure all serial data is sent
    
    // Prepare for sleep
    prepare();
    
    // Configure timer wakeup
    uint64_t sleepDuration = seconds * 1000000ULL; // Convert to microseconds
    esp_sleep_enable_timer_wakeup(sleepDuration);
    
    // Enter deep sleep
    esp_deep_sleep_start();
    
    // Code never reaches here
}
