#include "RemoteLogger.h"

// Static member initialization
String RemoteLogger::_serverUrl = "";
String RemoteLogger::_authToken = "";
String RemoteLogger::_deviceId = "";
bool RemoteLogger::_enabled = true;
std::vector<RemoteLogger::LogEntry> RemoteLogger::_buffer;

void RemoteLogger::begin(const String& serverUrl, const String& authToken, const String& deviceId) {
    _serverUrl = serverUrl;
    _authToken = authToken;
    _deviceId = deviceId;
    _enabled = true;
    _buffer.clear();
    _buffer.reserve(_maxBufferSize);
    
    Serial.println("[RemoteLogger] Initialized");
}

void RemoteLogger::debug(const String& component, const String& message, JsonObject context) {
    log("DEBUG", component, message, context);
}

void RemoteLogger::debug(const String& component, const String& message) {
    DynamicJsonDocument doc(1);
    log("DEBUG", component, message, doc.to<JsonObject>());
}

void RemoteLogger::info(const String& component, const String& message, JsonObject context) {
    log("INFO", component, message, context);
}

void RemoteLogger::info(const String& component, const String& message) {
    DynamicJsonDocument doc(1);
    log("INFO", component, message, doc.to<JsonObject>());
}

void RemoteLogger::warn(const String& component, const String& message, JsonObject context) {
    log("WARN", component, message, context);
}

void RemoteLogger::warn(const String& component, const String& message) {
    DynamicJsonDocument doc(1);
    log("WARN", component, message, doc.to<JsonObject>());
}

void RemoteLogger::error(const String& component, const String& message, JsonObject context) {
    log("ERROR", component, message, context);
}

void RemoteLogger::error(const String& component, const String& message) {
    DynamicJsonDocument doc(1);
    log("ERROR", component, message, doc.to<JsonObject>());
}

void RemoteLogger::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled && !_buffer.empty()) {
        // Flush remaining logs before disabling
        flush();
    }
}

bool RemoteLogger::isEnabled() {
    return _enabled;
}

void RemoteLogger::log(const String& level, const String& component, const String& message, JsonObject context) {
    // Always log to Serial for immediate feedback
    Serial.printf("[%s] [%s] %s\n", level.c_str(), component.c_str(), message.c_str());
    if (context && context.size() > 0) {
        String contextStr;
        serializeJson(context, contextStr);
        Serial.printf("  Context: %s\n", contextStr.c_str());
    }
    
    // Skip remote logging if disabled or not initialized
    if (!_enabled || _serverUrl.isEmpty()) {
        return;
    }
    
    // Skip if WiFi not connected (fail silently)
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    // Protect against memory allocation failures
    if (_buffer.size() >= _maxBufferSize * 3) {
        // Buffer too full, drop oldest entries silently
        _buffer.erase(_buffer.begin(), _buffer.begin() + _maxBufferSize);
    }
    
    // Create log entry
    LogEntry entry;
    entry.level = level;
    entry.component = component;
    entry.message = message;
    
    // Serialize context if provided (with error handling)
    if (context && context.size() > 0) {
        serializeJson(context, entry.contextJson);
    } else {
        entry.contextJson = "{}";
    }
    
    // Add to buffer (fail silently if push_back fails)
    try {
        _buffer.push_back(entry);
    } catch (...) {
        Serial.println("[RemoteLogger] Buffer push failed (silent)");
        return;
    }
    
    // Flush if buffer full (non-blocking, failures ignored)
    if (_buffer.size() >= _maxBufferSize) {
        flush();
    }
}

bool RemoteLogger::flush() {
    if (_buffer.empty()) {
        return true;
    }
    
    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[RemoteLogger] WiFi not connected, clearing buffer");
        _buffer.clear();
        return false;
    }
    
    if (!_enabled || _serverUrl.isEmpty() || _authToken.isEmpty() || _deviceId.isEmpty()) {
        Serial.println("[RemoteLogger] Cannot flush - not configured");
        _buffer.clear();
        return false;
    }
    
    bool success = sendLogs();
    
    if (success) {
        _buffer.clear();
    } else {
        // Keep buffer for retry, but limit size to prevent memory issues
        if (_buffer.size() > _maxBufferSize * 2) {
            // Remove oldest entries
            _buffer.erase(_buffer.begin(), _buffer.begin() + _maxBufferSize);
            Serial.println("[RemoteLogger] Buffer pruned after send failure");
        }
    }
    
    return success;
}

bool RemoteLogger::sendLogs() {
    if (_buffer.empty()) {
        return true;
    }
    
    // Double-check WiFi before attempting network operations
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[RemoteLogger] WiFi lost during send");
        return false;
    }
    
    // Wrap entire send operation in error handling
    WiFiClientSecure* client = nullptr;
    HTTPClient* http = nullptr;
    
    try {
        // Build JSON payload with memory check
        DynamicJsonDocument doc(4096);
        JsonArray entries = doc.createNestedArray("entries");
        
        for (const LogEntry& entry : _buffer) {
            JsonObject logObj = entries.createNestedObject();
            logObj["level"] = entry.level;
            logObj["component"] = entry.component;
            logObj["message"] = entry.message;
            
            // Parse context JSON string back to object
            if (!entry.contextJson.isEmpty() && entry.contextJson != "{}") {
                DynamicJsonDocument contextDoc(512);
                DeserializationError error = deserializeJson(contextDoc, entry.contextJson);
                if (!error) {
                    logObj["context"] = contextDoc.as<JsonObject>();
                }
            }
        }
        
        String payload;
        serializeJson(doc, payload);
        
        // Create client and HTTP on heap to ensure cleanup
        client = new WiFiClientSecure();
        if (!client) {
            Serial.println("[RemoteLogger] Failed to allocate client");
            return false;
        }
        
        client->setInsecure(); // TODO: Add certificate validation in production
        
        http = new HTTPClient();
        if (!http) {
            Serial.println("[RemoteLogger] Failed to allocate HTTP client");
            delete client;
            return false;
        }
        
        // Build URL
        String url = _serverUrl;
        if (!url.endsWith("/")) {
            url += "/";
        }
        url += "log.php";
        
        if (!http->begin(*client, url)) {
            Serial.println("[RemoteLogger] Failed to begin HTTP request");
            delete http;
            delete client;
            return false;
        }
        
        http->addHeader("Content-Type", "application/json");
        http->addHeader("X-Auth-Token", _authToken);
        http->addHeader("X-Device-ID", _deviceId);
        http->setTimeout(3000); // 3 second timeout (reduced from 5s)
        
        int httpCode = http->POST(payload);
        
        bool success = false;
        if (httpCode >= 200 && httpCode < 300) {
            Serial.printf("[RemoteLogger] Logs sent successfully (%d entries)\n", _buffer.size());
            success = true;
        } else {
            Serial.printf("[RemoteLogger] Failed to send logs: HTTP %d\n", httpCode);
            // Don't retrieve response body on failure to save time/memory
        }
        
        // Cleanup
        http->end();
        delete http;
        delete client;
        
        return success;
        
    } catch (...) {
        // Catch any exceptions and clean up
        Serial.println("[RemoteLogger] Exception during send (silent)");
        if (http) {
            http->end();
            delete http;
        }
        if (client) {
            delete client;
        }
        return false;
    }
}
