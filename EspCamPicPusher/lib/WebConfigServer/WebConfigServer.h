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
    
private:
    AsyncWebServer* server;
    ConfigManager* configManager;
    int serverPort;
    unsigned long lastActivityMillis;
    unsigned long timeoutMillis;
    bool cameraReady;
    CaptureCallback captureCallback;
    
    // Setup HTTP endpoints
    void setupRoutes();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleCapture(AsyncWebServerRequest* request);
    void handlePreview(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // HTML UI generator
    String generateHtmlPage();
    
    // Utility
    void logRequest(AsyncWebServerRequest* request);
};

#endif // WEB_CONFIG_SERVER_H
