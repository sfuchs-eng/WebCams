<?php
/**
 * Logging Library
 * Provides centralized logging with automatic daily rotation and retention management
 */

// Log levels
define('LOG_LEVEL_DEBUG', 'DEBUG');
define('LOG_LEVEL_INFO', 'INFO');
define('LOG_LEVEL_WARN', 'WARN');
define('LOG_LEVEL_ERROR', 'ERROR');

// Default retention days for logs
define('LOG_RETENTION_DAYS', 30);

/**
 * Get logs directory path
 * @return string
 */
function getLogsDir() {
    return __DIR__ . '/../logs';
}

/**
 * Ensure logs directory exists
 */
function ensureLogsDir() {
    $dir = getLogsDir();
    if (!is_dir($dir)) {
        mkdir($dir, 0755, true);
    }
}

/**
 * Get current date for log rotation
 * @return string YYYY-MM-DD format
 */
function getCurrentLogDate() {
    return date('Y-m-d');
}

/**
 * Check if log file needs rotation based on embedded date
 * @param string $logPath Full path to log file
 * @return bool True if rotation needed
 */
function needsLogRotation($logPath) {
    if (!file_exists($logPath)) {
        return false;
    }
    
    // Get file modification date
    $fileDate = date('Y-m-d', filemtime($logPath));
    $currentDate = getCurrentLogDate();
    
    return $fileDate !== $currentDate;
}

/**
 * Rotate log file by appending date and creating new file
 * @param string $logPath Full path to log file
 * @return bool True on success
 */
function rotateLogFile($logPath) {
    if (!file_exists($logPath)) {
        return true; // Nothing to rotate
    }
    
    // Get yesterday's date (file was last modified yesterday)
    $fileDate = date('Y-m-d', filemtime($logPath));
    
    // Create rotated filename
    $dir = dirname($logPath);
    $basename = basename($logPath, '.log');
    $rotatedPath = $dir . '/' . $basename . '_' . $fileDate . '.log';
    
    // If rotated file already exists, append timestamp
    if (file_exists($rotatedPath)) {
        $rotatedPath = $dir . '/' . $basename . '_' . $fileDate . '_' . time() . '.log';
    }
    
    // Rename current log to rotated name
    if (!rename($logPath, $rotatedPath)) {
        error_log("[Logging] Failed to rotate log: $logPath");
        return false;
    }
    
    return true;
}

/**
 * Clean up old log files beyond retention period
 * @param string $pattern Glob pattern for log files (e.g., "camera_*_*.log")
 * @param int $retentionDays Number of days to keep
 */
function cleanupOldLogs($pattern, $retentionDays = LOG_RETENTION_DAYS) {
    $logsDir = getLogsDir();
    $cutoffTime = time() - ($retentionDays * 24 * 60 * 60);
    $deleted = 0;
    
    $files = glob($logsDir . '/' . $pattern);
    foreach ($files as $file) {
        if (is_file($file) && filemtime($file) < $cutoffTime) {
            if (unlink($file)) {
                $deleted++;
            }
        }
    }
    
    if ($deleted > 0) {
        error_log("[Logging] Cleaned up $deleted old log files (pattern: $pattern, retention: $retentionDays days)");
    }
    
    return $deleted;
}

/**
 * Format log entry with standard structure
 * @param string $level Log level (DEBUG, INFO, WARN, ERROR)
 * @param string $component Component name (e.g., "OTA", "Upload", "WiFi")
 * @param string $message Log message
 * @param array $context Additional context data (will be JSON encoded)
 * @return string Formatted log entry
 */
function formatLogEntry($level, $component, $message, $context = []) {
    $timestamp = date('Y-m-d H:i:s');
    $entry = "[$timestamp] [$level] [$component] $message";
    
    if (!empty($context)) {
        $entry .= ' ' . json_encode($context, JSON_UNESCAPED_SLASHES);
    }
    
    return $entry . "\n";
}

/**
 * Write to server log file with automatic rotation
 * @param string $filename Log filename (e.g., "ota.log", "upload.log")
 * @param string $level Log level
 * @param string $component Component name
 * @param string $message Log message
 * @param array $context Additional context data
 * @return bool True on success
 */
