#include "ConfigManager.h"
#include <ArduinoJson.h>

// Include main configuration with defaults
#include "../../include/config.h"

// Include default WiFi credentials
#if __has_include("../../include/wifi_credentials.h")
    #include "../../include/wifi_credentials.h"
#else
    #define WIFI_SSID ""
    #define WIFI_PASSWORD ""
#endif

// Include default auth token
#if __has_include("../../include/auth_token.h")
    #include "../../include/auth_token.h"
#else
    #define SERVER_URL ""
    #define AUTH_TOKEN ""
#endif

ConfigManager::ConfigManager() {
    loadDefaults();
}

ConfigManager::~ConfigManager() {
    prefs.end();
}

bool ConfigManager::begin() {
    if (!prefs.begin("espcam", false)) {
        Serial.println("Failed to initialize preferences");
        return false;
    }
    
    // Try to load existing config
    if (!load()) {
        Serial.println("No valid config found, using defaults");
        loadDefaults();
        save(); // Save defaults to NVS
    }
    
    return true;
}

void ConfigManager::loadDefaults() {
    // WiFi defaults from included files or empty
    strncpy(config.wifiSsid, WIFI_SSID, MAX_SSID_LENGTH - 1);
    config.wifiSsid[MAX_SSID_LENGTH - 1] = '\0';
    
    strncpy(config.wifiPassword, WIFI_PASSWORD, MAX_PASSWORD_LENGTH - 1);
    config.wifiPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
    
    // Server defaults
    strncpy(config.serverUrl, SERVER_URL, MAX_URL_LENGTH - 1);
    config.serverUrl[MAX_URL_LENGTH - 1] = '\0';
    
    strncpy(config.authToken, AUTH_TOKEN, MAX_TOKEN_LENGTH - 1);
    config.authToken[MAX_TOKEN_LENGTH - 1] = '\0';
    
    // NTP defaults from config.h
    config.gmtOffsetSec = GMT_OFFSET_SEC;
    config.daylightOffsetSec = DAYLIGHT_OFFSET_SEC;
    
    // Load default schedule from config.h
    config.numCaptureTimes = NUM_CAPTURE_TIMES < MAX_CAPTURE_TIMES ? NUM_CAPTURE_TIMES : MAX_CAPTURE_TIMES;
    for (int i = 0; i < config.numCaptureTimes; i++) {
        config.captureTimes[i].hour = CAPTURE_TIMES[i].hour;
        config.captureTimes[i].minute = CAPTURE_TIMES[i].minute;
    }
    
    // Power management defaults
    config.webTimeoutMin = DEFAULT_WEB_TIMEOUT_MIN;
    config.sleepMarginSec = DEFAULT_SLEEP_MARGIN_SEC;
    
    config.isValid = true;
}

bool ConfigManager::load() {
    // Check if configuration exists
    if (!prefs.isKey("isValid")) {
        return false;
    }
    
    // Load all values
    config.isValid = prefs.getBool("isValid", false);
    if (!config.isValid) {
        return false;
    }
    
    prefs.getString("wifiSsid", config.wifiSsid, MAX_SSID_LENGTH);
    prefs.getString("wifiPassword", config.wifiPassword, MAX_PASSWORD_LENGTH);
    prefs.getString("serverUrl", config.serverUrl, MAX_URL_LENGTH);
    prefs.getString("authToken", config.authToken, MAX_TOKEN_LENGTH);
    
    config.gmtOffsetSec = prefs.getLong("gmtOffset", GMT_OFFSET_SEC);
    config.daylightOffsetSec = prefs.getInt("dstOffset", DAYLIGHT_OFFSET_SEC);
    
    config.numCaptureTimes = prefs.getInt("numCaptures", 0);
    if (config.numCaptureTimes > MAX_CAPTURE_TIMES) {
        config.numCaptureTimes = MAX_CAPTURE_TIMES;
    }
    
    // Load schedule
    for (int i = 0; i < config.numCaptureTimes; i++) {
        char hourKey[16], minKey[16];
        snprintf(hourKey, sizeof(hourKey), "hour_%d", i);
        snprintf(minKey, sizeof(minKey), "min_%d", i);
        
        config.captureTimes[i].hour = prefs.getInt(hourKey, 0);
        config.captureTimes[i].minute = prefs.getInt(minKey, 0);
    }
    
    config.webTimeoutMin = prefs.getInt("webTimeout", DEFAULT_WEB_TIMEOUT_MIN);
    config.sleepMarginSec = prefs.getInt("sleepMargin", DEFAULT_SLEEP_MARGIN_SEC);
    
    // Validate loaded configuration
    if (!validateConfig()) {
        Serial.println("Loaded config validation failed");
        return false;
    }
    
    Serial.println("Configuration loaded successfully from NVS");
    return true;
}

