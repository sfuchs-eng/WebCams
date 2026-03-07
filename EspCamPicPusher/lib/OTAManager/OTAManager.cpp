#include "OTAManager.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "mbedtls/sha256.h"

static const char* TAG = "OTAManager";

OTAManager::OTAManager() 
    : _state(OTA_IDLE), _progress(0), _otaHandle(0), _updatePartition(nullptr) {
    loadFirmwareVersion();
}

OTAManager::~OTAManager() {
    // Cleanup if needed
}

bool OTAManager::begin() {
    Serial.println("[OTA] Initializing OTA Manager");
    
    // Get current partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == nullptr) {
        setError("Failed to get running partition");
        return false;
    }
    
    Serial.printf("[OTA] Running partition: %s (offset 0x%x, size %d KB)\n", 
                  running->label, running->address, running->size / 1024);
    
    // Get update partition (opposite of running)
    _updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (_updatePartition == nullptr) {
        setError("Failed to get update partition");
        return false;
    }
    
    Serial.printf("[OTA] Update partition: %s (offset 0x%x, size %d KB)\n",
                  _updatePartition->label, _updatePartition->address, _updatePartition->size / 1024);
    
    return true;
}

void OTAManager::loadFirmwareVersion() {
    _firmwareVersion = FIRMWARE_VERSION;
}

String OTAManager::getFirmwareVersion() {
    return _firmwareVersion;
}

bool OTAManager::isFirstBootAfterOta() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        // If state is PENDING_VERIFY, this is first boot after OTA
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }
    
    return false;
}

bool OTAManager::confirmUpdate() {
    Serial.println("[OTA] Confirming update as valid");
    
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    
    if (err != ESP_OK) {
        Serial.printf("[OTA] Failed to mark app valid: %s\n", esp_err_to_name(err));
        return false;
    }
    
    Serial.println("[OTA] Update confirmed successfully");
    return true;
}

OtaUpdateInfo OTAManager::parseOtaInfo(const String& jsonResponse) {
    OtaUpdateInfo info;
    info.available = false;
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonResponse);
    
    if (error) {
        Serial.printf("[OTA] JSON parse error: %s\n", error.c_str());
        return info;
    }
    
    if (!doc.containsKey("ota")) {
        return info;
    }
    
    JsonObject ota = doc["ota"];
    info.available = ota["available"] | false;
    
    if (info.available) {
        info.firmwareFile = ota["firmware_file"] | "";
        info.firmwareVersion = ota["firmware_version"] | "";
        info.downloadUrl = ota["download_url"] | "";
        info.size = ota["size"] | 0;
        info.sha256 = ota["sha256"] | "";
        info.mandatory = ota["mandatory"] | false;
        
        Serial.println("\n[OTA] Update available!");
        Serial.printf("  Firmware: %s\n", info.firmwareFile.c_str());
        Serial.printf("  Version: %s\n", info.firmwareVersion.c_str());
        Serial.printf("  Size: %d bytes\n", info.size);
        Serial.printf("  SHA256: %s\n", info.sha256.c_str());
    }
    
    return info;
}

bool OTAManager::isOtaAvailable(const String& jsonResponse) {
    OtaUpdateInfo info = parseOtaInfo(jsonResponse);
    return info.available;
}

OtaResult OTAManager::performUpdate(const OtaUpdateInfo& info, const String& authToken, 
                                    const String& deviceId, const String& serverUrl) {
    Serial.println("\n======================================");
    Serial.println("[OTA] Starting OTA Update");
    Serial.println("======================================");
    
    _state = OTA_CHECKING;
    _progress = 0;
    
    // Validate update partition
    if (_updatePartition == nullptr) {
        setError("Update partition not available");
        return OTA_ERROR_PARTITION;
    }
    
    // Download and write firmware
    _state = OTA_DOWNLOADING;
    OtaResult result = downloadFirmware(info.downloadUrl, authToken, deviceId, 
                                        info.size, info.sha256, serverUrl);
    
    if (result != OTA_SUCCESS) {
        return result;
    }
    
    // Update completed successfully
    Serial.println("\n[OTA] Update completed successfully");
    Serial.println("[OTA] Rebooting in 3 seconds...");
    _state = OTA_REBOOTING;
    
    delay(3000);
    esp_restart();
    
    // Never reached
    return OTA_SUCCESS;
}

