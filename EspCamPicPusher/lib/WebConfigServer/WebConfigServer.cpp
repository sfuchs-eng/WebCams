#include "WebConfigServer.h"
#include "CameraMutex.h"
#include "CameraCapture.h"
#include "ScheduleManager.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <base64.h>

WebConfigServer::WebConfigServer(ConfigManager* configMgr, int port) {
    configManager = configMgr;
    serverPort = port;
    server = nullptr;
    lastActivityMillis = 0;
    captureCallback = nullptr;
    timeoutMillis = 0;
    cameraReady = false;
    isApMode = false;
}

WebConfigServer::~WebConfigServer() {
    stop();
}

bool WebConfigServer::begin() {
    if (!configManager) {
        Serial.println("Error: ConfigManager not set");
        return false;
    }
    
    // Calculate timeout in milliseconds
    timeoutMillis = configManager->getWebTimeoutMin() * 60UL * 1000UL;
    
    // Initialize server
    server = new AsyncWebServer(serverPort);
    
    // Setup routes
    setupRoutes();
    
    // Start server
    server->begin();
    
    // Reset activity timer
    resetActivityTimer();
    
    Serial.printf("\n=== Web Configuration Server Started ===\n");
    Serial.printf("URL: http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.printf("Timeout: %d minutes\n", configManager->getWebTimeoutMin());
    Serial.println("========================================\n");
    
    return true;
}

void WebConfigServer::stop() {
    if (server) {
        server->end();
        delete server;
        server = nullptr;
        Serial.println("Web server stopped");
    }
}

void WebConfigServer::setupRoutes() {
    // Main configuration page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleRoot(request);
    });
    
    // Get current configuration as JSON
    server->on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleGetConfig(request);
    });
    
    // Save configuration (JSON POST)
    server->on("/config", HTTP_POST, 
        [this](AsyncWebServerRequest* request) {
            // This gets called after body handler
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            this->logRequest(request);
            this->resetActivityTimer();
            this->handlePostConfig(request, data, len);
        }
    );
    
    // Test WiFi configuration (JSON POST)
    server->on("/config/test", HTTP_POST, 
        [this](AsyncWebServerRequest* request) {
            // This gets called after body handler
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            this->logRequest(request);
            this->resetActivityTimer();
            this->handleTestConfig(request, data, len);
        }
    );
    
    // Trigger image capture and upload
    server->on("/capture", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleCapture(request);
    });
    
    // Get image preview
    server->on("/preview", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handlePreview(request);
    });
    
    // Device status
    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleStatus(request);
    });
    
    // Authentication check
    server->on("/auth-check", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleAuthCheck(request);
    });
    
    // Reset to factory defaults
    server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->logRequest(request);
        this->resetActivityTimer();
        this->handleReset(request);
    });
    
    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest* request) {
        this->handleNotFound(request);
    });
}

void WebConfigServer::handleRoot(AsyncWebServerRequest* request) {
    String html = generateHtmlPage();
    request->send(200, "text/html", html);
}

void WebConfigServer::handleGetConfig(AsyncWebServerRequest* request) {
    String json = configManager->toJson();
    request->send(200, "application/json", json);
}

