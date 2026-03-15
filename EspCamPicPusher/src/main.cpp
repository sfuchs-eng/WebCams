#include <Arduino.h>
#include "globals.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "SleepManager.h"
#include "WebConfigServer.h"
#include "OTAManager.h"

// ============================================================================
// Global Variable Definitions  (declarations and externs in globals.h)
// ============================================================================

ConfigManager configManager;
ScheduleManager scheduleManager;
SleepManager sleepManager;
WebConfigServer* webServer = nullptr;
OTAManager otaManager;

OperatingMode currentMode = MODE_CONFIG;

unsigned long lastNtpUpdate = 0;
bool cameraInitialized = false;
bool isApMode = false;
bool otaValidationPending = false;
String pendingOtaFirmwareFile = "";

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
            
        case MODE_OTA:
            runOtaMode();
            break;
    }
    
    delay(100); // Small delay to prevent tight loop
}
