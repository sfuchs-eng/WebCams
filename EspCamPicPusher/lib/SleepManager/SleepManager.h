#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include <time.h>

// RTC memory structure for persistent data across deep sleep
typedef struct {
    uint32_t magic;           // Magic number to validate data
    uint32_t bootCount;       // Number of boots
    time_t lastNtpSync;       // Last successful NTP sync
    uint32_t failedCaptures;  // Consecutive failed capture attempts
    uint32_t wifiRetryCount;  // WiFi retry attempts during timer wake
} rtc_data_t;

enum WakeReason {
    WAKE_POWER_ON,      // Fresh boot/power cycle
    WAKE_TIMER,         // Woken by timer for scheduled capture
    WAKE_EXT,           // Woken by external trigger (button, etc.)
    WAKE_UNKNOWN        // Unknown wake reason
};

class SleepManager {
public:
    SleepManager();
    
    /**
     * Initialize sleep manager and read RTC data
     */
    void begin();
    
    /**
     * Get the reason for waking up
     * @return Wake reason enum
     */
    WakeReason getWakeReason();
    
    /**
     * Enter deep sleep for specified duration
     * @param seconds Number of seconds to sleep
     */
    void enterDeepSleep(uint64_t seconds);
    
    /**
     * Get last NTP sync time from RTC memory
     * @return Last sync time as epoch timestamp
     */
    time_t getLastNtpSync();
    
    /**
     * Update last NTP sync time in RTC memory
     * @param syncTime Sync time as epoch timestamp
     */
    void setLastNtpSync(time_t syncTime);
    
    /**
     * Get boot count from RTC memory
     * @return Number of boots since last power cycle
     */
    uint32_t getBootCount();
    
    /**
     * Increment boot count
     */
    void incrementBootCount();
    
    /**
     * Get failed capture count
     * @return Number of consecutive failed captures
     */
    uint32_t getFailedCaptureCount();
    
    /**
     * Increment failed capture counter
     */
    void incrementFailedCaptures();
    
    /**
     * Reset failed capture counter
     */
    void resetFailedCaptures();
    
    /**
     * Check if ESP should stay awake due to repeated failures
     * @param threshold Number of failures before staying awake
     * @return True if should stay awake
     */
    bool shouldStayAwake(uint32_t threshold = 3);
    
    /**
     * Prepare for deep sleep (cleanup)
     */
    void prepare();
    
    /**
     * Get wake reason as string for logging
     * @return String description of wake reason
     */
    String getWakeReasonString();
    
    /**
     * Get WiFi retry count from RTC memory
     * @return Number of WiFi retry attempts
     */
    uint32_t getWifiRetryCount();
    
    /**
     * Set WiFi retry count in RTC memory
     * @param count Number of retry attempts
     */
    void setWifiRetryCount(uint32_t count);
    
    /**
     * Increment WiFi retry counter
     */
    void incrementWifiRetryCount();
    
    /**
     * Reset WiFi retry counter
     */
    void resetWifiRetryCount();
    
private:
    rtc_data_t rtcData;
    WakeReason wakeReason;
    static const uint32_t RTC_DATA_MAGIC = 0xCAFEBABE;
    
    /**
     * Initialize RTC data structure with defaults
     */
    void initRtcData();
    
    /**
     * Validate RTC data structure
     * @return True if data is valid
     */
    bool validateRtcData();
    
    /**
     * Save data to RTC memory
     */
    void saveRtcData();
    
    /**
     * Load data from RTC memory
     */
    void loadRtcData();
};

#endif // SLEEP_MANAGER_H