String OTAManager::buildFullUrl(const String& baseUrl, const String& path) {
    // If path is already a full URL (starts with http:// or https://), return as-is
    if (path.startsWith("http://") || path.startsWith("https://")) {
        return path;
    }
    
    // If path starts with '/', it's an absolute path from web root
    // Extract protocol and domain from baseUrl, then append path
    if (path.startsWith("/")) {
        // Find the position after protocol (after ://)
        int protocolEnd = baseUrl.indexOf("://");
        if (protocolEnd > 0) {
            // Find the first '/' after protocol (start of path)
            int pathStart = baseUrl.indexOf("/", protocolEnd + 3);
            if (pathStart > 0) {
                // Return protocol+domain+path
                return baseUrl.substring(0, pathStart) + path;
            } else {
                // No path in baseUrl, just append to domain
                return baseUrl + path;
            }
        }
        // Fallback: just append (shouldn't happen with valid URLs)
        return baseUrl + path;
    }
    
    // Otherwise, path is relative to baseUrl
    String url = baseUrl;
    
    // Ensure no double slashes for relative paths
    if (url.endsWith("/") && path.startsWith("/")) {
        return url.substring(0, url.length() - 1) + path;
    } else if (!url.endsWith("/") && !path.startsWith("/")) {
        return url + "/" + path;
    } else {
        return url + path;
    }
}

