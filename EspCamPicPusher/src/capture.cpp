#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "CameraMutex.h"
#include "CameraCapture.h"
#include "OTAManager.h"
#include "RemoteLogger.h"

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

    // Capture image with sensor warm-up for proper AWB/AEC/AGC
    camera_fb_t * fb = CameraCapture::captureFrame(true);

    if (!fb) {
        CameraMutex::unlock();
        return false;
    }

    // Prepare HTTPS POST
    Serial.println("\n--- Uploading Image ---");
    WiFiClientSecure client;
    client.setInsecure(); // For testing; use proper certificate validation in production

    HTTPClient http;

    // Build upload URL from base URL (base URL can include path like /cams)
    String uploadUrl = String(configManager.getServerUrl()) + "/upload.php";
    http.begin(client, uploadUrl);

    // Set headers
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Auth-Token", configManager.getAuthToken());
    http.addHeader("X-Device-ID", WiFi.macAddress());
    http.addHeader("X-Firmware-Version", otaManager.getFirmwareVersion());

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
    CameraCapture::releaseFrame(fb);
    CameraMutex::unlock();

    // Check response
    bool success = false;
    String response = "";
    if (httpResponseCode > 0) {
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        response = http.getString();
        Serial.println("Response: " + response);

        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            Serial.println("✓ Image uploaded successfully!");
            success = true;

            // If validation pending, confirm OTA first — BEFORE checking for new OTA.
            // Without this guard the server still sees ota_scheduled set and would
            // offer the same firmware again, sending the device into an OTA loop
            // before validateOtaUpdate() ever runs.
            if (otaValidationPending) {
                validateOtaUpdate();
            }

            // Check for OTA available (only when not in a validation cycle)
            if (!otaValidationPending && otaManager.isOtaAvailable(response)) {
                Serial.println("\n[OTA] Update available in server response");

                // handleOtaUpdate saves OTA info to NVS and reboots into
                // dedicated OTA mode (no camera, no web server, no AsyncTCP).
                // This call does not return — the device will restart.
                handleOtaUpdate(response);
            }
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
// OTA Scheduling and Validation (called exclusively from captureAndPostImage)
// ============================================================================

void handleOtaUpdate(const String& response) {
    Serial.println("\n======================================");
    Serial.println("[OTA] OTA Update Available - Preparing Reboot");
    Serial.println("======================================");

    OtaUpdateInfo otaInfo = otaManager.parseOtaInfo(response);

    if (!otaInfo.available) {
        Serial.println("[OTA] No update available");
        RemoteLogger::warn("OTA", "parseOtaInfo returned no update");
        return;
    }

    // Check if this firmware has failed too many times (max 3 retries)
    static const uint32_t OTA_MAX_RETRIES = 3;
    uint32_t failCount = otaManager.getOtaFailureCount(otaInfo.firmwareFile);
    if (failCount >= OTA_MAX_RETRIES) {
        Serial.printf("[OTA] Firmware %s has failed %u times (max %u) — skipping\n",
                      otaInfo.firmwareFile.c_str(), failCount, OTA_MAX_RETRIES);
        RemoteLogger::warn("OTA", "Skipping OTA: max retries exceeded for " + otaInfo.firmwareFile);
        return;
    }

    // Log OTA intent with details (while RemoteLogger is still healthy)
    DynamicJsonDocument doc(512);
    JsonObject context = doc.to<JsonObject>();
    context["firmware_file"] = otaInfo.firmwareFile;
    context["version"] = otaInfo.firmwareVersion;
    context["size"] = otaInfo.size;
    context["attempt"] = failCount + 1;
    context["max_retries"] = OTA_MAX_RETRIES;
    RemoteLogger::info("OTA", "Saving OTA info and rebooting to OTA mode", context);

    // Save OTA metadata to NVS so the dedicated OTA boot can use it
    if (!otaManager.savePendingUpdate(otaInfo)) {
        Serial.println("[OTA] ERROR: Failed to save pending update to NVS");
        RemoteLogger::error("OTA", "Failed to save OTA info to NVS");
        return;
    }

    // Flush remote logs before reboot (still in a clean state, no async_tcp issues)
    RemoteLogger::flush();

    Serial.println("[OTA] Rebooting into dedicated OTA mode...");
    delay(1000);
    ESP.restart();
    // Never reaches here
}

void validateOtaUpdate() {
    Serial.println("\n[OTA] Validating update after first successful capture");

    if (otaManager.confirmUpdate()) {
        // Mark partition as valid
        Serial.println("[OTA] Update confirmed successfully");

        // Clear any leftover OTA data from NVS
        otaManager.clearPendingUpdate();
        otaManager.clearOtaFailures();
        // Note: clearConfirmInfo() is called AFTER sendConfirmation() succeeds
        // so that a network failure doesn't prevent retrying the confirmation.

        // Send success confirmation to server
        bool confirmSent = otaManager.sendConfirmation(configManager.getServerUrl(),
                                   configManager.getAuthToken(),
                                   WiFi.macAddress(),
                                   true,
                                   pendingOtaFirmwareFile,
                                   "");

        if (confirmSent) {
            otaManager.clearConfirmInfo();
            Serial.println("[OTA] Confirmation sent and NVS cleared");
            RemoteLogger::info("OTA", "Update validated and confirmed");
            otaValidationPending = false;
            pendingOtaFirmwareFile = "";
        } else {
            Serial.println("[OTA] WARNING: Confirmation send failed — will retry on next capture");
            // Leave otaValidationPending = true and pendingOtaFirmwareFile set so
            // the next successful capture calls validateOtaUpdate() again.
            // confirmUpdate() / esp_ota_mark_app_valid_cancel_rollback() is idempotent.
            RemoteLogger::warn("OTA", "Confirmation send failed, will retry");
        }
    } else {
        Serial.println("[OTA] Validation failed - rollback will occur on next reboot");
        RemoteLogger::error("OTA", "Update validation failed, rollback pending");
    }
}