bool ConfigManager::save() {
    if (!validateConfig()) {
        Serial.println("Cannot save invalid configuration");
        return false;
    }
    
    prefs.putBool("isValid", true);
    prefs.putString("wifiSsid", config.wifiSsid);
    prefs.putString("wifiPassword", config.wifiPassword);
    prefs.putString("serverUrl", config.serverUrl);
    prefs.putString("authToken", config.authToken);
    
    prefs.putLong("gmtOffset", config.gmtOffsetSec);
    prefs.putInt("dstOffset", config.daylightOffsetSec);
    
    prefs.putInt("numCaptures", config.numCaptureTimes);
    
    // Save schedule
    for (int i = 0; i < config.numCaptureTimes; i++) {
        char hourKey[16], minKey[16];
        snprintf(hourKey, sizeof(hourKey), "hour_%d", i);
        snprintf(minKey, sizeof(minKey), "min_%d", i);
        
        prefs.putInt(hourKey, config.captureTimes[i].hour);
        prefs.putInt(minKey, config.captureTimes[i].minute);
    }
    
    prefs.putInt("webTimeout", config.webTimeoutMin);
    prefs.putInt("sleepMargin", config.sleepMarginSec);
    
    Serial.println("Configuration saved to NVS");
    return true;
}

void ConfigManager::reset() {
    Serial.println("Resetting configuration to factory defaults");
    prefs.clear();
    loadDefaults();
    save();
}

bool ConfigManager::isValid() {
    return validateConfig();
}

bool ConfigManager::validateConfig() {
    // Validate SSID
    if (strlen(config.wifiSsid) == 0) {
        Serial.println("Validation failed: Empty SSID");
        return false;
    }
    
    // Validate server URL (should start with http:// or https://)
    if (strlen(config.serverUrl) < 7) {
        Serial.println("Validation failed: Invalid server URL");
        return false;
    }
    
    // Validate auth token
    if (strlen(config.authToken) == 0) {
        Serial.println("Validation failed: Empty auth token");
        return false;
    }
    
    // Validate timeouts
    if (config.webTimeoutMin < 1 || config.webTimeoutMin > MAX_WEB_TIMEOUT_MIN) {
        Serial.println("Validation failed: Invalid web timeout");
        return false;
    }
    
    if (config.sleepMarginSec < 0 || config.sleepMarginSec > 600) {
        Serial.println("Validation failed: Invalid sleep margin");
        return false;
    }
    
    // Validate schedule
    if (!validateSchedule()) {
        return false;
    }
    
    config.isValid = true;
    return true;
}

bool ConfigManager::validateSchedule() {
    if (config.numCaptureTimes < 1 || config.numCaptureTimes > MAX_CAPTURE_TIMES) {
        Serial.printf("Validation failed: Invalid number of capture times (%d)\n", config.numCaptureTimes);
        return false;
    }
    
    for (int i = 0; i < config.numCaptureTimes; i++) {
        if (config.captureTimes[i].hour < 0 || config.captureTimes[i].hour > 23) {
            Serial.printf("Validation failed: Invalid hour at index %d: %d\n", i, config.captureTimes[i].hour);
            return false;
        }
        if (config.captureTimes[i].minute < 0 || config.captureTimes[i].minute > 59) {
            Serial.printf("Validation failed: Invalid minute at index %d: %d\n", i, config.captureTimes[i].minute);
            return false;
        }
    }
    
    return true;
}

void ConfigManager::setWifiSsid(const char* ssid) {
    strncpy(config.wifiSsid, ssid, MAX_SSID_LENGTH - 1);
    config.wifiSsid[MAX_SSID_LENGTH - 1] = '\0';
}

void ConfigManager::setWifiPassword(const char* password) {
    strncpy(config.wifiPassword, password, MAX_PASSWORD_LENGTH - 1);
    config.wifiPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
}

