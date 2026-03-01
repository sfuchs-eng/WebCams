#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_camera.h"
#include "config.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"
#include "WebConfigServer.h"
#include "CameraMutex.h"

// ============================================================================
// Global Variables
// ============================================================================

// Manager instances
ConfigManager configManager;
ScheduleManager scheduleManager;
SleepManager sleepManager;
WebConfigServer* webServer = nullptr;

// Operating mode
enum OperatingMode {
    MODE_CONFIG,    // Web server active for configuration
    MODE_CAPTURE,   // Quick capture and return to sleep
    MODE_WAIT       // Waiting for next capture (not sleeping)
};

OperatingMode currentMode = MODE_CONFIG;

unsigned long lastNtpUpdate = 0;
bool cameraInitialized = false;
bool isApMode = false;  // Track if in AP+STA mode

// ============================================================================
// Function Declarations
// ============================================================================

void setupSerial();
bool setupWiFiSTA();
void setupWiFiAPSTA();
String generateApSsid();
bool isWiFiConnected();
void setupCamera();
void setupTime();
void updateTime();
bool captureAndPostImage();
void blinkLED(int times, int delayMs);

// New mode-specific functions
void runConfigMode();
void runCaptureMode();
void runWaitMode();
void enterSleepMode();
void buildScheduleFromConfig();
bool shouldEnterSleepMode();

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
    setupSerial();
    blinkLED(3, 200); // Visual indication of startup
    
    // Initialize sleep manager
    sleepManager.begin();
    
    // Initialize camera mutex for thread-safe access
    CameraMutex::init();
    
    // Initialize configuration manager
    if (!configManager.begin()) {
        Serial.println("ERROR: Failed to initialize configuration");
        blinkLED(10, 100);
        delay(5000);
        ESP.restart();
    }
    
    // Determine operating mode based on wake reason
    WakeReason wakeReason = sleepManager.getWakeReason();
    
    Serial.printf("\n=== Wake Reason: %s ===\n", sleepManager.getWakeReasonString().c_str());
    
    if (wakeReason == WAKE_POWER_ON) {
        // Fresh boot - enter configuration mode
        Serial.println("=== Entering CONFIGURATION MODE ===");
        currentMode = MODE_CONFIG;
        
        // Try to connect to WiFi
        bool wifiConnected = setupWiFiSTA();
        
        if (!wifiConnected) {
            // WiFi failed, start AP+STA mode
            Serial.println("WiFi connection failed, starting AP+STA mode");
            setupWiFiAPSTA();
            isApMode = true;
        } else {
            isApMode = false;
        }
        
        setupCamera();
        
        // Only setup time if WiFi is connected (NTP requires internet)
        if (wifiConnected || isWiFiConnected()) {
            setupTime();
        } else {
            Serial.println("Skipping NTP setup (no WiFi connection)");
        }
        
        // Start web server
        webServer = new WebConfigServer(&configManager);
        webServer->setCameraReady(cameraInitialized);
        webServer->setCaptureCallback(captureAndPostImage);
        webServer->setApMode(isApMode);
        if (!webServer->begin()) {
            Serial.println("ERROR: Failed to start web server");
        }
        
        Serial.println("\n=== EspCamPicPusher Ready - Config Mode ===");
        if (isApMode) {
            Serial.printf("AP Mode: Connect to %s\n", generateApSsid().c_str());
            Serial.println("Configuration URL: http://192.168.4.1/");
            if (isWiFiConnected()) {
                Serial.printf("Also available at: http://%s/\n", WiFi.localIP().toString().c_str());
            }
        } else {
            Serial.printf("Configuration URL: http://%s/\n", WiFi.localIP().toString().c_str());
        }
        Serial.printf("Web timeout: %d minutes\n", configManager.getWebTimeoutMin());
        Serial.println("===========================================\n");
        
    } else if (wakeReason == WAKE_TIMER) {
        // Timer wake - capture and return to sleep
        Serial.println("=== Entering CAPTURE MODE ===");
        currentMode = MODE_CAPTURE;
        
        // Get WiFi retry count
        uint32_t retryCount = sleepManager.getWifiRetryCount();
        Serial.printf("WiFi retry attempt: %u/5\n", retryCount);
        
        // Try to connect to WiFi
        bool wifiConnected = setupWiFiSTA();
        
        if (!wifiConnected) {
            // WiFi failed
            sleepManager.incrementFailedCaptures();
            
            if (retryCount < 5) {
                // Retry: increment counter and sleep for 5 minutes
                sleepManager.incrementWifiRetryCount();
                Serial.printf("\nWiFi retry %u/5 failed, sleeping for 5 minutes...\n", retryCount + 1);
                sleepManager.enterDeepSleep(300);  // 5 minutes = 300 seconds
                // Code never reaches here
            } else {
                // Max retries reached, skip this capture and sleep until next scheduled time
                Serial.println("\nWiFi unavailable after 5 retries, sleeping until next scheduled capture");
                sleepManager.resetWifiRetryCount();
                enterSleepMode();
                // Code never reaches here
            }
        }
        
        // WiFi connected successfully - reset retry counter
        sleepManager.resetWifiRetryCount();
        
        setupCamera();
        
        // Check if NTP sync needed (>24 hours since last)
        time_t lastSync = sleepManager.getLastNtpSync();
        time_t now = time(nullptr);
        if (lastSync == 0 || (now - lastSync) > 86400) {
            Serial.println("NTP sync required...");
            setupTime();
            sleepManager.setLastNtpSync(time(nullptr));
        } else {
            Serial.println("Using RTC time (NTP sync not required)");
        }
        
    } else {
        // Unknown wake - enter config mode to be safe
        Serial.println("=== Unknown wake reason - entering CONFIG MODE ===");
        currentMode = MODE_CONFIG;
        
        bool wifiConnected = setupWiFiSTA();
        if (!wifiConnected) {
            Serial.println("WiFi connection failed, starting AP+STA mode");
            setupWiFiAPSTA();
            isApMode = true;
        } else {
            isApMode = false;
        }
        
        setupCamera();
        
        if (wifiConnected || isWiFiConnected()) {
            setupTime();
        } else {
            Serial.println("Skipping NTP setup (no WiFi connection)");
        }
        
        // Start web server
        webServer = new WebConfigServer(&configManager);
        webServer->setCameraReady(cameraInitialized);
        webServer->setCaptureCallback(captureAndPostImage);
        webServer->setApMode(isApMode);
        if (!webServer->begin()) {
            Serial.println("ERROR: Failed to start web server");
        }
    }
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    switch (currentMode) {
        case MODE_CONFIG:
            runConfigMode();
            break;
            
        case MODE_CAPTURE:
            runCaptureMode();
            break;
            
        case MODE_WAIT:
            runWaitMode();
            break;
    }
    
    delay(100); // Small delay to prevent tight loop
}