OtaResult OTAManager::downloadFirmware(const String& url, const String& authToken, 
                                       const String& deviceId, size_t expectedSize, 
                                       const String& expectedSha256, const String& serverUrl) {
    Serial.printf("[OTA] Download URL from server: %s\n", url.c_str());
    
    // Build full URL from server URL and download path
    String fullUrl = buildFullUrl(serverUrl, url);
    
    Serial.printf("[OTA] Downloading from: %s\n", fullUrl.c_str());
    Serial.printf("[OTA] Expected size: %d bytes\n", expectedSize);
    Serial.printf("[OTA] Expected SHA256: %s\n", expectedSha256.c_str());
    
    // Check WiFi before attempting download
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected");
        Serial.println("[OTA] ERROR: WiFi disconnected before download");
        return OTA_ERROR_DOWNLOAD;
    }
    
    WiFiClientSecure client;
    client.setInsecure();  // TODO: Add certificate validation in production
    
    HTTPClient http;
    Serial.println("[OTA] Initializing HTTP client...");
    
    if (!http.begin(client, fullUrl)) {
        setError("HTTP client begin() failed");
        Serial.println("[OTA] ERROR: http.begin() failed - invalid URL or client error");
        Serial.printf("[OTA] URL was: %s\n", fullUrl.c_str());
        return OTA_ERROR_DOWNLOAD;
    }
    
    Serial.println("[OTA] Adding headers...");
    http.addHeader("X-Auth-Token", authToken);
    http.addHeader("X-Device-ID", deviceId);
    http.setTimeout(30000); // 30 second timeout for connection + headers
    
    // Feed watchdog before long blocking HTTP call
    esp_task_wdt_reset();
    
    Serial.println("[OTA] Sending GET request...");
    int httpCode = http.GET();
    
    // Feed watchdog after HTTP call completes
    esp_task_wdt_reset();
    
    Serial.printf("[OTA] HTTP response code: %d\n", httpCode);
    
    // Detailed error reporting for HTTP -1
    if (httpCode < 0) {
        String errorDetail;
        switch (httpCode) {
            case -1:
                errorDetail = "Connection failed (check network, DNS, or server availability)";
                break;
            case -2:
                errorDetail = "Send header failed";
                break;
            case -3:
                errorDetail = "Send payload failed";
                break;
            case -4:
                errorDetail = "Not connected";
                break;
            case -5:
                errorDetail = "Connection lost";
                break;
            case -11:
                errorDetail = "Read timeout";
                break;
            default:
                errorDetail = "HTTP client error " + String(httpCode);
        }
        setError("Download failed: " + errorDetail);
        Serial.printf("[OTA] ERROR: %s\n", errorDetail.c_str());
        Serial.printf("[OTA] WiFi status: %d (3=connected)\n", WiFi.status());
        Serial.printf("[OTA] RSSI: %d dBm\n", WiFi.RSSI());
        http.end();
        return OTA_ERROR_DOWNLOAD;
    }
    
    if (httpCode != HTTP_CODE_OK) {
        setError("Download failed: HTTP " + String(httpCode));
        Serial.printf("[OTA] ERROR: Server returned HTTP %d\n", httpCode);
        String response = http.getString();
        if (response.length() > 0 && response.length() < 500) {
            Serial.printf("[OTA] Server response: %s\n", response.c_str());
        }
        http.end();
        return OTA_ERROR_DOWNLOAD;
    }
    
    size_t contentLength = http.getSize();
    if (contentLength == 0 || contentLength != expectedSize) {
        setError("Invalid content length: " + String(contentLength));
        http.end();
        return OTA_ERROR_DOWNLOAD;
    }
    
    Serial.printf("[OTA] Content length: %d bytes\n", contentLength);
    
    // Begin OTA update
    _state = OTA_WRITING;
    esp_err_t err = esp_ota_begin(_updatePartition, OTA_SIZE_UNKNOWN, &_otaHandle);
    if (err != ESP_OK) {
        setError("OTA begin failed: " + String(esp_err_to_name(err)));
        http.end();
        return OTA_ERROR_WRITE;
    }
    
    // Stream and write firmware data
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    
    // SHA256 context for checksum calculation
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);  // 0 = SHA256 (not SHA224)
    
    Serial.println("[OTA] Writing firmware...");
    
    unsigned long lastPrint = 0;
    unsigned long lastWdtReset = 0;
    
    while (written < contentLength && http.connected()) {
        // Feed watchdog every 100ms to prevent timeout
        if (millis() - lastWdtReset > 100) {
            esp_task_wdt_reset();
            lastWdtReset = millis();
        }
        
        size_t available = stream->available();
        if (available) {
            size_t readBytes = stream->readBytes(buffer, min(available, sizeof(buffer)));
            
            // Write to partition
            err = esp_ota_write(_otaHandle, buffer, readBytes);
            if (err != ESP_OK) {
                esp_ota_abort(_otaHandle);
                mbedtls_sha256_free(&sha256_ctx);
                setError("OTA write failed: " + String(esp_err_to_name(err)));
                http.end();
                return OTA_ERROR_WRITE;
            }
            
            // Update SHA256
            mbedtls_sha256_update(&sha256_ctx, buffer, readBytes);
            
            written += readBytes;
            _progress = (written * 100) / contentLength;
            
            // Print progress every 10% or every 2 seconds
            if (millis() - lastPrint > 2000 || _progress % 10 == 0) {
                Serial.printf("[OTA] Progress: %d%% (%d / %d bytes)\n", 
                             _progress, written, contentLength);
                lastPrint = millis();
            }
            
            // Yield to other tasks periodically
            if (written % (32 * 1024) == 0) {  // Every 32KB
                yield();
            }
        } else {
            delay(10);  // This also yields
        }
    }
    
    http.end();
    
    if (written != contentLength) {
        esp_ota_abort(_otaHandle);
        mbedtls_sha256_free(&sha256_ctx);
        setError("Incomplete download: " + String(written) + "/" + String(contentLength));
        return OTA_ERROR_DOWNLOAD;
    }
    
    Serial.println("[OTA] Download complete");
    
    // Finalize SHA256 calculation
    _state = OTA_VALIDATING;
    unsigned char sha256_result[32];
    mbedtls_sha256_finish(&sha256_ctx, sha256_result);
    mbedtls_sha256_free(&sha256_ctx);
    
    // Convert to hex string
    String calculatedSha256 = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", sha256_result[i]);
        calculatedSha256 += hex;
    }
    
    Serial.printf("[OTA] Calculated SHA256: %s\n", calculatedSha256.c_str());
    Serial.printf("[OTA] Expected SHA256:   %s\n", expectedSha256.c_str());
    
    // Validate checksum
    if (!expectedSha256.equalsIgnoreCase(calculatedSha256)) {
        esp_ota_abort(_otaHandle);
        setError("Checksum mismatch");
        return OTA_ERROR_CHECKSUM;
    }
    
    Serial.println("[OTA] Checksum verified");
    
    // Finalize OTA
    err = esp_ota_end(_otaHandle);
    if (err != ESP_OK) {
        setError("OTA end failed: " + String(esp_err_to_name(err)));
        return OTA_ERROR_WRITE;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(_updatePartition);
    if (err != ESP_OK) {
        setError("Failed to set boot partition: " + String(esp_err_to_name(err)));
        return OTA_ERROR_PARTITION;
    }
    
    Serial.printf("[OTA] Boot partition set to: %s\n", _updatePartition->label);
    
    _progress = 100;
    return OTA_SUCCESS;
}