void ConfigManager::setServerUrl(const char* url) {
    strncpy(config.serverUrl, url, MAX_URL_LENGTH - 1);
    config.serverUrl[MAX_URL_LENGTH - 1] = '\0';
}

void ConfigManager::setAuthToken(const char* token) {
    strncpy(config.authToken, token, MAX_TOKEN_LENGTH - 1);
    config.authToken[MAX_TOKEN_LENGTH - 1] = '\0';
}

void ConfigManager::setWebTimeoutMin(int timeout) {
    if (timeout < 1) timeout = 1;
    if (timeout > MAX_WEB_TIMEOUT_MIN) timeout = MAX_WEB_TIMEOUT_MIN;
    config.webTimeoutMin = timeout;
}

void ConfigManager::clearSchedule() {
    config.numCaptureTimes = 0;
}

bool ConfigManager::addCaptureTime(int hour, int minute) {
    if (config.numCaptureTimes >= MAX_CAPTURE_TIMES) {
        Serial.println("Cannot add capture time: schedule full");
        return false;
    }
    
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        Serial.println("Cannot add capture time: invalid hour/minute");
        return false;
    }
    
    config.captureTimes[config.numCaptureTimes].hour = hour;
    config.captureTimes[config.numCaptureTimes].minute = minute;
    config.numCaptureTimes++;
    
    return true;
}

bool ConfigManager::setCaptureTime(int index, int hour, int minute) {
    if (index < 0 || index >= config.numCaptureTimes) {
        return false;
    }
    
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    
    config.captureTimes[index].hour = hour;
    config.captureTimes[index].minute = minute;
    
    return true;
}

bool ConfigManager::loadFromJson(const char* jsonStr) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Load WiFi settings
    if (doc.containsKey("wifiSsid")) {
        setWifiSsid(doc["wifiSsid"]);
    }
    if (doc.containsKey("wifiPassword")) {
        const char* pwd = doc["wifiPassword"];
        // Only update if not empty and not the masked placeholder
        if (pwd && strlen(pwd) > 0 && strcmp(pwd, "********") != 0) {
            setWifiPassword(pwd);
        }
    }
    
    // Load server settings
    if (doc.containsKey("serverUrl")) {
        setServerUrl(doc["serverUrl"]);
    }
    if (doc.containsKey("authToken")) {
        const char* token = doc["authToken"];
        // Only update if not empty and not the masked placeholder
        if (token && strlen(token) > 0 && strcmp(token, "********") != 0) {
            setAuthToken(token);
        }
    }
    
    // Load NTP settings
    if (doc.containsKey("gmtOffsetSec")) {
        config.gmtOffsetSec = doc["gmtOffsetSec"];
    }
    if (doc.containsKey("daylightOffsetSec")) {
        config.daylightOffsetSec = doc["daylightOffsetSec"];
    }
    
    // Load schedule
    if (doc.containsKey("schedule")) {
        JsonArray schedule = doc["schedule"].as<JsonArray>();
        clearSchedule();
        for (JsonObject item : schedule) {
            int hour = item["hour"];
            int minute = item["minute"];
            addCaptureTime(hour, minute);
        }
    }
    
    // Load power management
    if (doc.containsKey("webTimeoutMin")) {
        setWebTimeoutMin(doc["webTimeoutMin"]);
    }
    if (doc.containsKey("sleepMarginSec")) {
        config.sleepMarginSec = doc["sleepMarginSec"];
    }
    
    return validateConfig();
}

String ConfigManager::toJson() {
    StaticJsonDocument<2048> doc;
    
    doc["wifiSsid"] = config.wifiSsid;
    // Don't include password in JSON export for security
    doc["wifiPassword"] = "********";
    
    doc["serverUrl"] = config.serverUrl;
    // Don't include full token in JSON export for security
    doc["authToken"] = "********";
    
    doc["gmtOffsetSec"] = config.gmtOffsetSec;
    doc["daylightOffsetSec"] = config.daylightOffsetSec;
    
    JsonArray schedule = doc.createNestedArray("schedule");
    for (int i = 0; i < config.numCaptureTimes; i++) {
        JsonObject item = schedule.createNestedObject();
        item["hour"] = config.captureTimes[i].hour;
        item["minute"] = config.captureTimes[i].minute;
    }
    
    doc["webTimeoutMin"] = config.webTimeoutMin;
    doc["sleepMarginSec"] = config.sleepMarginSec;
    
    String output;
    serializeJson(doc, output);
    return output;
}