void WebConfigServer::handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    // Check authentication if password is set
    if (!checkAuthentication(request)) {
        AsyncWebServerResponse* response = request->beginResponse(401, "application/json", "{\"success\":false,\"message\":\"Authentication required\"}");
        response->addHeader("WWW-Authenticate", "Basic realm=\"EspCamPicPusher\"");
        request->send(response);
        return;
    }
    
    // Parse JSON from request body
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.println("Received config update:");
    Serial.println(body);
    
    // Parse JSON to check for WiFi credential changes
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, body);
    
    bool wifiChanged = false;
    String newSsid = "";
    String newPassword = "";
    
    if (!error && doc.containsKey("wifiSsid")) {
        newSsid = doc["wifiSsid"].as<String>();
        // Check if WiFi SSID changed
        if (newSsid != String(configManager->getWifiSsid())) {
            wifiChanged = true;
        }
        
        // Check password (only if not the placeholder)
        if (doc.containsKey("wifiPassword")) {
            newPassword = doc["wifiPassword"].as<String>();
            if (newPassword != "********") {
                wifiChanged = true;
            }
        }
    }
    
    // If in AP mode and WiFi credentials changed, test them first
    if (isApMode && wifiChanged) {
        Serial.println("WiFi credentials changed, testing connection...");
        
        // Test the connection
        WiFi.begin(newSsid.c_str(), newPassword == "********" ? configManager->getWifiPassword() : newPassword.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi test successful!");
            Serial.printf("Connected to %s\n", newSsid.c_str());
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            
            // Connection successful, save config and reboot
            if (configManager->loadFromJson(body.c_str())) {
                if (configManager->save()) {
                    // Send success response with reboot flag
                    request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi connected! Rebooting in 3 seconds...\",\"rebooting\":true}");
                    
                    // Reboot after a delay
                    delay(3000);
                    ESP.restart();
                    return;
                } else {
                    request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
                    return;
                }
            } else {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid configuration\"}");
                return;
            }
        } else {
            // Connection failed
            Serial.println("WiFi test failed!");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"WiFi connection test failed. Please check credentials.\"}");
            
            // Try to reconnect to original WiFi if in AP+STA mode
            String originalSsid = String(configManager->getWifiSsid());
            if (originalSsid.length() > 0) {
                Serial.printf("Attempting to reconnect to original WiFi: %s\n", originalSsid.c_str());
                WiFi.begin(originalSsid.c_str(), configManager->getWifiPassword());
            }
            return;
        }
    }
    
    // Normal config save (no WiFi change or not in AP mode)
    if (configManager->loadFromJson(body.c_str())) {
        // Save to NVS
        if (configManager->save()) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
        }
    } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid configuration\"}");
    }
}

void WebConfigServer::handleTestConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    // Check authentication if password is set
    if (!checkAuthentication(request)) {
        AsyncWebServerResponse* response = request->beginResponse(401, "application/json", "{\"success\":false,\"message\":\"Authentication required\"}");
        response->addHeader("WWW-Authenticate", "Basic realm=\"EspCamPicPusher\"");
        request->send(response);
        return;
    }
    
    // Parse JSON from request body
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    Serial.println("Testing WiFi configuration:");
    Serial.println(body);
    
    // Parse JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"connected\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Extract WiFi credentials
    if (!doc.containsKey("wifiSsid")) {
        request->send(400, "application/json", "{\"success\":false,\"connected\":false,\"message\":\"WiFi SSID required\"}");
        return;
    }
    
    String ssid = doc["wifiSsid"].as<String>();
    String password = "";
    bool passwordChanged = false;
    
    if (doc.containsKey("wifiPassword")) {
        password = doc["wifiPassword"].as<String>();
        // If password is placeholder, use current password
        if (password == "********") {
            password = String(configManager->getWifiPassword());
        } else {
            passwordChanged = true;
        }
    }
    
    // Check if credentials are same as current configuration
    String currentSsid = String(configManager->getWifiSsid());
    String currentPassword = String(configManager->getWifiPassword());
    bool credentialsUnchanged = (ssid == currentSsid) && 
                                 (!passwordChanged || password == currentPassword);
    
    // If already in STA mode (not AP mode)
    if (!isApMode) {
        if (credentialsUnchanged) {
            // Same credentials, check if currently connected
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi credentials unchanged and already connected");
                String response = "{\"success\":true,\"connected\":true,\"ip\":\"" + 
                                 WiFi.localIP().toString() + 
                                 "\",\"rssi\":" + String(WiFi.RSSI()) + 
                                 ",\"message\":\"Already connected with this WiFi configuration\",\"unchanged\":true}";
                request->send(200, "application/json", response);
                return;
            }
        }
        
        // Different credentials in STA mode - save and tell user to reboot
        Serial.println("WiFi credentials changed in STA mode, saving configuration");
        
        // Update config with new credentials
        if (configManager->loadFromJson(body.c_str())) {
            if (configManager->save()) {
                Serial.println("Configuration saved, reboot required");
                request->send(200, "application/json", 
                    "{\"success\":true,\"connected\":false,\"message\":\"Configuration saved. Please reboot to apply WiFi changes.\",\"needsReboot\":true}");
                return;
            } else {
                request->send(500, "application/json", 
                    "{\"success\":false,\"connected\":false,\"message\":\"Failed to save configuration\"}");
                return;
            }
        } else {
            request->send(400, "application/json", 
                "{\"success\":false,\"connected\":false,\"message\":\"Invalid configuration\"}");
            return;
        }
    }
    
    // In AP mode - test the connection live
    Serial.printf("Testing connection to: %s (AP mode)\n", ssid.c_str());
    
    // Test WiFi connection
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        // Success!
        Serial.println("Test connection successful!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        
        // Build JSON response with connection details
        String response = "{\"success\":true,\"connected\":true,\"ip\":\"" + 
                         WiFi.localIP().toString() + 
                         "\",\"rssi\":" + String(WiFi.RSSI()) + 
                         ",\"message\":\"Connected successfully!\"}";
        
        request->send(200, "application/json", response);
    } else {
        // Failed
        Serial.println("Test connection failed");
        
        // Try to reconnect to original WiFi if in AP+STA mode
        String originalSsid = String(configManager->getWifiSsid());
        if (originalSsid.length() > 0) {
            Serial.printf("Reconnecting to original WiFi: %s\n", originalSsid.c_str());
            WiFi.begin(originalSsid.c_str(), configManager->getWifiPassword());
        }
        
        request->send(400, "application/json", "{\"success\":false,\"connected\":false,\"message\":\"Connection failed. Check SSID and password.\"}");
    }
}

