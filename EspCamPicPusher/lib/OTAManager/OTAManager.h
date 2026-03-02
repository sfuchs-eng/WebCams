#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <ArduinoJson.h>

// Firmware version - update this with each release
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.1.0"
#endif

// OTA States
enum OtaState {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_DOWNLOADING,
    OTA_WRITING,
    OTA_VALIDATING,
    OTA_REBOOTING,
    OTA_FAILED
};

// OTA Result
enum OtaResult {
    OTA_SUCCESS,
    OTA_NO_UPDATE,
    OTA_ERROR_DOWNLOAD,
    OTA_ERROR_WRITE,
    OTA_ERROR_VALIDATE,
    OTA_ERROR_CHECKSUM,
    OTA_ERROR_PARTITION,
    OTA_ERROR_MEMORY
};

// OTA Update Info
struct OtaUpdateInfo {
    bool available;
    String firmwareFile;
    String firmwareVersion;
    String downloadUrl;
    size_t size;
    String sha256;
    bool mandatory;
};

class OTAManager {
public:
    OTAManager();
    ~OTAManager();
    
    /**
     * Initialize OTA manager
     * Must be called after WiFi connection
     * @return true on success
     */
    bool begin();
    
    /**
     * Parse OTA info from upload JSON response
     * @param jsonResponse JSON response string from server
     * @return OtaUpdateInfo structure
     */
    OtaUpdateInfo parseOtaInfo(const String& jsonResponse);
    
    /**
     * Check if OTA update is available in response
     * @param jsonResponse JSON response from server
     * @return true if OTA available
     */
    bool isOtaAvailable(const String& jsonResponse);
    
    /**
     * Perform OTA update (download, flash, validate, reboot)
     * Blocking operation that takes 30-60 seconds
     * @param info OTA update information
     * @param authToken Authentication token for download
     * @param deviceId Device identifier
     * @param serverUrl Server base URL for constructing full download URL
     * @return OtaResult status code
     */
    OtaResult performUpdate(const OtaUpdateInfo& info, const String& authToken, 
                           const String& deviceId, const String& serverUrl);
    
    /**
     * Get current firmware version
     * @return Version string (e.g., "1.1.0")
     */
    String getFirmwareVersion();
    
    /**
     * Check if this is first boot after OTA
     * @return true if validation needed
     */
    bool isFirstBootAfterOta();
    
    /**
     * Validate current firmware and mark as valid
     * Call on successful first boot after OTA
     * @return true on success
     */
    bool confirmUpdate();
    
    /**
     * Send OTA confirmation to server
     * @param serverUrl Server base URL
     * @param authToken Auth token
     * @param deviceId Device ID
     * @param success true if OTA succeeded, false if failed/rollback
     * @param firmwareFile Firmware filename that was attempted
     * @param errorMessage Error description if failed
     * @return true if confirmation sent successfully
     */
    bool sendConfirmation(const String& serverUrl, const String& authToken, 
                         const String& deviceId, bool success, 
                         const String& firmwareFile, const String& errorMessage);
    
    /**
     * Get current OTA state
     */
    OtaState getState() const { return _state; }
    
    /**
     * Get last error message
     */
    String getLastError() const { return _lastError; }
    
    /**
     * Get download progress (0-100)
     */
    uint8_t getProgress() const { return _progress; }

private:
    OtaState _state;
    String _lastError;
    uint8_t _progress;
    String _firmwareVersion;
    
    // ESP-IDF OTA handles
    esp_ota_handle_t _otaHandle;
    const esp_partition_t* _updatePartition;
    
    /**
     * Build full URL from base URL and relative path
     * Base URL can be domain root or include path component
     * @param baseUrl Server base URL (e.g., "https://server.com" or "https://server.com/cams")
     * @param path Relative path (e.g., "/ota-download.php?file=...") or full URL
     * @return Full URL (e.g., "https://server.com/cams/ota-download.php?file=...")
     */
    String buildFullUrl(const String& baseUrl, const String& path);
    
    /**
     * Download firmware from URL
     * @return OtaResult status
     */
    OtaResult downloadFirmware(const String& url, const String& authToken, 
                               const String& deviceId, size_t expectedSize, 
                               const String& expectedSha256, const String& serverUrl);
    
    /**
     * Calculate SHA256 of data using mbedtls
     * @param data Data buffer
     * @param length Data length
     * @return SHA256 hex string
     */
    String calculateSha256(const uint8_t* data, size_t length);
    
    /**
     * Set error state
     */
    void setError(const String& error);
    
    /**
     * Load firmware version from compiled-in constant
     */
    void loadFirmwareVersion();
};

#endif // OTA_MANAGER_H