function writeServerLog($filename, $level, $component, $message, $context = []) {
    ensureLogsDir();
    $logPath = getLogsDir() . '/' . $filename;
    
    // Check if rotation needed before writing
    if (needsLogRotation($logPath)) {
        rotateLogFile($logPath);
    }
    
    // Format and write entry
    $entry = formatLogEntry($level, $component, $message, $context);
    $result = file_put_contents($logPath, $entry, FILE_APPEND | LOCK_EX);
    
    if ($result === false) {
        error_log("[Logging] Failed to write to log: $logPath");
        return false;
    }
    
    return true;
}

/**
 * Write to camera-specific log file with automatic rotation
 * Used by camera devices to log remotely via API
 * @param string $deviceId Camera device identifier
 * @param string $level Log level
 * @param string $component Component name
 * @param string $message Log message
 * @param array $context Additional context data
 * @return bool True on success
 */
function writeCameraLog($deviceId, $level, $component, $message, $context = []) {
    ensureLogsDir();
    
    // Sanitize device ID for filename (same logic as storage.php)
    $safeDeviceId = preg_replace('/[^a-zA-Z0-9_-]/', '_', $deviceId);
    
    // Camera logs include date in filename for automatic daily separation
    $currentDate = getCurrentLogDate();
    $filename = "camera_{$safeDeviceId}_{$currentDate}.log";
    $logPath = getLogsDir() . '/' . $filename;
    
    // Format and write entry
    $entry = formatLogEntry($level, $component, $message, $context);
    $result = file_put_contents($logPath, $entry, FILE_APPEND | LOCK_EX);
    
    if ($result === false) {
        error_log("[Logging] Failed to write camera log: $logPath");
        return false;
    }
    
    // Periodically clean up old camera logs (check 1% of the time to avoid overhead)
    if (rand(1, 100) === 1) {
        cleanupOldLogs("camera_*_*.log", LOG_RETENTION_DAYS);
    }
    
    return true;
}

/**
 * Convenience function: Log OTA-related events
 * @param string $message Log message
 * @param array $context Additional context
 * @param string $level Log level (default: INFO)
 */
function logOta($message, $context = [], $level = LOG_LEVEL_INFO) {
    return writeServerLog('ota.log', $level, 'OTA', $message, $context);
}

/**
 * Convenience function: Log upload-related events
 * @param string $message Log message
 * @param array $context Additional context
 * @param string $level Log level (default: INFO)
 */
function logUpload($message, $context = [], $level = LOG_LEVEL_INFO) {
    return writeServerLog('upload.log', $level, 'Upload', $message, $context);
}

/**
 * Get recent log entries from a file
 * @param string $filename Log filename
 * @param int $lines Number of lines to retrieve
 * @return array Array of log lines
 */
function getRecentLogEntries($filename, $lines = 100) {
    $logPath = getLogsDir() . '/' . $filename;
    
    if (!file_exists($logPath)) {
        return [];
    }
    
    // Use tail-like approach for efficiency
    $file = new SplFileObject($logPath, 'r');
    $file->seek(PHP_INT_MAX);
    $totalLines = $file->key();
    
    $startLine = max(0, $totalLines - $lines);
    $entries = [];
    
    $file->seek($startLine);
    while (!$file->eof()) {
        $line = trim($file->fgets());
        if (!empty($line)) {
            $entries[] = $line;
        }
    }
    
    return $entries;
}

/**
 * Parse log entry into structured data
 * @param string $entry Log entry line
 * @return array|null Parsed data or null if invalid format
 */
function parseLogEntry($entry) {
    // Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] [component] message {json}
    $pattern = '/^\[([^\]]+)\] \[([^\]]+)\] \[([^\]]+)\] (.+)$/';
    
    if (!preg_match($pattern, $entry, $matches)) {
        return null;
    }
    
    $timestamp = $matches[1];
    $level = $matches[2];
    $component = $matches[3];
    $messageAndContext = $matches[4];
    
    // Try to extract JSON context from end of message
    $context = [];
    if (preg_match('/^(.+?) (\{.+\})$/', $messageAndContext, $contextMatches)) {
        $message = $contextMatches[1];
        $context = json_decode($contextMatches[2], true) ?? [];
    } else {
        $message = $messageAndContext;
    }
    
    return [
        'timestamp' => $timestamp,
        'level' => $level,
        'component' => $component,
        'message' => $message,
        'context' => $context
    ];
}