bool OTAManager::sendConfirmation(const String& serverUrl, const String& authToken, 
                                  const String& deviceId, bool success, 
                                  const String& firmwareFile, const String& errorMessage) {
    Serial.println("[OTA] Sending confirmation to server");
    
    // Build confirmation URL from base URL
    String confirmUrl = buildFullUrl(serverUrl, "/ota-confirm.php");
    
    Serial.printf("[OTA] Confirmation URL: %s\n", confirmUrl.c_str());
    
    // Build JSON payload
    DynamicJsonDocument doc(512);
    doc["success"] = success;
    doc["firmware_file"] = firmwareFile;
    doc["firmware_version"] = _firmwareVersion;
    
    if (success) {
        doc["message"] = "OTA update successful";
        doc["boot_count"] = 1;
    } else {
        doc["error"] = errorMessage;
        doc["rollback"] = true;
    }
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    
    Serial.printf("[OTA] Confirmation payload: %s\n", jsonPayload.c_str());
    
    // Send POST request
    WiFiClientSecure client;
    client.setInsecure();
    
    HTTPClient http;
    http.begin(client, confirmUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Auth-Token", authToken);
    http.addHeader("X-Device-ID", deviceId);
    
    int httpCode = http.POST(jsonPayload);
    
    bool result = false;
    if (httpCode > 0) {
        Serial.printf("[OTA] Confirmation response: %d\n", httpCode);
        String response = http.getString();
        Serial.printf("[OTA] Server response: %s\n", response.c_str());
        result = (httpCode >= 200 && httpCode < 300);
    } else {
        Serial.printf("[OTA] Confirmation failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return result;
}

void OTAManager::setError(const String& error) {
    _state = OTA_FAILED;
    _lastError = error;
    Serial.printf("[OTA ERROR] %s\n", error.c_str());
}

String OTAManager::calculateSha256(const uint8_t* data, size_t length) {
    unsigned char hash[32];
    mbedtls_sha256(data, length, hash, 0);  // 0 = SHA256
    
    String result = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        result += hex;
    }
    
    return result;
}

// ============================================================================
// NVS Persistence for Reboot-to-OTA Mode
// ============================================================================

bool OTAManager::savePendingUpdate(const OtaUpdateInfo& info) {
    Serial.println("[OTA] Saving pending update to NVS");
    
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        Serial.println("[OTA] ERROR: Failed to open NVS namespace 'ota'");
        return false;
    }
    
    prefs.putBool("pending", true);
    prefs.putString("fwFile", info.firmwareFile);
    prefs.putString("fwVersion", info.firmwareVersion);
    prefs.putString("dlUrl", info.downloadUrl);
    prefs.putUInt("size", info.size);
    prefs.putString("sha256", info.sha256);
    prefs.putBool("mandatory", info.mandatory);
    
    prefs.end();
    
    Serial.printf("[OTA] Saved: %s v%s (%d bytes)\n", 
                  info.firmwareFile.c_str(), info.firmwareVersion.c_str(), info.size);
    return true;
}

OtaUpdateInfo OTAManager::loadPendingUpdate() {
    OtaUpdateInfo info;
    info.available = false;
    
    Preferences prefs;
    if (!prefs.begin("ota", true)) {  // read-only
        return info;
    }
    
    if (!prefs.getBool("pending", false)) {
        prefs.end();
        return info;
    }
    
    info.available = true;
    info.firmwareFile = prefs.getString("fwFile", "");
    info.firmwareVersion = prefs.getString("fwVersion", "");
    info.downloadUrl = prefs.getString("dlUrl", "");
    info.size = prefs.getUInt("size", 0);
    info.sha256 = prefs.getString("sha256", "");
    info.mandatory = prefs.getBool("mandatory", false);
    
    prefs.end();
    
    Serial.printf("[OTA] Loaded pending: %s v%s (%d bytes)\n",
                  info.firmwareFile.c_str(), info.firmwareVersion.c_str(), info.size);
    return info;
}

bool OTAManager::hasPendingUpdate() {
    Preferences prefs;
    if (!prefs.begin("ota", true)) {  // read-only
        return false;
    }
    bool pending = prefs.getBool("pending", false);
    prefs.end();
    return pending;
}

void OTAManager::clearPendingUpdate() {
    Serial.println("[OTA] Clearing pending update from NVS");
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        return;
    }
    // Only remove pending-related keys, preserve failure tracking
    prefs.remove("pending");
    prefs.remove("fwFile");
    prefs.remove("fwVersion");
    prefs.remove("dlUrl");
    prefs.remove("size");
    prefs.remove("sha256");
    prefs.remove("mandatory");
    prefs.end();
}

