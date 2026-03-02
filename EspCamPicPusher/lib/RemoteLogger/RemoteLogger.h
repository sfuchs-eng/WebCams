#ifndef REMOTE_LOGGER_H
#define REMOTE_LOGGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/**
 * RemoteLogger - Fail-safe asynchronous remote logging to server API
 * 
 * Provides debug(), info(), warn(), error() methods for logging to server.
 * Logs are batched and sent asynchronously to minimize performance impact.
 * Falls back to Serial logging if server unreachable.
 * 
 * FAIL-SAFE DESIGN:
 * - All network failures are caught and logged to Serial only
 * - WiFi connection checked before network operations
 * - Memory allocation failures handled gracefully
 * - Timeouts prevent blocking (3s max)
 * - Buffer auto-prunes to prevent memory issues
 * - Never throws exceptions or disrupts main functionality
 */
class RemoteLogger {
public:
    /**
     * Initialize remote logger
     * @param serverUrl Base server URL (e.g., "https://example.com/webcams")
     * @param authToken Authentication token
     * @param deviceId Device identifier (MAC address)
     */
    static void begin(const String& serverUrl, const String& authToken, const String& deviceId);
    
    /**
     * Log DEBUG level message
     * @param component Component name (e.g., "OTA", "WiFi", "Camera")
     * @param message Log message
     * @param context Optional JSON context object (default: empty)
     */
    static void debug(const String& component, const String& message, JsonObject context);
    static void debug(const String& component, const String& message);
    
    /**
     * Log INFO level message
     * @param component Component name
     * @param message Log message
     * @param context Optional JSON context object (default: empty)
     */
    static void info(const String& component, const String& message, JsonObject context);
    static void info(const String& component, const String& message);
    
    /**
     * Log WARN level message
     * @param component Component name
     * @param message Log message
     * @param context Optional JSON context object (default: empty)
     */
    static void warn(const String& component, const String& message, JsonObject context);
    static void warn(const String& component, const String& message);
    
    /**
     * Log ERROR level message
     * @param component Component name
     * @param message Log message
     * @param context Optional JSON context object (default: empty)
     */
    static void error(const String& component, const String& message, JsonObject context);
    static void error(const String& component, const String& message);
    
    /**
     * Flush pending logs immediately (blocking)
     * Automatically called by log methods when buffer is full
     * @return true if flush successful
     */
    static bool flush();
    
    /**
     * Enable or disable remote logging
     * When disabled, only Serial logging occurs
     * @param enabled Enable flag
     */
    static void setEnabled(bool enabled);
    
    /**
     * Check if remote logging is enabled
     */
    static bool isEnabled();

private:
    struct LogEntry {
        String level;
        String component;
        String message;
        String contextJson;
    };
    
    static String _serverUrl;
    static String _authToken;
    static String _deviceId;
    static bool _enabled;
    static std::vector<LogEntry> _buffer;
    static const size_t _maxBufferSize = 10;
    
    /**
     * Add log entry to buffer and flush if full
     */
    static void log(const String& level, const String& component, const String& message, JsonObject context);
    
    /**
     * Send buffered logs to server
     */
    static bool sendLogs();
};

#endif // REMOTE_LOGGER_H