void WebConfigServer::handleCapture(AsyncWebServerRequest* request) {
    if (!cameraReady) {
        request->send(503, "application/json", "{\"success\":false,\"message\":\"Camera not ready\"}");
        return;
    }
    
    if (!captureCallback) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Capture callback not set\"}");
        return;
    }
    
    // Trigger the capture and upload via callback
    bool success = captureCallback();
    
    if (success) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Image captured and uploaded successfully\"}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Capture or upload failed\"}");
    }
}

void WebConfigServer::handlePreview(AsyncWebServerRequest* request) {
    if (!cameraReady) {
        request->send(503, "text/plain", "Camera not ready");
        return;
    }
    
    // Acquire camera mutex to prevent concurrent access
    if (!CameraMutex::lock(5000)) {
        request->send(503, "text/plain", "Camera busy, try again");
        return;
    }
    
    // Capture preview image with sensor warm-up for proper auto-adjustment
    camera_fb_t* fb = CameraCapture::captureFrame(true);
    if (!fb) {
        CameraMutex::unlock();
        request->send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    // Copy JPEG data to heap buffer (async response needs it after we return the frame buffer)
    uint8_t* jpegCopy = (uint8_t*)malloc(fb->len);
    if (!jpegCopy) {
        esp_camera_fb_return(fb);
        CameraMutex::unlock();
        request->send(500, "text/plain", "Out of memory");
        return;
    }
    
    memcpy(jpegCopy, fb->buf, fb->len);
    size_t jpegLen = fb->len;
    
    // Return frame buffer immediately (we have a copy)
    CameraCapture::releaseFrame(fb);
    CameraMutex::unlock();
    
    // Send image as JPEG using the copied data
    // The buffer will be freed by the chunked response callback after sending
    AsyncWebServerResponse* response = request->beginChunkedResponse("image/jpeg", 
        [jpegCopy, jpegLen](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            // Calculate how many bytes to send in this chunk
            size_t remaining = jpegLen - index;
            if (remaining == 0) {
                // All data sent, free the buffer
                free((void*)jpegCopy);
                return 0; // Signal end of data
            }
            
            size_t toSend = (remaining < maxLen) ? remaining : maxLen;
            memcpy(buffer, jpegCopy + index, toSend);
            return toSend;
        }
    );
    
    request->send(response);
}

void WebConfigServer::handleStatus(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    
    doc["macAddress"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    doc["remainingTimeout"] = getRemainingSeconds();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["cameraReady"] = cameraReady;
    
    // AP mode information
    doc["apMode"] = isApMode;
    if (isApMode) {
        doc["apSsid"] = WiFi.softAPSSID();
        doc["apIp"] = WiFi.softAPIP().toString();
    }
    
    // STA mode information
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    doc["staConnected"] = staConnected;
    if (staConnected) {
        doc["staIp"] = WiFi.localIP().toString();
        doc["staSsid"] = WiFi.SSID();
    } else {
        // Use IP as fallback for non-STA mode
        doc["ipAddress"] = WiFi.localIP().toString();
    }
    
    // Add current local time
    struct tm timeinfo;
    if (ScheduleManager::getCurrentTime(&timeinfo)) {
        doc["localTime"] = ScheduleManager::formatTime(&timeinfo);
    } else {
        doc["localTime"] = "Time not synced";
    }
    
    String output;
    serializeJson(doc, output);
    
    request->send(200, "application/json", output);
}

void WebConfigServer::handleAuthCheck(AsyncWebServerRequest* request) {
    // Check if authentication is required
    if (strlen(configManager->getWebPassword()) == 0) {
        // No password set, no auth required
        request->send(200, "application/json", "{\"authenticated\":true,\"required\":false}");
        return;
    }
    
    // Check authentication
    if (checkAuthentication(request)) {
        request->send(200, "application/json", "{\"authenticated\":true,\"required\":true}");
    } else {
        // Send 401 with WWW-Authenticate header to trigger browser login prompt
        AsyncWebServerResponse* response = request->beginResponse(401, "application/json", "{\"authenticated\":false,\"required\":true}");
        response->addHeader("WWW-Authenticate", "Basic realm=\"EspCamPicPusher\"");
        request->send(response);
    }
}

void WebConfigServer::handleReset(AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Resetting to factory defaults...\"}");
    
    // Reset configuration
    configManager->reset();
    
    // Restart ESP
    delay(1000);
    ESP.restart();
}

void WebConfigServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
}

