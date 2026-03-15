#pragma once

#include <Arduino.h>

// Forward declarations (full headers included by each .cpp that uses them)
class ConfigManager;
class ScheduleManager;
class SleepManager;
class WebConfigServer;
class OTAManager;

// ============================================================================
// Operating Mode
// ============================================================================

enum OperatingMode {
    MODE_CONFIG,    // Web server active for configuration
    MODE_CAPTURE,   // Quick capture and return to sleep
    MODE_WAIT,      // Waiting for next capture (not sleeping)
    MODE_OTA        // Dedicated OTA update mode (minimal boot)
};

// ============================================================================
// Global Variables (defined in main.cpp)
// ============================================================================

extern ConfigManager configManager;
extern ScheduleManager scheduleManager;
extern SleepManager sleepManager;
extern WebConfigServer* webServer;
extern OTAManager otaManager;

extern OperatingMode currentMode;
extern unsigned long lastNtpUpdate;
extern bool cameraInitialized;
extern bool isApMode;
extern bool otaValidationPending;
extern String pendingOtaFirmwareFile;

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
bool captureAndPostImage();
void blinkLED(int times, int delayMs);

void runConfigMode();
void runCaptureMode();
void runWaitMode();
void runOtaMode();
void enterSleepMode();
bool shouldEnterSleepMode();
void handleOtaUpdate(const String& response);
void validateOtaUpdate();
String resolveHostname();
