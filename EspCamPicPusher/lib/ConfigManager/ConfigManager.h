#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Maximum array sizes
#define MAX_CAPTURE_TIMES 24
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define MAX_URL_LENGTH 256
#define MAX_TOKEN_LENGTH 128

// Configuration structure
struct AppConfig {
    // WiFi settings
    char wifiSsid[MAX_SSID_LENGTH];
    char wifiPassword[MAX_PASSWORD_LENGTH];
    
    // Server settings
    char serverUrl[MAX_URL_LENGTH];
    char authToken[MAX_TOKEN_LENGTH];
    
    // NTP settings
    long gmtOffsetSec;
    int daylightOffsetSec;
    
    // Capture schedule
    int numCaptureTimes;
    struct {
        int hour;
        int minute;
    } captureTimes[MAX_CAPTURE_TIMES];
    
    // Power management
    int webTimeoutMin;
    int sleepMarginSec;
    
    // Validation flag
    bool isValid;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // Initialize and load configuration
    bool begin();
    
    // Load configuration from NVS
    bool load();
    
    // Save configuration to NVS
    bool save();
    
    // Reset to factory defaults
    void reset();
    
    // Validation
    bool isValid();
    
    // Getters
    const char* getWifiSsid() { return config.wifiSsid; }
    const char* getWifiPassword() { return config.wifiPassword; }
    const char* getServerUrl() { return config.serverUrl; }
    const char* getAuthToken() { return config.authToken; }
    long getGmtOffsetSec() { return config.gmtOffsetSec; }
    int getDaylightOffsetSec() { return config.daylightOffsetSec; }
    int getNumCaptureTimes() { return config.numCaptureTimes; }
    int getCaptureHour(int index) { return config.captureTimes[index].hour; }
    int getCaptureMinute(int index) { return config.captureTimes[index].minute; }
    int getWebTimeoutMin() { return config.webTimeoutMin; }
    int getSleepMarginSec() { return config.sleepMarginSec; }
    
    // Get entire config structure
    AppConfig& getConfig() { return config; }
    
    // Setters
    void setWifiSsid(const char* ssid);
    void setWifiPassword(const char* password);
    void setServerUrl(const char* url);
    void setAuthToken(const char* token);
    void setGmtOffsetSec(long offset) { config.gmtOffsetSec = offset; }
    void setDaylightOffsetSec(int offset) { config.daylightOffsetSec = offset; }
    void setWebTimeoutMin(int timeout);
    void setSleepMarginSec(int margin) { config.sleepMarginSec = margin; }
    
    // Schedule management
    void clearSchedule();
    bool addCaptureTime(int hour, int minute);
    bool setCaptureTime(int index, int hour, int minute);
    
    // Configuration from JSON
    bool loadFromJson(const char* jsonStr);
    String toJson();
    
private:
    Preferences prefs;
    AppConfig config;
    
    void loadDefaults();
    bool validateConfig();
    bool validateSchedule();
};

#endif // CONFIG_MANAGER_H