// ============================================================================
// Serial Setup
// ============================================================================

void setupSerial() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== EspCamPicPusher ===");
    Serial.println("Starting...");
}

// ============================================================================
// WiFi Setup Functions
// ============================================================================

String generateApSsid() {
    // Get last 4 characters of MAC address for unique SSID
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String suffix = mac.substring(mac.length() - 4);
    suffix.toUpperCase();
    return "ESP32-CAM-" + suffix;
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void setupWiFiAPSTA() {
    Serial.println("\n--- WiFi AP+STA Setup ---");
    
    // Get configured credentials
    const char* ssid = configManager.getWifiSsid();
    const char* password = configManager.getWifiPassword();
    
    // Generate AP SSID
    String apSsid = generateApSsid();
    
    // Start AP+STA mode
    WiFi.mode(WIFI_AP_STA);
    
    // Configure and start Access Point (no password)
    bool apStarted = WiFi.softAP(apSsid.c_str());
    if (apStarted) {
        Serial.println("Access Point started");
        Serial.printf("AP SSID: %s\n", apSsid.c_str());
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("ERROR: Failed to start Access Point");
    }
    
    // Attempt to connect to configured WiFi
    Serial.printf("Attempting STA connection to: %s\n", ssid);
    WiFi.begin(ssid, password);
    
    Serial.println("\n=== AP+STA Mode Active ===");
    Serial.printf("Connect to: %s\n", apSsid.c_str());
    Serial.printf("Configuration URL: http://192.168.4.1\n");
    Serial.println("===========================\n");
}

bool setupWiFiSTA() {
    Serial.println("\n--- WiFi STA Setup ---");
    
    const char* ssid = configManager.getWifiSsid();
    const char* password = configManager.getWifiPassword();
    
    Serial.printf("Connecting to: %s\n", ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
        return true;
    } else {
        Serial.println("\nWiFi connection failed!");
        return false;
    }
}

// ============================================================================
// Camera Setup
// ============================================================================

void setupCamera() {
    Serial.println("\n--- Camera Setup ---");
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = CAMERA_FRAME_SIZE;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = 1;
    
    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        cameraInitialized = false;
        
        // In capture mode, count as failed attempt
        if (currentMode == MODE_CAPTURE) {
            sleepManager.incrementFailedCaptures();
        }
        return;
    }
    
    cameraInitialized = true;
    Serial.println("Camera initialized successfully");
    
    // Get sensor for additional settings
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
        s->set_aec2(s, 0);           // 0 = disable , 1 = enable
        s->set_ae_level(s, 0);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    }
}