String WebConfigServer::generateHtmlPage() {
    String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EspCamPicPusher Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: #333;
            color: white;
            padding: 20px;
            text-align: center;
        }
        .header h1 { font-size: 24px; margin-bottom: 5px; }
        .header p { font-size: 14px; opacity: 0.8; }
        .content { padding: 30px; }
        .section {
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 1px solid #eee;
        }
        .section:last-child { border-bottom: none; }
        .section h2 {
            font-size: 18px;
            margin-bottom: 15px;
            color: #333;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #555;
            font-size: 14px;
        }
        input[type="text"],
        input[type="password"],
        input[type="number"] {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        .schedule-item {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
            align-items: center;
        }
        .schedule-item input {
            width: 80px;
        }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
            transition: all 0.3s;
        }
        .btn-primary {
            background: #667eea;
            color: white;
        }
        .btn-primary:hover {
            background: #5568d3;
        }
        .btn-secondary {
            background: #6c757d;
            color: white;
        }
        .btn-secondary:hover {
            background: #5a6268;
        }
        .btn-danger {
            background: #dc3545;
            color: white;
        }
        .btn-danger:hover {
            background: #c82333;
        }
        .btn-success {
            background: #28a745;
            color: white;
        }
        .btn-success:hover {
            background: #218838;
        }
        .btn-small {
            padding: 5px 10px;
            font-size: 12px;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 20px;
        }
        .status-info {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 15px;
        }
        .status-info p {
            margin: 5px 0;
            font-size: 14px;
        }
        .message {
            position: fixed;
            bottom: 0;
            right: 0;
            padding: 15px 20px;
            border-radius: 0;
            display: none;
            z-index: 9999;
            min-width: 300px;
            max-width: 500px;
            box-shadow: -2px -2px 10px rgba(0,0,0,0.2);
            animation: slideIn 0.3s ease-out;
        }
        @keyframes slideIn {
            from {
                transform: translateX(100%);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
        .message.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
            border-right: none;
            border-bottom: none;
        }
        .message.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
            border-right: none;
            border-bottom: none;
        }
        .preview-container {
            margin-top: 15px;
            text-align: center;
        }
        .preview-container img {
            max-width: 100%;
            border-radius: 5px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.2);
        }
        #countdown {
            position: fixed;
            top: 20px;
            right: 20px;
            background: rgba(0,0,0,0.8);
            color: white;
            padding: 10px 20px;
            border-radius: 5px;
            font-size: 14px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div id="countdown">Loading...</div>
    <div class="container">
        <div class="header">
            <h1>📷 EspCamPicPusher</h1>
            <p>Web Configuration Interface</p>
        </div>
        <div class="content">
            <div id="message" class="message"></div>
            
            <!-- Status Section -->
            <div class="section">
                <h2>📊 Device Status</h2>
                <div class="status-info" id="statusInfo">
                    <p><strong>Local Time:</strong> <span id="localTime">-</span></p>
                    <p><strong>IP:</strong> <span id="ipAddress">-</span></p>
                    <p><strong>MAC:</strong> <span id="macAddress">-</span></p>
                    <p><strong>RSSI:</strong> <span id="rssi">-</span> dBm</p>
                    <p><strong>Free Heap:</strong> <span id="freeHeap">-</span> bytes</p>
                </div>
            </div>

            <!-- WiFi Configuration -->
            <div class="section">
                <h2>📡 WiFi Configuration</h2>
                <p style="font-size: 12px; color: #666; margin-bottom: 15px;">
                    💡 Tip: If WiFi connection fails during boot, device will create ESP32-CAM-XXXX access point at 192.168.4.1
                </p>
                <div class="form-group">
                    <label>SSID:</label>
                    <input type="text" id="wifiSsid" placeholder="WiFi Network Name">
                </div>
                <div class="form-group">
                    <label>Password:</label>
                    <input type="password" id="wifiPassword" placeholder="Leave blank to keep current">
                </div>
                <div id="wifiTestResult" class="message" style="display:none;"></div>
                <button class="btn btn-secondary btn-small" onclick="testWiFiConfig()">🔍 Test WiFi Connection</button>
            </div>

            <!-- Server Configuration -->
            <div class="section">
                <h2>🌐 Server Configuration</h2>
                <div class="form-group">
                    <label>Server URL:</label>
                    <input type="text" id="serverUrl" placeholder="https://example.com/upload.php">
                </div>
                <div class="form-group">
                    <label>Auth Token:</label>
                    <input type="password" id="authToken" placeholder="Leave blank to keep current">
                </div>
            </div>

            <!-- Schedule Configuration -->
            <div class="section">
                <h2>⏰ Capture Schedule</h2>
                <div id="scheduleContainer"></div>
                <button class="btn btn-secondary btn-small" onclick="addScheduleItem()">+ Add Time</button>
            </div>

            <!-- Timezone Configuration -->
            <div class="section">
                <h2>🌍 Timezone</h2>
                <div class="form-group">
                    <label>GMT Offset (seconds):</label>
                    <input type="number" id="gmtOffsetSec" placeholder="3600">
                </div>
                <div class="form-group">
                    <label>Daylight Offset (seconds):</label>
                    <input type="number" id="daylightOffsetSec" placeholder="3600">
                </div>
            </div>

            <!-- Power Management -->
            <div class="section">
                <h2>⚡ Power Management</h2>
                <div class="form-group">
                    <label>Web Timeout (minutes):</label>
                    <input type="number" id="webTimeoutMin" placeholder="15" min="1" max="240">
                </div>
                <div class="form-group">
                    <label>Sleep Margin (seconds):</label>
                    <input type="number" id="sleepMarginSec" placeholder="60" min="0" max="600">
                </div>
            </div>

            <!-- Web Authentication -->
            <div class="section">
                <h2>🔒 Web Authentication</h2>
                <p style="font-size: 12px; color: #666; margin-bottom: 10px;">Leave password empty to disable authentication</p>
                <div class="form-group">
                    <label>Username:</label>
                    <input type="text" id="webUsername" placeholder="admin">
                </div>
                <div class="form-group">
                    <label>Password:</label>
                    <input type="password" id="webPassword" placeholder="Leave blank to keep current or disable auth">
                </div>
            </div>

            <!-- Manual Capture -->
            <div class="section">
                <h2>📸 Manual Capture</h2>
                <button class="btn btn-success" onclick="capturePreview()">📷 Capture & Preview</button>
                <button class="btn btn-primary" onclick="captureAndPush()" style="margin-left: 10px;">📤 Capture & Push to Server</button>
                <div class="preview-container" id="previewContainer"></div>
            </div>

            <!-- Action Buttons -->
            <div class="button-group">
                <button class="btn btn-primary" id="saveBtn" onclick="saveConfig()">💾 Save Configuration</button>
                <button class="btn btn-secondary" onclick="loadConfig()">🔄 Reload</button>
                <button class="btn btn-danger" onclick="resetConfig()">⚠️ Factory Reset</button>
            </div>
            <div id="authWarning" class="message error" style="display:none; margin-top: 10px;">
                Authentication required. Please log in to save configuration.
            </div>
        </div>
    </div>

    <script>
        let schedule = [];
        let isAuthenticated = false;
        let authRequired = false;

        function showMessage(text, isError = false) {
            const msg = document.getElementById('message');
            msg.textContent = text;
            msg.className = 'message ' + (isError ? 'error' : 'success');
            msg.style.display = 'block';
            setTimeout(() => { msg.style.display = 'none'; }, 5000);
        }
        
        function showWiFiTestResult(text, isError = false) {
            const msg = document.getElementById('wifiTestResult');
            msg.textContent = text;
            msg.className = 'message ' + (isError ? 'error' : 'success');
            msg.style.display = 'block';
        }

        function updateCountdown() {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    const remaining = data.remainingTimeout;
                    const minutes = Math.floor(remaining / 60);
                    const seconds = remaining % 60;
                    document.getElementById('countdown').textContent = 
                        `⏱️ ${minutes}:${seconds.toString().padStart(2, '0')}`;
                    
                    document.getElementById('localTime').textContent = data.localTime || '-';
                    document.getElementById('macAddress').textContent = data.macAddress;
                    document.getElementById('rssi').textContent = data.rssi;
                    document.getElementById('freeHeap').textContent = data.freeHeap.toLocaleString();
                    
                    // Show AP mode status if active
                    if (data.apMode) {
                        let ipText = 'AP: ' + data.apIp;
                        if (data.staConnected) {
                            ipText += ' | STA: ' + data.staIp;
                        } else {
                            ipText += ' (AP Mode: No STA connection)';
                        }
                        document.getElementById('ipAddress').textContent = ipText;
                    } else if (data.staConnected) {
                        document.getElementById('ipAddress').textContent = data.staIp;
                    } else {
                        document.getElementById('ipAddress').textContent = data.ipAddress || '-';
                    }
                });
        }

        function loadConfig() {
            fetch('/config')
                .then(r => r.json())
                .then(config => {
                    document.getElementById('wifiSsid').value = config.wifiSsid || '';
                    // Don't populate password fields - keep them empty for security
                    document.getElementById('wifiPassword').placeholder = 'Current: ' + (config.wifiPassword === '********' ? 'Configured' : 'Not set');
                    document.getElementById('serverUrl').value = config.serverUrl || '';
                    document.getElementById('authToken').placeholder = 'Current: ' + (config.authToken === '********' ? 'Configured' : 'Not set');
                    document.getElementById('gmtOffsetSec').value = config.gmtOffsetSec || 3600;
                    document.getElementById('daylightOffsetSec').value = config.daylightOffsetSec || 3600;
                    document.getElementById('webTimeoutMin').value = config.webTimeoutMin || 15;
                    document.getElementById('sleepMarginSec').value = config.sleepMarginSec || 60;
                    
                    // Load web authentication
                    document.getElementById('webUsername').value = config.webUsername || '';
                    document.getElementById('webPassword').placeholder = 'Current: ' + (config.webPassword === '********' ? 'Set' : 'Not set');
                    
                    // Check if auth is required
                    authRequired = config.webPassword === '********';
                    checkAuthStatus();
                    
                    schedule = config.schedule || [];
                    renderSchedule();
                    
                    showMessage('Configuration loaded');
                })
                .catch(err => showMessage('Failed to load config: ' + err, true));
        }
        
        function checkAuthStatus() {
            // Check if we can access the config endpoint (which doesn't require auth for GET)
            // If auth is required, hide the save button and show warning
            const saveBtn = document.getElementById('saveBtn');
            const authWarning = document.getElementById('authWarning');
            
            if (authRequired) {
                // Make request to auth-check endpoint which will trigger browser login if not authenticated
                fetch('/auth-check', {
                    credentials: 'include'  // Include credentials if cached
                })
                    .then(r => {
                        if (r.ok) {
                            // Successfully authenticated
                            isAuthenticated = true;
                            saveBtn.style.display = '';
                            authWarning.style.display = 'none';
                        } else if (r.status === 401) {
                            // Not authenticated - browser should have shown prompt
                            // User either cancelled or entered wrong credentials
                            isAuthenticated = false;
                            saveBtn.style.display = 'none';
                            authWarning.style.display = 'block';
                            authWarning.innerHTML = 'Authentication required. <button class="btn btn-small btn-primary" onclick="triggerLogin()" style="margin-left: 10px;">Login</button>';
                        }
                    })
                    .catch(() => {
                        isAuthenticated = false;
                        saveBtn.style.display = 'none';
                        authWarning.style.display = 'block';
                    });
            } else {
                // No auth required
                isAuthenticated = true;
                saveBtn.style.display = '';
                authWarning.style.display = 'none';
            }
        }
        
        function triggerLogin() {
            // Make a request to auth-check which will trigger the browser's login prompt
            fetch('/auth-check', {
                credentials: 'include'
            })
                .then(r => {
                    if (r.ok) {
                        // Authentication successful
                        showMessage('\u2713 Authenticated successfully!');
                        checkAuthStatus();
                    } else {
                        // User cancelled or wrong credentials
                        showMessage('\u274c Authentication failed or cancelled', true);
                    }
                })
                .catch(err => {
                    showMessage('\u274c Authentication error: ' + err, true);
                });
        }

        function testWiFiConfig() {
            const ssid = document.getElementById('wifiSsid').value;
            const password = document.getElementById('wifiPassword').value;
            
            if (!ssid) {
                showWiFiTestResult('❌ Please enter WiFi SSID', true);
                return;
            }
            
            showWiFiTestResult('🔄 Testing connection to ' + ssid + '...', false);
            
            const testData = {
                wifiSsid: ssid,
                wifiPassword: password || '********'
            };
            
            fetch('/config/test', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'include',
                body: JSON.stringify(testData)
            })
            .then(r => {
                if (r.status === 401) {
                    throw new Error('Authentication required');
                }
                return r.json();
            })
            .then(data => {
                if (data.connected) {
                    showWiFiTestResult('✅ Connected! IP: ' + data.ip + ' | Signal: ' + data.rssi + ' dBm', false);
                } else {
                    showWiFiTestResult('❌ Connection failed: ' + data.message, true);
                }
            })
            .catch(err => {
                showWiFiTestResult('❌ Test error: ' + err, true);
            });
        }

        function saveConfig() {
            const config = {
                wifiSsid: document.getElementById('wifiSsid').value,
                wifiPassword: document.getElementById('wifiPassword').value,
                serverUrl: document.getElementById('serverUrl').value,
                authToken: document.getElementById('authToken').value,
                gmtOffsetSec: parseInt(document.getElementById('gmtOffsetSec').value),
                daylightOffsetSec: parseInt(document.getElementById('daylightOffsetSec').value),
                schedule: schedule,
                webTimeoutMin: parseInt(document.getElementById('webTimeoutMin').value),
                sleepMarginSec: parseInt(document.getElementById('sleepMarginSec').value),
                webUsername: document.getElementById('webUsername').value,
                webPassword: document.getElementById('webPassword').value
            };

            fetch('/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'include',
                body: JSON.stringify(config)
            })
            .then(r => {
                if (r.status === 401) {
                    throw new Error('Authentication required. Please refresh and log in.');
                }
                if (!r.ok) {
                    throw new Error('HTTP ' + r.status);
                }
                return r.json();
            })
            .then(data => {
                if (data.success) {
                    // Check if device is rebooting
                    if (data.rebooting) {
                        showMessage('✓ WiFi connected! Device rebooting in 3 seconds...');
                        // Show countdown modal
                        let countdown = 30;
                        const countdownInterval = setInterval(() => {
                            countdown--;
                            showMessage(`✓ Device rebooting... Reconnecting in ${countdown}s`);
                            if (countdown <= 0) {
                                clearInterval(countdownInterval);
                                // Attempt to redirect to current URL (will work if on new WiFi)
                                window.location.reload();
                            }
                        }, 1000);
                    } else {
                        showMessage('✓ Configuration saved successfully!');
                        // Clear password fields after successful save
                        document.getElementById('wifiPassword').value = '';
                        document.getElementById('authToken').value = '';
                        document.getElementById('webPassword').value = '';
                        
                        // Reload config to update auth status
                        setTimeout(() => loadConfig(), 1000);
                    }
                } else {
                    showMessage('❌ Failed: ' + data.message, true);
                }
            })
            .catch(err => showMessage('❌ Save error: ' + err, true));
        }

        function resetConfig() {
            if (!confirm('Reset to factory defaults? Device will restart.')) return;
            
            fetch('/reset', { method: 'POST' })
                .then(() => showMessage('Resetting... Device will restart'))
                .catch(err => showMessage('Reset error: ' + err, true));
        }

        function renderSchedule() {
            const container = document.getElementById('scheduleContainer');
            container.innerHTML = '';
            
            // Sort schedule by time of day (hour, then minute)
            schedule.sort((a, b) => {
                if (a.hour !== b.hour) return a.hour - b.hour;
                return a.minute - b.minute;
            });
            
            schedule.forEach((item, index) => {
                const div = document.createElement('div');
                div.className = 'schedule-item';
                div.innerHTML = `
                    <input type="number" min="0" max="23" value="${item.hour}" 
                           onchange="updateScheduleItem(${index}, 'hour', this.value)" placeholder="HH">
                    <span>:</span>
                    <input type="number" min="0" max="59" value="${item.minute}" 
                           onchange="updateScheduleItem(${index}, 'minute', this.value)" placeholder="MM">
                    <button class="btn btn-danger btn-small" onclick="removeScheduleItem(${index})">✕</button>
                `;
                container.appendChild(div);
            });
        }

        function addScheduleItem() {
            schedule.push({ hour: 12, minute: 0 });
            renderSchedule();
        }

        function removeScheduleItem(index) {
            schedule.splice(index, 1);
            renderSchedule();
        }

        function updateScheduleItem(index, field, value) {
            schedule[index][field] = parseInt(value);
            renderSchedule(); // Re-render to maintain sorted order
        }

        function capturePreview() {
            const container = document.getElementById('previewContainer');
            container.innerHTML = '<p>Capturing...</p>';
            
            fetch('/preview')
                .then(r => r.blob())
                .then(blob => {
                    const url = URL.createObjectURL(blob);
                    container.innerHTML = `<img src="${url}" alt="Preview">`;
                    showMessage('Image captured!');
                })
                .catch(err => {
                    container.innerHTML = '';
                    showMessage('Capture failed: ' + err, true);
                });
        }
        
        function captureAndPush() {
            showMessage('Capturing and uploading...');
            
            fetch('/capture')
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage('✓ ' + data.message);
                    } else {
                        showMessage('✗ ' + data.message, true);
                    }
                })
                .catch(err => {
                    showMessage('✗ Request failed: ' + err, true);
                });
        }

        // Initialize
        loadConfig();
        updateCountdown();
        setInterval(updateCountdown, 1000);
    </script>
