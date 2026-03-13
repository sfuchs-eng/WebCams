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

    // Setup HTTP endpoints
    void setupRoutes();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTestConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleCapture(AsyncWebServerRequest* request);
    void handleCaptureResult(AsyncWebServerRequest* request);
    void handlePreview(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleAuthCheck(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // HTML UI generator
    String generateHtmlPage();
    
    // Utility
    void logRequest(AsyncWebServerRequest* request);
    bool checkAuthentication(AsyncWebServerRequest* request);
};

#endif // WEB_CONFIG_SERVER_H