// ============================================================================
// Time Setup and Update
// ============================================================================

void setupTime() {
    Serial.println("\n--- Time Setup ---");
    Serial.printf("NTP Server: %s\n", NTP_SERVER);
    
    long gmtOffset = configManager.getGmtOffsetSec();
    int dstOffset = configManager.getDaylightOffsetSec();
    
    configTime(gmtOffset, dstOffset, NTP_SERVER, NTP_SERVER2);
    
    Serial.print("Waiting for NTP time sync");
    int attempts = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && attempts < 20) {
        Serial.print(".");
        delay(500);
        attempts++;
    }
    
    if (attempts < 20) {
        Serial.println("\nTime synchronized!");
        Serial.println(ScheduleManager::formatTime(&timeinfo));
        lastNtpUpdate = millis();
    } else {
        Serial.println("\nFailed to obtain time!");
    }
}

void updateTime() {
    Serial.println("Updating time from NTP...");
    long gmtOffset = configManager.getGmtOffsetSec();
    int dstOffset = configManager.getDaylightOffsetSec();
    configTime(gmtOffset, dstOffset, NTP_SERVER, NTP_SERVER2);
    delay(1000);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println(ScheduleManager::formatTime(&timeinfo));
    }
}

// ============================================================================
// Mode Functions
// ============================================================================

void runConfigMode() {
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
            webServer->begin();
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
            webServer->begin();
            return;
        }
    }
    
    // Enter sleep mode for next capture
    enterSleepMode();
}

void runWaitMode() {
    static unsigned long lastCheck = 0;
    
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
    
    // Build schedule array from config
    int numTimes = configManager.getNumCaptureTimes();
    ScheduleTime schedule[MAX_CAPTURE_TIMES];
    for (int i = 0; i < numTimes; i++) {
        schedule[i].hour = configManager.getCaptureHour(i);
        schedule[i].minute = configManager.getCaptureMinute(i);
    }
    
    // Check if it's time to capture
    if (scheduleManager.isTimeToCapture(&timeinfo, schedule, numTimes)) {
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

// ============================================================================
// Scheduled Capture Logic (REMOVED - replaced by mode functions)
// ============================================================================

// ============================================================================
// Image Capture and Upload
// ============================================================================

bool captureAndPostImage() {
    Serial.println("\n--- Capturing Image ---");
    
    if (!cameraInitialized) {
        Serial.println("Camera not initialized!");
        return false;
    }
    
    // Acquire camera mutex to prevent concurrent access from web server
    if (!CameraMutex::lock(5000)) {
        Serial.println("Failed to acquire camera mutex (timeout)");
        return false;
    }
    
    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();
    
    if (!fb) {
        Serial.println("Camera capture failed!");
        CameraMutex::unlock();
        return false;
    }
    
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    // Prepare HTTPS POST
    Serial.println("\n--- Uploading Image ---");
    WiFiClientSecure client;
    client.setInsecure(); // For testing; use proper certificate validation in production
    
    HTTPClient http;
    
    const char* serverUrl = configManager.getServerUrl();
    http.begin(client, serverUrl);
    
    // Set headers
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Auth-Token", configManager.getAuthToken());
    http.addHeader("X-Device-ID", WiFi.macAddress());
    
    // Get formatted timestamp
    struct tm timeinfo;
    String timestamp = "unknown";
    if (ScheduleManager::getCurrentTime(&timeinfo)) {
        timestamp = ScheduleManager::formatTime(&timeinfo);
    }
    http.addHeader("X-Timestamp", timestamp);
    
    // Send POST request
    int httpResponseCode = http.POST(fb->buf, fb->len);
    
    // Release frame buffer and mutex
    esp_camera_fb_return(fb);
    CameraMutex::unlock();
    
    // Check response
    bool success = false;
    if (httpResponseCode > 0) {
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        String response = http.getString();
        Serial.println("Response: " + response);
        
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            Serial.println("✓ Image uploaded successfully!");
            success = true;
        } else {
            Serial.println("✗ Upload failed with HTTP error");
        }
    } else {
        Serial.printf("✗ Upload failed: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
    return success;
}

// ============================================================================
// LED Blink Utility (using built-in LED if available)
// ============================================================================

void blinkLED(int times, int delayMs) {
    // XIAO ESP32S3 doesn't have a standard LED_BUILTIN, but we can try
    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
    #endif
}