</body>
</html>
)=====";
    
    return html;
}

void WebConfigServer::logRequest(AsyncWebServerRequest* request) {
    Serial.printf("HTTP %s %s from %s\n", 
        request->methodToString(), 
        request->url().c_str(),
        request->client()->remoteIP().toString().c_str());
}

bool WebConfigServer::isTimeoutExpired() {
    return (millis() - lastActivityMillis) >= timeoutMillis;
}

void WebConfigServer::resetActivityTimer() {
    lastActivityMillis = millis();
}

int WebConfigServer::getRemainingSeconds() {
    unsigned long elapsed = millis() - lastActivityMillis;
    if (elapsed >= timeoutMillis) {
        return 0;
    }
    return (timeoutMillis - elapsed) / 1000;
}

String WebConfigServer::getIpAddress() {
    return WiFi.localIP().toString();
}

void WebConfigServer::setCameraReady(bool initialized) {
    cameraReady = initialized;
}

void WebConfigServer::setCaptureCallback(CaptureCallback callback) {
    captureCallback = callback;
}

void WebConfigServer::setApMode(bool apMode) {
    isApMode = apMode;
}

bool WebConfigServer::checkAuthentication(AsyncWebServerRequest* request) {
    // If no password is set, allow access
    const char* configPassword = configManager->getWebPassword();
    if (strlen(configPassword) == 0) {
        return true;
    }
    
    // Check for Authorization header
    if (!request->hasHeader("Authorization")) {
        return false;
    }
    
    String authHeader = request->header("Authorization");
    
    // Check if it's Basic auth
    if (!authHeader.startsWith("Basic ")) {
        return false;
    }
    
    // Extract base64 encoded credentials
    String encoded = authHeader.substring(6);
    
    // Build expected credentials and encode
    String expectedCreds = String(configManager->getWebUsername()) + ":" + String(configPassword);
    String expectedEncoded = base64::encode(expectedCreds);
    
    return (encoded == expectedEncoded);
}
