#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_camera.h"
#include "config.h"

// Include WiFi credentials from separate file
// Create include/wifi_credentials.h from include/wifi_credentials_template.h
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#else
    #error "Please create include/wifi_credentials.h from include/wifi_credentials_template.h"
#endif

// Include server authentication token from separate file
// Create include/auth_token.h from include/auth_token_template.h with your server URL and authentication token
#if __has_include("auth_token.h")
    #include "auth_token.h"
#else
    #error "Please create include/auth_token.h from include/auth_token_template.h with your server URL and authentication token"
#endif

// ============================================================================
// Global Variables
// ============================================================================

unsigned long lastNtpUpdate = 0;
bool captureExecuted[NUM_CAPTURE_TIMES] = {false};

// ============================================================================
// Function Declarations
// ============================================================================

void setupSerial();
void setupWiFi();
void setupCamera();
void setupTime();
void updateTime();
bool captureAndPostImage();
void checkScheduledCapture();
String getFormattedTime();
void blinkLED(int times, int delayMs);

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
    setupSerial();
    blinkLED(3, 200); // Visual indication of startup
    
    setupWiFi();
    setupCamera();
    setupTime();
    
    Serial.println("\n=== EspCamPicPusher Ready ===");
    Serial.println("Configuration:");
    Serial.printf("  Server URL: %s\n", SERVER_URL);
    Serial.printf("  Capture times: %d per day\n", NUM_CAPTURE_TIMES);
    for (int i = 0; i < NUM_CAPTURE_TIMES; i++) {
        Serial.printf("    %02d:%02d\n", CAPTURE_TIMES[i].hour, CAPTURE_TIMES[i].minute);
    }
    Serial.println("=============================\n");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Update time periodically
    if (millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
        updateTime();
        lastNtpUpdate = millis();
    }
    
    // Check if it's time to capture and post
    checkScheduledCapture();
    
    // Small delay to prevent tight loop
    delay(1000);
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
// WiFi Setup
// ============================================================================

void setupWiFi() {
    Serial.println("\n--- WiFi Setup ---");
    Serial.printf("Connecting to: %s\n", WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
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
    } else {
        Serial.println("\nWiFi connection failed!");
        Serial.println("Please check your credentials in wifi_credentials.h");
        delay(5000);
        ESP.restart();
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
        delay(5000);
        ESP.restart();
    }
    
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
    
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, NTP_SERVER2);
    
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
        Serial.println(getFormattedTime());
        lastNtpUpdate = millis();
    } else {
        Serial.println("\nFailed to obtain time!");
    }
}

void updateTime() {
    Serial.println("Updating time from NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, NTP_SERVER2);
    delay(1000);
    Serial.println(getFormattedTime());
}

String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Failed to obtain time";
    }
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

// ============================================================================
// Scheduled Capture Logic
// ============================================================================

void checkScheduledCapture() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return;
    }
    
    for (int i = 0; i < NUM_CAPTURE_TIMES; i++) {
        // Check if current time matches scheduled time
        if (timeinfo.tm_hour == CAPTURE_TIMES[i].hour && 
            timeinfo.tm_min == CAPTURE_TIMES[i].minute) {
            
            // If not already executed for this minute
            if (!captureExecuted[i]) {
                Serial.println("\n======================================");
                Serial.printf("Scheduled capture triggered: %02d:%02d\n", 
                    CAPTURE_TIMES[i].hour, CAPTURE_TIMES[i].minute);
                Serial.println("======================================");
                
                if (captureAndPostImage()) {
                    captureExecuted[i] = true;
                    blinkLED(2, 100); // Success indication
                } else {
                    blinkLED(5, 50); // Error indication
                }
            }
        } else {
            // Reset flag when minute changes
            captureExecuted[i] = false;
        }
    }
}

// ============================================================================
// Image Capture and Upload
// ============================================================================

bool captureAndPostImage() {
    Serial.println("\n--- Capturing Image ---");
    
    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed!");
        return false;
    }
    
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    // Prepare HTTPS POST
    Serial.println("\n--- Uploading Image ---");
    WiFiClientSecure client;
    client.setInsecure(); // For testing; use proper certificate validation in production
    
    HTTPClient http;
    http.begin(client, SERVER_URL);
    
    // Set headers
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Auth-Token", AUTH_TOKEN);  // Using X-Auth-Token for Apache/PHP-FPM compatibility
    http.addHeader("X-Device-ID", WiFi.macAddress());
    http.addHeader("X-Timestamp", getFormattedTime());
    
    // Send POST request
    int httpResponseCode = http.POST(fb->buf, fb->len);
    
    // Release frame buffer
    esp_camera_fb_return(fb);
    
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