void OTAManager::recordOtaFailure(const String& firmwareFile) {
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        return;
    }
    
    String storedFile = prefs.getString("failFile", "");
    uint32_t count = 0;
    
    if (storedFile == firmwareFile) {
        // Same firmware — increment existing counter
        count = prefs.getUInt("failCount", 0) + 1;
    } else {
        // Different firmware — reset counter
        count = 1;
        prefs.putString("failFile", firmwareFile);
    }
    
    prefs.putUInt("failCount", count);
    prefs.end();
    
    Serial.printf("[OTA] Recorded failure #%u for %s\n", count, firmwareFile.c_str());
}

uint32_t OTAManager::getOtaFailureCount(const String& firmwareFile) {
    Preferences prefs;
    if (!prefs.begin("ota", true)) {  // read-only
        return 0;
    }
    
    String storedFile = prefs.getString("failFile", "");
    uint32_t count = 0;
    if (storedFile == firmwareFile) {
        count = prefs.getUInt("failCount", 0);
    }
    prefs.end();
    return count;
}

void OTAManager::clearOtaFailures() {
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        return;
    }
    prefs.remove("failFile");
    prefs.remove("failCount");
    prefs.end();
    Serial.println("[OTA] Cleared failure tracking");
}

void OTAManager::saveConfirmInfo(const String& firmwareFile) {
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        return;
    }
    prefs.putString("confFile", firmwareFile);
    prefs.end();
    Serial.printf("[OTA] Saved confirm info: %s\n", firmwareFile.c_str());
}

String OTAManager::loadConfirmFirmwareFile() {
    Preferences prefs;
    if (!prefs.begin("ota", true)) {  // read-only
        return "";
    }
    String file = prefs.getString("confFile", "");
    prefs.end();
    return file;
}

void OTAManager::clearConfirmInfo() {
    Preferences prefs;
    if (!prefs.begin("ota", false)) {
        return;
    }
    prefs.remove("confFile");
    prefs.end();
    Serial.println("[OTA] Cleared confirm info");
}
