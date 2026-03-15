#include <Arduino.h>
#include <WiFi.h>
#include "globals.h"
#include "ConfigManager.h"
#include "OTAManager.h"

// ============================================================================
// OTA Mode — dedicated minimal boot: download and flash a pending update
// ============================================================================

void runOtaMode() {
    // This runs once from loop() after the minimal OTA boot in setup().
    // No camera, no web server, no AsyncTCP — clean environment for OTA.

    Serial.println("\n======================================");
    Serial.println("[OTA] Executing OTA Update (dedicated mode)");
    Serial.println("======================================");

    // Load pending update info from NVS
    OtaUpdateInfo otaInfo = otaManager.loadPendingUpdate();

    if (!otaInfo.available) {
        Serial.println("[OTA] ERROR: No pending update found in NVS (unexpected)");
        otaManager.clearPendingUpdate();
        Serial.println("[OTA] Rebooting normally...");
        delay(1000);
        ESP.restart();
        return;
    }

    Serial.printf("[OTA] Firmware: %s v%s\n", otaInfo.firmwareFile.c_str(), otaInfo.firmwareVersion.c_str());
    Serial.printf("[OTA] Size: %d bytes, SHA256: %s\n", otaInfo.size, otaInfo.sha256.c_str());
    Serial.printf("[OTA] Free heap: %d bytes\n", ESP.getFreeHeap());

    // Clear pending flag NOW, before performUpdate(). The data is already in
    // otaInfo in memory. performUpdate() calls esp_restart() on success, so
    // clearing after would never execute — causing an infinite OTA reboot loop.
    otaManager.clearPendingUpdate();

    // Save firmware filename for post-OTA confirmation (survives the reboot)
    otaManager.saveConfirmInfo(otaInfo.firmwareFile);

    // Perform OTA update (download, flash, validate, reboot)
    OtaResult result = otaManager.performUpdate(otaInfo,
                                                configManager.getAuthToken(),
                                                WiFi.macAddress(),
                                                configManager.getServerUrl());

    if (result == OTA_SUCCESS) {
        // performUpdate() calls esp_restart() on success — never reaches here.
        // But just in case:
        Serial.println("[OTA] Update applied, rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        // OTA failed — record failure for retry limiting
        String errorMsg = "OTA failed in dedicated mode: " + otaManager.getLastError();
        Serial.println(errorMsg);
        otaManager.recordOtaFailure(otaInfo.firmwareFile);

        // Try to send failure confirmation to server
        otaManager.sendConfirmation(configManager.getServerUrl(),
                                   configManager.getAuthToken(),
                                   WiFi.macAddress(),
                                   false,
                                   otaInfo.firmwareFile,
                                   errorMsg);

        Serial.println("[OTA] Rebooting to normal operation...");
        delay(2000);
        ESP.restart();
    }
}
