#ifndef WEB_CONFIG_SERVER_H
#define WEB_CONFIG_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
#include "esp_camera.h"

// Callback function type for capture trigger
typedef bool (*CaptureCallback)();

class WebConfigServer {
public:
    WebConfigServer(ConfigManager* configMgr, int port = 80);
    ~WebConfigServer();
    
    /**
     * Initialize and start web server
     * @return True if successful
     */
    bool begin();
    
    /**
     * Stop web server
     */
    void stop();
    
    /**
     * Check if activity timeout has expired
     * @return True if timeout expired
     */
    bool isTimeoutExpired();
    
    /**
     * Reset activity timer
     */
    void resetActivityTimer();
    
    /**
     * Get remaining timeout seconds
     * @return Seconds remaining until timeout
     */
    int getRemainingSeconds();
    
    /**
     * Get server IP address
     * @return IP address as string
     */
    String getIpAddress();
    
    /**
     * Set camera pointer for capture operations
     * @param initialized True if camera is initialized
     */
    void setCameraReady(bool initialized);
    
    /**
     * Set callback function for manual capture trigger
     * @param callback Function to call when capture is requested
     */
    void setCaptureCallback(CaptureCallback callback);
    
    /**
     * Set AP mode status
     * @param apMode True if in AP+STA mode
     */
    void setApMode(bool apMode);

    // --- Decoupled-capture helpers (called from the main loop) ---

    /** Returns true if the web UI has queued a capture-and-push operation. */
    bool isCaptureRequested() { return captureRequested; }

    /** Clear the request flag and mark result as pending. Call before starting work. */
    void ackCaptureRequest() { captureRequested = false; captureResult = 0; }

    /** Store the result so /capture/result can return it to the browser. */
    void setCaptureResult(bool success) { captureResult = success ? 1 : 2; }

    // --- Decoupled WiFi-test helpers (called from the main loop) ---
    // State: -1=idle, 0=pending (main loop should WiFi.begin), 1=in_progress, 2=success, 3=failed

    /** Returns true when the main loop should call WiFi.begin() for a new test. */
    bool isWifiTestPending()    const { return wifiTestState == 0; }
    /** Returns true while the main loop polls WiFi.status() for the test result. */
    bool isWifiTestInProgress() const { return wifiTestState == 1; }
    /** SSID to test. Valid when isWifiTestPending() or isWifiTestInProgress(). */
    String getWifiTestSsid()    const { return wifiTestSsid; }
    /** Password to test. Valid when isWifiTestPending() or isWifiTestInProgress(). */
    String getWifiTestPassword()const { return wifiTestPassword; }
    /** Transition PENDING → IN_PROGRESS; call immediately after WiFi.begin(). */
    void ackWifiTest() { wifiTestState = 1; }
    /** Store the final result so /config/test/result can return it to the browser. */
    void setWifiTestResult(bool success, const String& ip = "", int rssi = 0);
    
private:
    AsyncWebServer* server;
    ConfigManager* configManager;
    int serverPort;
    unsigned long lastActivityMillis;
    unsigned long timeoutMillis;
    bool cameraReady;
    CaptureCallback captureCallback;
    bool isApMode;

    // Decoupled capture-request state.
    // Written by the async-web-server task (Core 0), read by the main loop (Core 1).
    // -1 = idle, 0 = pending (queued, not yet done), 1 = success, 2 = failed.
    volatile bool captureRequested;
    volatile int  captureResult;

    // Decoupled WiFi-test state (same cross-core pattern as capture above).
    // Written by async handler when queueing; driven by main loop to completion.
    volatile int  wifiTestState;
    String        wifiTestSsid;
    String        wifiTestPassword;
    String        wifiTestResultIp;
    int           wifiTestResultRssi;

    // Setup HTTP endpoints
    void setupRoutes();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTestConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleWifiTestResult(AsyncWebServerRequest* request);
    void handleCapture(AsyncWebServerRequest* request);
    void handleCaptureResult(AsyncWebServerRequest* request);
    void handlePreview(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleAuthCheck(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // Utility
    void logRequest(AsyncWebServerRequest* request);
    bool checkAuthentication(AsyncWebServerRequest* request);
};

#endif // WEB_CONFIG_SERVER_H
